// #define LATCH_PIN 8
// #define CLOCK_PIN 12
// #define DATA_PIN 11
// #define BUTTON_PIN 2

#define LATCH_PIN 0
#define CLOCK_PIN 4
#define DATA_PIN 1
#define BUTTON_PIN 2

#define BUTTON_TIMEOUT 250

const char HEART[8] = {
  0b00000000,
  0b01100110,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

const char BIRD[8] = {
  0b01110000,
  0b11010000,
  0b01110000,
  0b01111000,
  0b01111111,
  0b01111110,
  0b01111100,
  0b00100000
};

const char SPEEDS[4] = { 40, 50, 60, 70 };

enum Direction {
  RIGHT,
  LEFT,
};

enum State {
  PLAYING,
  PAUSED,
};

// State
State currentState;
char PIXELS[8];

char currentRow;
char currentCol;
char size;
Direction direction;

bool isButtonPressed;

unsigned long buttonTime;
unsigned long lastUpdateTime;

void setup() {
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPressed, RISING);

  isButtonPressed = false;

  buttonTime = millis();
  lastUpdateTime = millis();

  setPlaying();
}

void setPlaying() {
  currentState = PLAYING;
  memset(PIXELS, 0, 8);
  currentRow = 7;
  currentCol = 0;
  size = 3;
  direction = RIGHT;
}

void setPaused(char pixels[8]) {
  currentState = PAUSED;
  memcpy(PIXELS, pixels, 8 * sizeof(char));
}

void buttonPressed() {
  if (millis() - buttonTime < BUTTON_TIMEOUT) return;
  buttonTime = millis();
  isButtonPressed = true;
}

void loop() {
  updatePixels();
  displayPixels();
}

/*
 * Update PIXELS without updating the display.
 */
void updatePixels() {
  if (millis() - lastUpdateTime < SPEEDS[currentRow / 2]) return;

  switch (currentState) {
    case PLAYING:
      updatePlaying();
      break;
    case PAUSED:
      updatePause();
      break;
  }

  lastUpdateTime = millis();
}

void updatePlaying() {
  if (isButtonPressed) {
    isButtonPressed = false;

    int firstOverlappingColumn = -1;
    int numOverlapping = 0;

    if (currentRow == 7) {
      firstOverlappingColumn = currentCol;
      numOverlapping = size;
    } else {
      for (int col = 0; col < 8; col++) {
        bool overlapping = PIXELS[currentRow + 1] >> (7 - col) & 1 == 1
                           && col >= currentCol
                           && col < currentCol + size;

        if (overlapping) {
          if (firstOverlappingColumn < 0) {
            firstOverlappingColumn = col;
          }

          numOverlapping++;
        }
      }
    }

    if (numOverlapping == 0) {
      setPaused(BIRD);
    } else if (currentRow == 0 && numOverlapping > 0) {
      setPaused(HEART);
    } else {
      if (firstOverlappingColumn > 0) {
        currentCol = firstOverlappingColumn;
      }

      size = numOverlapping;

      drawCursorRow();

      currentRow--;
    }
  } else {
    bool swapDirection = (direction == RIGHT && currentCol + size > 7)
                         || (direction == LEFT && currentCol == 0);

    if (swapDirection) {
      direction = direction == RIGHT ? LEFT : RIGHT;
    }

    currentCol += direction == RIGHT ? 1 : -1;

    drawCursorRow();
  }
}

void drawCursorRow() {
  PIXELS[currentRow] = 0;

  for (int col = 0; col < 8; col++) {
    PIXELS[currentRow] |= col >= currentCol && col < currentCol + size ? 1 << (7 - col) : 0;
  }
}

void updatePause() {
  if (isButtonPressed) {
    isButtonPressed = false;
    setPlaying();
  }
}

struct Mask {
  char left;
  char right;
};

struct Mask rowMask(int row) {
  switch (row) {
    case 0: return { 0b00000000, 0b00000001 };
    case 1: return { 0b00100000, 0b00000000 };
    case 2: return { 0b00000000, 0b00010000 };
    case 3: return { 0b00000000, 0b00001000 };
    case 4: return { 0b00001000, 0b00000000 };
    case 5: return { 0b00000000, 0b00100000 };
    case 6: return { 0b00000100, 0b00000000 };
    case 7: return { 0b00000000, 0b10000000 };
    default: return { 0b00000000, 0b00000000 };
  }
}

struct Mask colMask(int col) {
  switch (col) {
    case 0: return { 0b00010000, 0b00000000 };
    case 1: return { 0b00000010, 0b00000000 };
    case 2: return { 0b00000001, 0b00000000 };
    case 3: return { 0b00000000, 0b00000010 };
    case 4: return { 0b00000000, 0b01000000 };
    case 5: return { 0b00000000, 0b00000100 };
    case 6: return { 0b01000000, 0b00000000 };
    case 7: return { 0b10000000, 0b00000000 };
    default: return { 0b00000000, 0b00000000 };
  }
}

const struct Mask FULL_COL_MASK = { 0b11010011, 0b01000110 };

/*
 * Send two bytes of data to the shift registers.
 */
void writeBits(char left, char right) {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, left);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, right);
  digitalWrite(LATCH_PIN, HIGH);
}

/*
 * Update the display with the stored data in PIXELS.
 */
void displayPixels() {
  for (int row = 0; row < 8; row++) {
    struct Mask row_mask = rowMask(row);

    row_mask.left |= FULL_COL_MASK.left;
    row_mask.right |= FULL_COL_MASK.right;

    for (int col = 0; col < 8; col++) {
      struct Mask col_mask = colMask(col);

      if (PIXELS[row] >> (7 - col) & 1 == 1) {
        row_mask.left ^= col_mask.left;
        row_mask.right ^= col_mask.right;
      }
    }

    writeBits(row_mask.left, row_mask.right);
  }
}

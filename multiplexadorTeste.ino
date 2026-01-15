#include <FastLED.h>
#include <CD74HC4067.h>

// ============================================================
// 1) WS2812B 8x8 (64 LEDs)
// ============================================================
#define LED_PIN       10
#define NUM_LEDS      64
#define BRIGHTNESS    15
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB

CRGB leds[NUM_LEDS];

const bool SERPENTINE_LAYOUT = false;

const int W = 8;
const int H = 8;
const int N = W * H;

// ============================================================
// 2) 4x CD74HC4067
// ============================================================
const int PIN_S0 = 2;
const int PIN_S1 = 3;
const int PIN_S2 = 4;
const int PIN_S3 = 5;

const int SIG_PINS[4] = { A0, A1, A2, A3 };

CD74HC4067 mux(PIN_S0, PIN_S1, PIN_S2, PIN_S3);
const int CHANNEL_SETTLE_US = 200;

// Lock: 1 toque = 1 ação, só volta a aceitar depois de soltar
bool inputLocked = false;
unsigned long lockStartMs = 0;
const unsigned long MIN_LOCK_MS = 120;
int lockedIdx = -1;

// ============================================================
// 3) Minesweeper config
// ============================================================
const int MINES = 10;

bool mines[N];
bool revealed[N];
uint8_t neighborCount[N];
bool lastPressed[N];

// ============================================================
// 4) Estado do jogo + efeitos fim
// ============================================================
enum GameState { PLAYING, LOST, WON };
GameState state = PLAYING;

unsigned long endStartMs = 0;
int lastBlinkPhase = -1;

const unsigned long END_DURATION_MS = 5000;
const unsigned long BLINK_PERIOD_MS = 250;

// ============================================================
// 5) Mapeamentos
// ============================================================
int cellIndexFromXY(int x, int y) { return y * W + x; }

bool inBounds(int x, int y) {
  return (x >= 0 && x < W && y >= 0 && y < H);
}

int ledIndexFromXY(int x, int y) {
  if (!SERPENTINE_LAYOUT) return y * W + x;
  if ((y % 2) == 0) return y * W + x;
  return y * W + (W - 1 - x);
}

void setCellColor(int x, int y, const CRGB& c) {
  leds[ledIndexFromXY(x, y)] = c;
}

void clearAllLeds() {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
  FastLED.show();
}

void fillAll(const CRGB& c) {
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = c;
  FastLED.show();
}

// ============================================================
// 6) Cores Minesweeper
// ============================================================
CRGB colorForCount(uint8_t c) {
  switch (c) {
    case 0: return CRGB(60, 60, 60);
    case 1: return CRGB::Blue;
    case 2: return CRGB::Green;
    case 3: return CRGB::Red;
    case 4: return CRGB(0, 0, 160);
    case 5: return CRGB(160, 0, 0);
    case 6: return CRGB::Cyan;
    case 7: return CRGB(255, 0, 255);
    case 8: return CRGB(255, 140, 0);
    default: return CRGB(60, 60, 60);
  }
}

// ============================================================
// 7) Contagem de minas vizinhas
// ============================================================
uint8_t countNeighbors(int x, int y) {
  uint8_t c = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx, ny = y + dy;
      if (!inBounds(nx, ny)) continue;
      if (mines[cellIndexFromXY(nx, ny)]) c++;
    }
  }
  return c;
}

// ============================================================
// 8) Leitura via 4 mux (estável simples + flush)
// ============================================================
bool readCellPressedStable(int idx) {
  const int muxId = idx / 16;
  const int ch    = idx % 16;

  mux.channel(ch);
  delayMicroseconds(CHANNEL_SETTLE_US);

  // flush (reduz leituras "lixo" ao mudar canal)
  (void)digitalRead(SIG_PINS[muxId]);
  delayMicroseconds(200);

  // filtro leve (não agressivo)
  const int SAMPLES = 5;
  const int NEED_LOW = 4;
  int lowCount = 0;

  for (int i = 0; i < SAMPLES; i++) {
    if (digitalRead(SIG_PINS[muxId]) == LOW) lowCount++;
    delayMicroseconds(150);
  }

  return (lowCount >= NEED_LOW);
}

// Debug: imprime mux/canal
void printPressInfo(int idx) {
  const int muxId = idx / 16;
  const int ch    = idx % 16;
  const int x     = idx % W;
  const int y     = idx / W;

  Serial.print("TOQUE -> idx=");
  Serial.print(idx);
  Serial.print(" (x=");
  Serial.print(x);
  Serial.print(", y=");
  Serial.print(y);
  Serial.print(") | MUX=");
  Serial.print(muxId);
  Serial.print(" | CANAL=");
  Serial.println(ch);
}

// “Antigo”: varre e pára no 1º novo toque
int scanForNewPress() {
  for (int i = 0; i < N; i++) {
    bool pressed  = readCellPressedStable(i);
    bool newPress = (!lastPressed[i] && pressed);
    lastPressed[i] = pressed;

    if (newPress) {
      printPressInfo(i);
      return i;
    }
  }
  return -1;
}

// ============================================================
// 9) Vitória / Fim de jogo
// ============================================================
bool checkWin() {
  int revealedSafe = 0;
  for (int i = 0; i < N; i++) {
    if (!mines[i] && revealed[i]) revealedSafe++;
  }
  return (revealedSafe == (N - MINES));
}

void beginEnd(GameState endState) {
  state = endState;
  endStartMs = millis();
  lastBlinkPhase = -1;

  if (endState == WON)  Serial.println("FIM: GANHOU!");
  if (endState == LOST) Serial.println("FIM: PERDEU!");
}

void updateEndBlink() {
  unsigned long elapsed = millis() - endStartMs;

  int phase = (elapsed / BLINK_PERIOD_MS) % 2;
  if (phase == lastBlinkPhase) return;
  lastBlinkPhase = phase;

  if (phase == 0) fillAll(CRGB::Black);
  else fillAll(state == WON ? CRGB::Green : CRGB::Red);
}

// ============================================================
// 10) Reveal + Flood fill
// ============================================================
void revealCell(int startIdx) {
  if (startIdx < 0 || startIdx >= N) return;
  if (revealed[startIdx]) return;

  if (mines[startIdx]) {
    beginEnd(LOST);
    return;
  }

  int stack[N];
  int sp = 0;
  stack[sp++] = startIdx;

  while (sp > 0) {
    int idx = stack[--sp];
    if (revealed[idx]) continue;

    revealed[idx] = true;

    int x = idx % W;
    int y = idx / W;

    uint8_t c = neighborCount[idx];
    setCellColor(x, y, colorForCount(c));

    if (c != 0) continue;

    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;

        int nx = x + dx, ny = y + dy;
        if (!inBounds(nx, ny)) continue;

        int nIdx = cellIndexFromXY(nx, ny);
        if (revealed[nIdx]) continue;
        if (mines[nIdx]) continue;

        stack[sp++] = nIdx;
      }
    }
  }

  FastLED.show();

  if (checkWin()) beginEnd(WON);
}

// ============================================================
// 11) Novo jogo
// ============================================================
void newGame() {
  state = PLAYING;

  inputLocked = false;
  lockStartMs = 0;
  lockedIdx = -1;

  for (int i = 0; i < N; i++) {
    mines[i] = false;
    revealed[i] = false;
    neighborCount[i] = 0;
    lastPressed[i] = false;
  }

  clearAllLeds();

  randomSeed(analogRead(A4) + micros());

  int placed = 0;
  while (placed < MINES) {
    int r = random(N);
    if (!mines[r]) {
      mines[r] = true;
      placed++;
    }
  }

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int idx = cellIndexFromXY(x, y);
      neighborCount[idx] = countNeighbors(x, y);
    }
  }

  Serial.println("NOVO JOGO!");
}

// ============================================================
// 12) Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Minesweeper 8x8 (4 mux) - 1 toque = 1 leitura.");

  for (int i = 0; i < 4; i++) {
    pinMode(SIG_PINS[i], INPUT_PULLUP);
  }

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  newGame();
}

void loop() {
  // Fim de jogo: pisca 5s e reinicia
  if (state != PLAYING) {
    updateEndBlink();
    if (millis() - endStartMs >= END_DURATION_MS) {
      newGame();
      delay(50);
    }
    return;
  }

  // Lock: só desbloqueia quando o MESMO contacto for solto
  if (inputLocked) {
    if (millis() - lockStartMs < MIN_LOCK_MS) return;

    if (lockedIdx >= 0 && !readCellPressedStable(lockedIdx)) {
      inputLocked = false;
      lockedIdx = -1;
    }
    return;
  }

  int pressIdx = scanForNewPress();
  if (pressIdx == -1) return;

  inputLocked = true;
  lockStartMs = millis();
  lockedIdx = pressIdx;

  revealCell(pressIdx);

  delay(30);
}
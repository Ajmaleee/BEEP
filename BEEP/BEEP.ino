/*
 * BEEP - Phase 1 firmware
 * ==================================================================
 * An expressive OLED face for an ESP32-C3 Mini + SSD1306 128x64.
 *
 *   "BEEP should look alive even when nobody interacts with it."
 *
 * Pipeline:  Animator  ->  FaceState  ->  Renderer  ->  SSD1306
 *
 * The main loop NEVER blocks: everything is timed from millis().
 * No microphone / WiFi / BLE / audio / cloud / AI in this phase.
 *
 * Requires: Adafruit SSD1306 + Adafruit GFX  (see README.md).
 * ------------------------------------------------------------------
 */
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "face_types.h"
#include "expressions.h"
#include "animator.h"
#include "renderer.h"

using namespace beep;

// ------------------------------------------------------------------
// Display
// ------------------------------------------------------------------
Adafruit_SSD1306 display(BEEP_SCREEN_W, BEEP_SCREEN_H, &Wire, -1);

// ------------------------------------------------------------------
// State
// ------------------------------------------------------------------
FaceGeometry geo;
Animator anim;

uint32_t lastFrame = 0;
char lineBuf[24];
uint8_t lineLen = 0;

static void setupGeometry() {
  geo.screenW = BEEP_SCREEN_W;
  geo.screenH = BEEP_SCREEN_H;
  geo.eyeHalfW = BEEP_EYE_HALF_W;
  geo.eyeCxOffset = BEEP_EYE_CX_OFFSET;
  geo.eyeCy = BEEP_EYE_CY;
  geo.browOffsetY = BEEP_BROW_OFFSET_Y;
  geo.browHalfW = BEEP_BROW_HALF_W;
  geo.mouthBaseY = BEEP_MOUTH_BASE_Y;
}

static AnimatorConfig makeConfig() {
  AnimatorConfig c;
  c.transitionMs = BEEP_TRANSITION_MS;
  c.blinkMinMs = BEEP_BLINK_MIN_MS;
  c.blinkMaxMs = BEEP_BLINK_MAX_MS;
  c.blinkDurationMs = BEEP_BLINK_DURATION_MS;
  c.asymBlinkChance = BEEP_BLINK_ASYMM_CHANCE;
  c.doubleBlinkChance = BEEP_BLINK_DOUBLE_CHANCE;
  c.gazeMinMs = BEEP_GAZE_MIN_MS;
  c.gazeMaxMs = BEEP_GAZE_MAX_MS;
  c.demoStepMs = BEEP_DEMO_STEP_MS;
  c.quirkChance = BEEP_QUIRK_CHANCE;
  c.gazeRadius = BEEP_GAZE_RADIUS;
  c.gazeSpeed = BEEP_GAZE_SPEED;
  c.breatheAmp = BEEP_BREATHE_AMP;
  c.breathePeriodMs = BEEP_BREATHE_PERIOD_MS;
  return c;
}

// ------------------------------------------------------------------
// Serial command parsing
// ------------------------------------------------------------------
static bool ieq(const char* a, const char* b) {
  while (*a && *b) {
    char ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += 32;
    if (cb >= 'A' && cb <= 'Z') cb += 32;
    if (ca != cb) return false;
    ++a; ++b;
  }
  return *a == *b;
}

struct CmdEntry {
  const char* name;
  uint8_t code;      // Expr, or CMD_DEMO / CMD_HELP
};

static const uint8_t CMD_DEMO = 0xFE;
static const uint8_t CMD_HELP = 0xFD;

static const CmdEntry kCmds[] = {
  {"idle", EXPR_IDLE},
  {"happy", EXPR_HAPPY},
  {"excited", EXPR_EXCITED},
  {"curious", EXPR_CURIOUS},
  {"surprised", EXPR_SURPRISED},
  {"sleepy", EXPR_SLEEPY},
  {"sleep", EXPR_SLEEPING},
  {"sad", EXPR_SAD},
  {"annoyed", EXPR_ANNOYED},
  {"love", EXPR_AFFECTIONATE},
  {"think", EXPR_THINKING},
  {"listen", EXPR_LISTENING},
  {"confused", EXPR_CONFUSED},
  {"demo", CMD_DEMO},
  {"help", CMD_HELP},
  {"?", CMD_HELP},
};
static const uint8_t kCmdCount = sizeof(kCmds) / sizeof(kCmds[0]);

static void printHelp() {
  Serial.println(F("BEEP commands:"));
  Serial.println(F("  idle happy excited curious surprised sleepy"));
  Serial.println(F("  sleep sad annoyed love think listen confused"));
  Serial.println(F("  demo   (cycle all automatically)"));
}

static void handleCommand(const char* raw) {
  // trim both ends
  while (*raw == ' ') ++raw;
  char cmd[24];
  uint8_t n = 0;
  while (raw[n] && n < sizeof(cmd) - 1) { cmd[n] = raw[n]; ++n; }
  while (n > 0 && cmd[n - 1] == ' ') --n;
  cmd[n] = '\0';
  if (n == 0) return;

  for (uint8_t i = 0; i < kCmdCount; ++i) {
    if (!ieq(cmd, kCmds[i].name)) continue;
    if (kCmds[i].code == CMD_HELP) { printHelp(); return; }
    if (kCmds[i].code == CMD_DEMO) {
      anim.startDemo(millis());
      Serial.println(F("BEEP: demo mode"));
      return;
    }
    Expr e = (Expr)kCmds[i].code;
    anim.setExpr(e, millis());
    Serial.print(F("BEEP: "));
    Serial.println(expressionName(e));
    return;
  }

  Serial.print(F("BEEP: unknown '"));
  Serial.print(cmd);
  Serial.println(F("' (try help)"));
}

// ------------------------------------------------------------------
// Arduino entry points
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  Wire.begin(BEEP_I2C_SDA, BEEP_I2C_SCL, BEEP_I2C_HZ);
  Wire.setClock(BEEP_I2C_HZ);

  if (!display.begin(SSD1306_SWITCHCAPVCC, BEEP_I2C_ADDR)) {
    Serial.println(F("BEEP: SSD1306 init FAILED (check address/pins)"));
    // keep running so the failure is visible on the serial monitor
  }
  display.clearDisplay();
  display.display();

  randomSeed(micros());

  setupGeometry();
  AnimatorConfig cfg = makeConfig();
  anim.begin(millis(), geo, cfg);

  Serial.println(F("BEEP awake. Type 'help' for commands."));
}

void loop() {
  // --- non-blocking serial input ---
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleCommand(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = ch;
    }
  }

  // --- animation ---
  uint32_t now = millis();
  anim.update(now);

  // --- throttled render ---
  if (now - lastFrame >= BEEP_FRAME_MS) {
    lastFrame = now;
    display.clearDisplay();
    drawFace(display, anim.face(), geo);
    display.display();
  }
}

# BEEP — an expressive OLED pet (Phase 1: the face)

BEEP is a tiny virtual creature for an **ESP32-C3 Mini** and a **128×64 SSD1306
OLED**. Phase 1 is *only* the face engine: a procedurally drawn, emotional,
non-blocking animated face that looks alive on its own. No microphone, Wi-Fi,
Bluetooth, audio, cloud or AI yet.

> Status: this firmware has **not** been compiled, uploaded or tested on
> hardware. No toolchain was available in the authoring environment. The design
> was verified visually via the bundled Python preview (see below). Treat every
> hardware detail as an assumption until you confirm it — see
> [Assumptions vs. verified](#assumptions-vs-verified).

---

## 1. What's in the box

```
BEEP/
  BEEP.ino          Arduino entry points: display init, serial commands, main loop
  config.h          ALL hardware pins + every tunable number (one place to change)
  face_types.h      Pure data: Expr, FaceParams, EyeState, FaceState, FaceGeometry
  expressions.h/.cpp The expression library (parameter table + timing policy)
  animator.h/.cpp   Animation engine: transitions, blinking, gaze, breathing, demo
  renderer.h/.cpp   Deterministic procedural drawing of a FaceState
preview/
  beep_preview.py   Dependency-free host preview (ASCII + PNG), mirrors the renderer
  out/              Generated PNG previews of every expression
README.md
```

Pipeline:

```
serial command ─► Animator ─► FaceState ─► Renderer ─► SSD1306 framebuffer ─► OLED
                     ▲
              millis(), random()   (all timing + randomness live here)
```

`Animator` produces a fully-resolved `FaceState`. The `Renderer` receives that
state and draws it. This separation means drawing is **reproducible**: the same
state always produces the same face, and all randomness/timing lives in one
place.

### Library choice

**Adafruit SSD1306 + Adafruit GFX** were chosen over U8g2 because:

- We draw everything ourselves with a handful of primitives (`drawPixel`,
  `drawFastVLine`, `fillRect`). Adafruit GFX exposes exactly these, simply.
- The SSD1306 driver keeps a plain 1-bit framebuffer, which is perfect for
  procedural monochrome art and cheap partial updates.
- Excellent, well-documented ESP32 support and the most common tutorial base.
- U8g2's advantages (huge font set, many controllers) are irrelevant here — we
  use no text and one controller. It would add weight without benefit.

---

## 2. The face design (why BEEP looks like BEEP)

BEEP is a two-eyed creature, not a status display. The whole face is built from
a single reusable **eye primitive**:

- Each eye is a scanline **aperture** bounded above and below by a
  *superellipse* profile. One `shape` exponent tunes the eye from a round oval
  (`2.0`) toward a rounded "screen" eye (`>2.5`).
- **Signed bulges** (`top`, `bot`) let the same primitive render an open oval
  (`+17,+15`), a heavy-lidded glare, or a happy upward crescent (both positive
  but asymmetric, e.g. `+8,-3`). This is why there is *one* eye renderer instead
  of a dozen hand-drawn sprites.
- A **signed upper-lid slant** tilts the top edge so the *inner* corner drops
  (anger) or rises (sadness).
- The **pupil** is a shape mask (a filled disc or an implicit-function heart)
  clipped to the aperture, plus a small white **glint**. Clipping is free because
  the pupil is tested against the same per-column aperture used to fill the eye.
- **Brows** and a small **mouth** are added only when they strengthen an
  emotion; `IDLE` has no mouth at all, keeping the resting face clean.

Identity choices that carry across firmware revisions:

- Big, wide-set eyes with a static glint (upper-left) — BEEP's "spark."
- A slight superellipse shape (`2.6`) so eyes are friendly, not perfect circles.
- A dark-pupil-on-white-eye look for maximum contrast on a tiny mono screen.
- The `AFFECTIONATE` state uses **heart pupils** as a deliberate, instantly
  readable signature; every other state stays abstract and un-emoji.

---

## 3. Expressions

Each expression is a row in `expressions.cpp` — a complete `FaceParams` set.
Adding a new emotion is three lines (enum value, table row, name/timing) and the
renderer never changes.

| Command      | Expression     | Look                                                       | Duration |
|--------------|----------------|------------------------------------------------------------|----------|
| `idle`       | IDLE           | Open, calm, no mouth                                       | persistent |
| `happy`      | HAPPY          | Raised lower lids, smile                                   | 2.6 s |
| `excited`    | EXCITED        | Wide eyes, big pupils, open smile                          | 3.0 s |
| `curious`    | CURIOUS        | One eye raised/bigger, one brow up, small "o"              | 2.6 s |
| `surprised`  | SURPRISED      | Very wide eyes, tiny pupils, high brows, "O" mouth         | 1.5 s |
| `sleepy`     | SLEEPY         | Heavy slanted lids, low gaze, flat mouth                   | 4.0 s |
| `sleep`      | SLEEPING       | Closed eyes, slow breathing                                | persistent |
| `sad`        | SAD            | Droopy outer lids, inner-raised brows, frown               | 4.0 s |
| `annoyed`    | ANNOYED        | Low inner-slanted lids, angry brows, frown                 | 3.2 s |
| `love`       | AFFECTIONATE   | Soft eyes, heart pupils, smile                             | 3.6 s |
| `think`      | THINKING       | Gaze up/aside, one brow raised, small flat mouth           | 4.5 s |
| `listen`     | LISTENING      | Wide attentive eyes, raised brows, tiny "o"                | persistent |
| `confused`   | CONFUSED       | One eye squinted, other wide, wavy mouth                   | 2.6 s |

Temporary expressions cross-fade back to `idle`. Persistent states
(`idle`, `sleeping`, `listening`) stay until changed. Timing lives in the
`kHold[]` table.

---

## 4. Animation architecture

All in `animator.cpp`, all non-blocking, all `millis()`-based (rollover-safe via
signed-difference comparisons):

- **Expression transitions** — `setExpr()` snapshots the current blended
  parameters into `from_`, sets `to_` to the new expression, and eases via
  smoothstep over `BEEP_TRANSITION_MS`. Because it snapshots, changing your mind
  mid-transition is continuous, never a jump.
- **Randomised blinking** — next blink is scheduled within
  `[BEEP_BLINK_MIN_MS, BEEP_BLINK_MAX_MS]`. Occasional **asymmetric** (one-eyed)
  blinks and **double** blinks are injected by probability. Per-eye start
  offsets make normal blinks feel organic. Blink lowers the aperture toward a
  closed crescent so it also works on half-lidded expressions.
- **Idle gaze** — every few seconds a new pupil target is chosen inside
  `BEEP_GAZE_RADIUS`; the pupil eases toward it at `BEEP_GAZE_SPEED` px/s. A
  "quirk" chance occasionally fires a quick follow-up dart (BEEP notices
  something). Gaze influence fades out when an expression already looks away.
- **Breathing** — a tiny sinusoidal vertical bob (`BEEP_BREATHE_AMP`) keeps the
  whole face subtly alive, most visible while sleeping.
- **Personality** — the quirks above (asymmetric blinks, double blinks, gaze
  darts) are the "small occasional animations."
- **Demo mode** — cycles every expression automatically for
  `BEEP_DEMO_STEP_MS` each.

Frame rate is throttled to `BEEP_FRAME_MS` (~30 fps). Full-frame I²C transfer of
1024 bytes at 400 kHz takes ~20 ms, so ~30 fps is the practical ceiling — ample
for these movements.

---

## 5. Memory & performance

- No dynamic allocation, no animation assets, no extra libraries.
- Fixed-size structs; the whole expression table is ~1 KB in flash.
- Procedural drawing only; work per frame is a few thousand pixel writes.
- No networking/audio/cloud dependencies.
- All timers use signed `millis()` deltas, safe across the ~49-day rollover.

---

## 6. Installing the libraries

In the Arduino IDE:

1. **Boards Manager** (`Tools ▸ Board ▸ Boards Manager`): search `esp32` by
   Espressif and install the ESP32 core. Select **ESP32C3 Dev Module** under
   `Tools ▸ Board ▸ ESP32 Arduino`.
2. **Library Manager** (`Tools ▸ Manage Libraries`): install
   **Adafruit SSD1306** and **Adafruit GFX Library**. Accept the "install all"
   prompt for **Adafruit BusIO** if offered.
3. Open `BEEP/BEEP.ino`, choose the correct COM port, and Upload.

No other dependency is required.

---

## 7. Host-side preview (before the OLED arrives)

`preview/beep_preview.py` renders the *same* geometry and expression table to
ASCII and to PNG. It needs **only the Python standard library** (no pip install,
no build system).

```bash
python preview/beep_preview.py --list            # list expressions
python preview/beep_preview.py --ascii idle      # one expression as ASCII art
python preview/beep_preview.py --all-ascii       # every expression
python preview/beep_preview.py --png             # PNGs into preview/out/
```

It mirrors the renderer's scanline eye model, signed bulges, lid slant, pupil
clipping, glints, brows and mouth shapes. **It is a design preview, not the
firmware** — if you change `expressions.cpp`, update the table in the preview to
match (the two are intentionally kept readable side by side). `preview/out/`
already contains renders of all 13 expressions.

---

## 8. Configuring for your hardware

Everything lives in `config.h`. When the module arrives:

| Setting | Default | Change if… |
|---|---|---|
| `BEEP_SCREEN_W` / `BEEP_SCREEN_H` | `128` / `64` | you have a 128×32 display |
| `BEEP_I2C_SDA` / `BEEP_I2C_SCL` | `8` / `9` | your board wires I²C elsewhere |
| `BEEP_I2C_ADDR` | `0x3C` | scan finds `0x3D` |
| `BEEP_I2C_HZ` | `400000` | module is unstable at 400 kHz → try `100000` |
| `BEEP_FRAME_MS` | `33` | you want lower power (e.g. `50`–`66`) |
| eye geometry, blink/transition/gaze timings | see file | taste |

**I²C address scan** (run once on-device if unsure):

```cpp
#include <Wire.h>
void setup(){
  Wire.begin(8, 9);
  Serial.begin(115200);
  for (uint8_t a = 1; a < 127; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print("found 0x"); Serial.println(a, HEX); }
  }
}
void loop(){}
```

If `display.begin()` fails, BEEP prints
`BEEP: SSD1306 init FAILED (check address/pins)` and keeps running so the
message is visible on the serial monitor.

---

## 9. Complete serial command list

Case-insensitive, one command per line, at **115200 baud**:

```
idle  happy  excited  curious  surprised  sleepy  sleep
sad   annoyed  love  think  listen  confused  demo  help
```

- `demo` cycles all expressions automatically.
- Any expression command cancels demo mode.
- `help` (or `?`) prints the list.
- BEEP echoes a single confirmation line per command; it does not flood.

---

## 10. Assumptions vs. verified

**Assumptions (unverified — check when hardware arrives):**

- Display is SSD1306, 128×64, I²C, address `0x3C`.
- ESP32-C3 Mini I²C is on **GPIO8 (SDA) / GPIO9 (SCL)**. Many C3 "Super Mini"
  boards use these, but **some use GPIO5/GPIO6** or expose different pins.
- `400 kHz` I²C is stable with the module's pull-ups.
- The Adafruit library API used (`Adafruit_SSD1306(w,h,&Wire,-1)`,
  `begin(SSD1306_SWITCHCAPVCC, addr)`) matches the installed versions.

**Verified here:**

- The visual design and all 13 expressions were rendered and reviewed through
  the Python preview.
- No compilation, upload, or electrical test has occurred.

---

## 11. Physical testing checklist

When the display arrives:

1. **Wiring**: VCC→3V3, GND→GND, SDA→`BEEP_I2C_SDA`, SCL→`BEEP_I2C_SCL`.
   Confirm 3.3 V logic (SSD1306 modules are usually 3.3 V tolerant).
2. **Address**: run the I²C scan; set `BEEP_I2C_ADDR` to what you find.
3. **Init**: on boot, serial should print `BEEP awake.`; if it prints the
   failure line, recheck pins/address.
4. **Idle**: BEEP should blink randomly (some one-eyed), drift its pupils, and
   breathe subtly — it must look alive with no input.
5. **Each command**: run all 13; confirm the face matches the preview and that
   temporary expressions return to `idle`.
6. **Demo**: confirm smooth transitions and no flicker.
7. **Persistence**: `sleep` and `listen` must stay until changed; `hello`/typo
   should report "unknown".
8. **Responsiveness**: commands must react within a frame (~33 ms), and the
   loop must never stall.
9. **Panel tuning**: if any edge content is clipped, adjust eye geometry in
   `config.h`; if the face is too large/small, scale `BEEP_EYE_HALF_W` and
   `BEEP_EYE_CX_OFFSET` together.
10. **Stability**: leave running; confirm no drift or glitches (millis-rollover
    safe by construction, but watch for I²C dropouts).

---

## 12. Extending BEEP

- **New expression**: add the enum, a `kParams` row, a `kNames` entry and a
  `kHold` value. Nothing else changes.
- **New mouth/pupil shape**: extend the enums and the matching `switch` in
  `renderer.cpp`.
- **Future AI/mood input**: feed an `Expr` (or drive `FaceParams` directly) into
  `Animator::setExpr()`; the renderer and animation engine already support
  arbitrary states without modification. That is exactly the seam Phase 2
  (microphone/AI) will plug into.

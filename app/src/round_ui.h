#pragma once
// Round-display UI (BUDDY_ROUND_DISPLAY), header-only, included from
// main.cpp after its globals. Menus fall back to the stick UI scaled into
// the circle.
//
// One look throughout: Clawd orange and ink, cream text on a warm dark
// background. Dark pages share a status line at the top, a small caps
// label, one hero in the middle and the page dots at the bottom.

static const int RND   = 412;        // panel size
static const int RND_C = RND / 2;    // centre and radius

// Approval buttons (and their tap zone).
static const int RND_BTN_Y0 = 296, RND_BTN_Y1 = 350;
static const int RND_ALLOW_X0 = 62, RND_ALLOW_X1 = 202;
static const int RND_DENY_X0 = 210, RND_DENY_X1 = 350;

// Controls page buttons.
static const int RND_CTL_Y = 182, RND_CTL_R = 50;
static const int RND_MUTE_X = 106, RND_BRIGHT_X = 206, RND_SLEEP_X = 306;

// Palette (RGB565).
static const uint16_t CLAWD_ORANGE = 0xDBAA;   // #D97757
static const uint16_t CLAWD_INK    = 0x18C3;   // #1A1A1A
static const uint16_t CLAWD_PINK   = 0xF8B2;
static const uint16_t RND_BG       = 0x1081;   // #12100E
static const uint16_t RND_PANEL    = 0x20E3;   // #221F1C
static const uint16_t RND_CREAM    = 0xEF5C;   // #ECE9E2
static const uint16_t RND_DIM      = 0x8C2F;   // #8C877D
static const uint16_t RND_ALLOW    = 0x6DEF;   // #6BBF7A
static const uint16_t RND_DENY     = 0xE289;   // #E5534B

static const int RND_STATUS_Y = 44, RND_LABEL_Y = 90, RND_DOTS_Y = 388;

// Night mode: 22:00-07:00 by the RTC, once it has been set.
static const uint8_t RND_NIGHT_FROM = 22, RND_NIGHT_TO = 7;
static const int     RND_NIGHT_Y = 340;    // toggle row on the controls page

enum { RND_PAGE_FACE, RND_PAGE_STATS, RND_PAGE_CONTROLS, RND_PAGES };
static uint8_t  rndPage = RND_PAGE_FACE;
static bool     rndNightOn = true;
static uint32_t rndLastFrame = 0;
static bool     rndDecisionAllow = false;  // what the user tapped, for feedback

// Nearest-neighbour copy of a region of the stick sprite onto the canvas.
// Both are panel-order RGB565, so pixels move as raw 16-bit words.
static void rndBlit(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
  const uint16_t* src = (const uint16_t*)spr.getBuffer();
  uint16_t* dst = (uint16_t*)M5.canvas().getBuffer();
  if (!src || !dst) return;
  for (int y = 0; y < dh; y++) {
    int ty = dy + y;
    if (ty < 0 || ty >= RND) continue;
    const uint16_t* srow = src + (sy + y * sh / dh) * W + sx;
    uint16_t* drow = dst + ty * RND;
    for (int x = 0; x < dw; x++) {
      int tx = dx + x;
      if (tx >= 0 && tx < RND) drow[tx] = srow[x * sw / dw];
    }
  }
}

static uint16_t rndBlend(uint16_t a, uint16_t b, float t) {
  int ar = a >> 11, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = b >> 11, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  return ((ar + (int)((br - ar) * t)) << 11) | ((ag + (int)((bg - ag) * t)) << 5) | (ab + (int)((bb - ab) * t));
}

static void rndText(const lgfx::IFont* f, uint8_t datum, uint16_t fg, uint16_t bg,
                    const char* s, int x, int y) {
  TFT_eSPI& c = M5.canvas();
  c.setFont(f);
  c.setTextDatum(datum);
  c.setTextColor(fg, bg);
  c.drawString(s, x, y);
}

static bool rndClockSet() { return dataRtcValid() || _clkDt.Year >= 2024; }

static void rndDots(uint16_t on, uint16_t off) {
  TFT_eSPI& c = M5.canvas();
  for (int i = 0; i < RND_PAGES; i++)
    c.fillCircle(RND_C - 16 + i * 16, RND_DOTS_Y, 3, i == rndPage ? on : off);
}

static int rndBatteryPct() {
  int mv = (int)(M5.Axp.GetBatVoltage() * 1000);
  int pct = (mv - 3200) / 10;
  pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
  // On battery the charge only goes down, so never show it creeping back up.
  static int shown = -1;
  bool charging = M5.Axp.GetVBusVoltage() > 4.0f;
  if (shown < 0 || charging || pct < shown) shown = pct;
  return shown;
}

// Top of every dark page: the time and a small battery.
static void rndStatus() {
  TFT_eSPI& c = M5.canvas();
  int pct = rndBatteryPct();
  uint16_t col = _onUsb ? RND_ALLOW : pct <= 20 ? RND_DENY : RND_DIM;
  char s[16];
  if (rndClockSet()) snprintf(s, sizeof(s), "%02u:%02u", _clkTm.Hours, _clkTm.Minutes);
  else               snprintf(s, sizeof(s), "--:--");
  rndText(&fonts::DejaVu12, MR_DATUM, RND_DIM, RND_BG, s, RND_C - 8, RND_STATUS_Y);
  int x = RND_C + 8, y = RND_STATUS_Y - 6;
  c.drawRoundRect(x, y, 22, 12, 3, RND_DIM);
  c.fillRect(x + 22, y + 3, 2, 6, RND_DIM);
  c.fillRect(x + 2, y + 2, 18 * pct / 100, 8, col);
  snprintf(s, sizeof(s), _onUsb ? "%d%%+" : "%d%%", pct);
  rndText(&fonts::DejaVu12, ML_DATUM, col, RND_BG, s, x + 29, RND_STATUS_Y);
}

static void rndLabel(const char* s, int y = RND_LABEL_Y) {
  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_BG, s, RND_C, y);
}

// 31200 -> "31.2k", 1840000 -> "1.84M".
static void rndFmtCount(uint32_t v, char* out, size_t n) {
  if (v < 1000)            snprintf(out, n, "%lu", (unsigned long)v);
  else if (v < 100000)     snprintf(out, n, "%.1fk", v / 1000.0f);
  else if (v < 1000000)    snprintf(out, n, "%luk", (unsigned long)(v / 1000));
  else                     snprintf(out, n, "%.2fM", v / 1000000.0f);
}

// Up to maxRows lines of `in`, broken every `cols` characters (monospace).
static uint8_t rndChop(const char* in, uint8_t cols, char out[][32], uint8_t maxRows) {
  uint8_t n = 0;
  size_t len = strlen(in);
  for (size_t i = 0; i < len && n < maxRows; i += cols, n++) {
    snprintf(out[n], 32, "%.*s", (int)cols, in + i);
  }
  if (n == maxRows && len > (size_t)cols * maxRows) {
    size_t l = strlen(out[n - 1]);
    if (l >= 2) { out[n - 1][l - 2] = '.'; out[n - 1][l - 1] = '.'; }
  }
  return n;
}

// A comet sweeping round the bezel while Claude is working.
static void rndComet(uint16_t bg) {
  if (!tama.sessionsRunning) return;
  TFT_eSPI& c = M5.canvas();
  int head = (millis() / 6) % 360;
  for (int i = 0; i < 6; i++) {
    int a0 = (head - (i + 1) * 12 + 720) % 360, a1 = a0 + 12;
    uint16_t col = rndBlend(bg, CLAWD_ORANGE, 1.0f - i / 6.0f);
    c.fillArc(RND_C, RND_C, RND_C - 2, RND_C - 7, a0, min(a1, 360), col);
    if (a1 > 360) c.fillArc(RND_C, RND_C, RND_C - 2, RND_C - 7, 0, a1 - 360, col);
  }
}

// ------------------------------------------------------------ Clawd --

static void rndEye(int cx, int cy, int w, int h, uint16_t col) {
  M5.canvas().fillRoundRect(cx - w / 2, cy - h / 2, w, h, max(2, w / 10), col);
}

static void rndChevron(int cx, int cy, int w, uint16_t col) {   // ^
  TFT_eSPI& c = M5.canvas();
  int t = max(4, w / 5);
  c.drawWideLine(cx - w / 2, cy + w / 3, cx, cy - w / 3, t, col);
  c.drawWideLine(cx, cy - w / 3, cx + w / 2, cy + w / 3, t, col);
}

static void rndCross(int cx, int cy, int w, uint16_t col) {     // X
  TFT_eSPI& c = M5.canvas();
  int t = max(4, w / 5);
  c.drawWideLine(cx - w / 2, cy - w / 2, cx + w / 2, cy + w / 2, t, col);
  c.drawWideLine(cx - w / 2, cy + w / 2, cx + w / 2, cy - w / 2, t, col);
}

static void rndHeartShape(int cx, int cy, int r, uint16_t col) {
  TFT_eSPI& c = M5.canvas();
  c.fillCircle(cx - r * 6 / 10, cy - r / 4, r * 6 / 10, col);
  c.fillCircle(cx + r * 6 / 10, cy - r / 4, r * 6 / 10, col);
  c.fillTriangle(cx - r * 12 / 10, cy, cx + r * 12 / 10, cy, cx, cy + r * 13 / 10, col);
}

// Clawd's two eyes, centred on (cx, cy), `scale` 1.0 = the face page.
// mood: the persona state; dx/dy offset where they look.
static void rndEyes(int cx, int cy, float scale, int mood, int dx = 0, int dy = 0) {
  uint32_t now = millis();
  int ex = (int)(80 * scale), ew = (int)(60 * scale), eh = (int)(84 * scale);
  int l = cx - ex + dx, r = cx + ex + dx, y = cy + dy;
  switch (mood) {
    case P_SLEEP:
      rndEye(l, y + eh / 4, ew, max(4, eh / 7), CLAWD_INK);
      rndEye(r, y + eh / 4, ew, max(4, eh / 7), CLAWD_INK);
      if (scale >= 1.0f) {
        int ph = (now / 120) % 40;                          // a z drifting up
        rndText(&fonts::DejaVu24, MC_DATUM, CLAWD_INK, CLAWD_ORANGE, "z", r + 50 + ph / 2, y - 30 - ph);
      }
      break;
    case P_CELEBRATE:
      rndChevron(l, y, ew, CLAWD_INK);
      rndChevron(r, y, ew, CLAWD_INK);
      break;
    case P_HEART:
      rndHeartShape(l, y, (int)(34 * scale), CLAWD_PINK);
      rndHeartShape(r, y, (int)(34 * scale), CLAWD_PINK);
      break;
    case P_DIZZY:
      rndCross(l, y, ew, CLAWD_INK);
      rndCross(r, y, ew, CLAWD_INK);
      break;
    default: {
      // blink roughly every 4 s, with the odd double blink
      uint32_t ph = now % 4100;
      bool blink = ph < 130 || (now % 13900 > 300 && now % 13900 < 430);
      int h = blink ? max(4, eh / 8) : (mood == P_ATTENTION ? eh * 8 / 7 : eh);
      rndEye(l, y, ew, h, CLAWD_INK);
      rndEye(r, y, ew, h, CLAWD_INK);
    }
  }
}

// Orange band across the top of a dark page, with Clawd's eyes in it.
static void rndBand(int bottom, int mood) {
  M5.canvas().fillRect(0, 0, RND, bottom, CLAWD_ORANGE);
  rndEyes(RND_C, bottom / 2 + 14, 0.5f, mood);
}

// ------------------------------------------------------------ pages --

static void rndFace() {
  TFT_eSPI& c = M5.canvas();
  c.fillScreen(CLAWD_ORANGE);
  uint32_t now = millis();

  // Where the eyes look: wander when idle, scan left-right when working.
  static float lx = 0, ly = 0;
  static int tx = 0, ty = 0;
  static uint32_t nextLook = 0;
  if ((int32_t)(now - nextLook) >= 0) {
    uint32_t h = now * 2654435761u;
    tx = (int)((h >> 8) % 41) - 20;
    ty = (int)((h >> 16) % 21) - 10;
    if ((h >> 4) % 3 == 0) tx = ty = 0;                   // often just look ahead
    nextLook = now + 1800 + (h >> 20) % 2500;
  }
  int gx = tx, gy = ty;
  if (activeState == P_BUSY || (tama.sessionsRunning && activeState == P_IDLE)) {
    gx = ((now / 900) & 1) ? 24 : -24;                    // reading
    gy = 6;
  }
  lx += (gx - lx) * 0.3f;
  ly += (gy - ly) * 0.3f;
  rndEyes(RND_C, 178, 1.0f, activeState, (int)lx, (int)ly);

  if (rndClockSet()) {
    char hm[6];
    snprintf(hm, sizeof(hm), "%02u:%02u", _clkTm.Hours, _clkTm.Minutes);
    rndText(&fonts::DejaVu24, MC_DATUM, CLAWD_INK, CLAWD_ORANGE, hm, RND_C, 318);
  }
  int pct = rndBatteryPct();
  if (pct <= 20 && !_onUsb) {
    char b[8];
    snprintf(b, sizeof(b), "%d%%", pct);
    rndText(&fonts::DejaVu12, MC_DATUM, CLAWD_INK, CLAWD_ORANGE, b, RND_C, 350);
  }
  rndDots(CLAWD_INK, rndBlend(CLAWD_ORANGE, CLAWD_INK, 0.35f));
}

// One figure in the stats row: value over a small label.
static void rndFigure(int x, int y, const char* value, const char* label, uint16_t col) {
  rndText(&fonts::DejaVu24, MC_DATUM, col, RND_BG, value, x, y);
  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_BG, label, x, y + 26);
}

static void rndStats() {
  TFT_eSPI& c = M5.canvas();
  rndStatus();
  rndLabel("TOKENS TODAY");
  char s[24];
  rndFmtCount(tama.tokensToday, s, sizeof(s));
  rndText(&fonts::DejaVu56, MC_DATUM, RND_CREAM, RND_BG, s, RND_C, 144);

  c.drawFastHLine(RND_C - 120, 196, 240, RND_PANEL);
  char run[8], wait[8], all[12];
  snprintf(run, sizeof(run), "%u", tama.sessionsRunning);
  snprintf(wait, sizeof(wait), "%u", tama.sessionsWaiting);
  rndFmtCount(stats().tokens, all, sizeof(all));
  rndFigure(RND_C - 96, 232, run, "running", tama.sessionsRunning ? CLAWD_ORANGE : RND_CREAM);
  rndFigure(RND_C, 232, wait, "waiting", tama.sessionsWaiting ? RND_DENY : RND_CREAM);
  rndFigure(RND_C + 96, 232, all, "all time", RND_CREAM);

  char yes[8], no[8];
  snprintf(yes, sizeof(yes), "%u", stats().approvals);
  snprintf(no, sizeof(no), "%u", stats().denials);
  rndFigure(RND_C - 50, 300, yes, "allowed", RND_ALLOW);
  rndFigure(RND_C + 50, 300, no, "denied", RND_DENY);

  rndText(&fonts::DejaVu12, MC_DATUM, tama.connected ? RND_DIM : RND_DENY, RND_BG,
          tama.connected ? "Claude linked" : "Claude not connected", RND_C, 360);
  rndDots(RND_CREAM, RND_PANEL);
  rndComet(RND_BG);
}

static void rndSpeaker(int cx, int cy, uint16_t col, bool muted) {
  TFT_eSPI& c = M5.canvas();
  c.fillRect(cx - 18, cy - 7, 9, 14, col);
  c.fillTriangle(cx - 10, cy, cx + 2, cy - 14, cx + 2, cy + 14, col);
  if (muted) {
    c.drawWideLine(cx + 8, cy - 8, cx + 20, cy + 8, 3, col);
    c.drawWideLine(cx + 8, cy + 8, cx + 20, cy - 8, 3, col);
  } else {
    for (int r : {11, 19}) {
      c.drawArc(cx + 2, cy, r + 1, r - 1, 315, 360, col);
      c.drawArc(cx + 2, cy, r + 1, r - 1, 0, 45, col);
    }
  }
}

static void rndMoon(int cx, int cy, uint16_t col, uint16_t bg) {
  TFT_eSPI& c = M5.canvas();
  c.fillCircle(cx, cy, 16, col);
  c.fillCircle(cx + 8, cy - 6, 14, bg);
}

static void rndSun(int cx, int cy, uint16_t col) {
  TFT_eSPI& c = M5.canvas();
  c.fillCircle(cx, cy, 8, col);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    c.drawWideLine(cx + cosf(a) * 12, cy + sinf(a) * 12, cx + cosf(a) * 17, cy + sinf(a) * 17, 2, col);
  }
}

static void rndControls() {
  TFT_eSPI& c = M5.canvas();
  rndStatus();
  rndLabel("CONTROLS");

  // Active means "not the default": an orange button with ink on it.
  bool muted = !settings().sound;
  uint16_t mbg = muted ? CLAWD_ORANGE : RND_PANEL, mfg = muted ? CLAWD_INK : RND_CREAM;
  c.fillCircle(RND_MUTE_X, RND_CTL_Y, RND_CTL_R, mbg);
  rndSpeaker(RND_MUTE_X, RND_CTL_Y - 10, mfg, muted);
  rndText(&fonts::DejaVu12, MC_DATUM, muted ? CLAWD_INK : RND_DIM, mbg, muted ? "muted" : "sound",
          RND_MUTE_X, RND_CTL_Y + 24);

  c.fillCircle(RND_BRIGHT_X, RND_CTL_Y, RND_CTL_R, RND_PANEL);
  rndSun(RND_BRIGHT_X, RND_CTL_Y - 12, RND_CREAM);
  for (int i = 0; i < 4; i++)                                  // brightness 0..4 as pips
    c.fillCircle(RND_BRIGHT_X - 15 + i * 10, RND_CTL_Y + 24, 3, i < brightLevel ? CLAWD_ORANGE : RND_DIM);

  c.fillCircle(RND_SLEEP_X, RND_CTL_Y, RND_CTL_R, RND_PANEL);
  rndMoon(RND_SLEEP_X, RND_CTL_Y - 10, RND_CREAM, RND_PANEL);
  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_PANEL, "sleep", RND_SLEEP_X, RND_CTL_Y + 24);

  char s[40];
  snprintf(s, sizeof(s), "%s  %.2fV", _onUsb ? "on USB" : "on battery", M5.Axp.GetBatVoltage());
  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_BG, s, RND_C, 278);
  rndText(&fonts::DejaVu12, MC_DATUM, tama.connected ? RND_DIM : RND_DENY, RND_BG,
          tama.connected ? "Claude linked" : "Claude not connected", RND_C, 300);

  char nl[32];
  snprintf(nl, sizeof(nl), "night %02u-%02u  %s", RND_NIGHT_FROM, RND_NIGHT_TO, rndNightOn ? "on" : "off");
  uint16_t ncol = rndNightOn ? CLAWD_ORANGE : RND_DIM;
  c.drawRoundRect(RND_C - 80, RND_NIGHT_Y - 14, 160, 28, 14, ncol);
  rndText(&fonts::DejaVu12, MC_DATUM, ncol, RND_BG, nl, RND_C, RND_NIGHT_Y);
  rndDots(RND_CREAM, RND_PANEL);
}

static void rndApproval(bool inPrompt) {
  TFT_eSPI& c = M5.canvas();
  int mood = inPrompt ? P_ATTENTION : (rndDecisionAllow ? P_CELEBRATE : P_DIZZY);
  rndBand(140, mood);

  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_BG, "Claude wants to use", RND_C, 166);
  rndText(&fonts::DejaVu40, MC_DATUM, RND_CREAM, RND_BG, tama.promptTool, RND_C, 200);

  // Command in a monospace box: 3 rows of 25 columns.
  static char rows[3][32];
  uint8_t n = rndChop(tama.promptHint, 25, rows, 3);
  if (n) {
    c.fillRoundRect(64, 230, 284, 58, 10, RND_PANEL);
    for (uint8_t i = 0; i < n; i++)
      rndText(&fonts::FreeMono9pt7b, TL_DATUM, RND_CREAM, RND_PANEL, rows[i], 74, 235 + i * 17);
  }

  if (!inPrompt) {
    rndText(&fonts::DejaVu40, MC_DATUM, rndDecisionAllow ? RND_ALLOW : RND_DENY, RND_BG,
            rndDecisionAllow ? "Allowed" : "Denied", RND_C, 322);
    return;
  }
  int bh = RND_BTN_Y1 - RND_BTN_Y0;
  c.fillRoundRect(RND_ALLOW_X0, RND_BTN_Y0, RND_ALLOW_X1 - RND_ALLOW_X0, bh, bh / 2, RND_ALLOW);
  c.fillRoundRect(RND_DENY_X0, RND_BTN_Y0, RND_DENY_X1 - RND_DENY_X0, bh, bh / 2, RND_DENY);
  int by = (RND_BTN_Y0 + RND_BTN_Y1) / 2;
  rndText(&fonts::DejaVu24, MC_DATUM, CLAWD_INK, RND_ALLOW, "Allow", (RND_ALLOW_X0 + RND_ALLOW_X1) / 2, by);
  rndText(&fonts::DejaVu24, MC_DATUM, CLAWD_INK, RND_DENY, "Deny", (RND_DENY_X0 + RND_DENY_X1) / 2, by);

  uint32_t waited = (millis() - promptArrivedMs) / 1000;
  char s[12];
  snprintf(s, sizeof(s), "%lus", (unsigned long)waited);
  rndText(&fonts::DejaVu12, MC_DATUM, waited >= 10 ? RND_DENY : RND_DIM, RND_BG, s, RND_C, 370);

  // The "led" setting: a ring pulsing round the bezel while a prompt waits.
  if (settings().led) {
    float t = (sinf(millis() / 260.0f) + 1.0f) * 0.5f;
    c.fillArc(RND_C, RND_C, RND_C - 1, RND_C - 9, 0, 360, rndBlend(RND_BG, CLAWD_ORANGE, 0.2f + 0.8f * t));
  }
}

static void rndPasskey() {
  char pk[8];
  snprintf(pk, sizeof(pk), "%06lu", (unsigned long)blePasskey());
  rndBand(140, P_ATTENTION);
  rndLabel("BLUETOOTH PAIRING", 180);
  rndText(&fonts::DejaVu56, MC_DATUM, RND_CREAM, RND_BG, pk, RND_C, 236);
  rndText(&fonts::DejaVu12, MC_DATUM, RND_DIM, RND_BG, "enter this in the Claude desktop app", RND_C, 290);
}

// ------------------------------------------------------- lifecycle --

static void roundInit() {
  _prefs.begin("buddy", true);
  rndNightOn = _prefs.getBool("night", true);
  _prefs.end();
}

// Quiet hours: dim and mute, except while a prompt needs reading.
static bool roundNight() {
  if (!rndNightOn || tama.promptId[0]) return false;
  if (!rndClockSet()) return false;
  uint8_t h = _clkTm.Hours;
  return h >= RND_NIGHT_FROM || h < RND_NIGHT_TO;
}

// Once per loop: re-apply brightness when quiet hours start or end, and
// wake the screen when the board is picked up.
static void roundTick(bool dark) {
  static bool was = false;
  bool now = roundNight();
  if (now != was && !dark) applyBrightness();
  was = now;

  // Wake on lift: while the screen is off, compare gravity against where
  // it pointed when the screen went off. 0.35 g is a lift or a tilt of
  // ~20 degrees; desk knocks settle back under it.
  static bool     armed = false;
  static float    bx, by, bz;
  static uint32_t lastRead = 0;
  if (!screenOff) { armed = false; return; }
  if (millis() - lastRead < 100) return;
  lastRead = millis();
  float ax, ay, az;
  M5.Imu.getAccelData(&ax, &ay, &az);
  if (!armed) { bx = ax; by = ay; bz = az; armed = true; return; }
  float dx = ax - bx, dy = ay - by, dz = az - bz;
  if (dx * dx + dy * dy + dz * dz > 0.35f * 0.35f) wake();
}

// Stick UI scaled 1.5x into the circle: menus, splash, info pages.
static void roundStick(const Palette& p) {
  M5.canvas().fillScreen(p.bg);
  rndBlit(0, 0, W, H, 104, 26, 204, 360);
}

static bool roundOwnsInput() {
  return !(menuOpen || settingsOpen || resetOpen || displayMode != DISP_NORMAL);
}

// Called once per frame instead of spr.pushSprite(). Caps at ~20 fps: a
// full push takes ~15 ms and the pet only animates at 5 fps.
static void roundRender(bool inPrompt, bool force = false) {
  if (!force && millis() - rndLastFrame < 50) return;
  rndLastFrame = millis();
  M5.canvas().fillScreen(RND_BG);

  if (!roundOwnsInput() || !(buddyMode || characterLoaded())) roundStick(characterPalette());
  else if (blePasskey())                                       rndPasskey();
  else if (tama.promptId[0])                                   rndApproval(inPrompt);
  else if (rndPage == RND_PAGE_CONTROLS)                       rndControls();
  else if (rndPage == RND_PAGE_STATS)                          rndStats();
  else                                                         rndFace();

  M5.present();
}

// On an approval button, with a little slack since fingers are big.
static bool rndOnButton(int x, int y, int x0, int x1) {
  return x >= x0 - 8 && x <= x1 + 8 && y >= RND_BTN_Y0 - 10 && y <= RND_BTN_Y1 + 14;
}

static bool rndInCircle(int x, int y, int cx, int cy, int r) {
  return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

// Taps while the round UI owns input. approve/deny come from main.cpp.
static void roundTap(int x, int y, bool inPrompt) {
  if (blePasskey()) return;
  if (inPrompt) {
    // Deliberate presses only: start and end on the same button, not a brush,
    // and not before the card has been seen.
    if (millis() - promptArrivedMs < 500 || M5.tapMs() < 40) return;
    bool allow = rndOnButton(x, y, RND_ALLOW_X0, RND_ALLOW_X1) &&
                 rndOnButton(M5.tapEndX(), M5.tapEndY(), RND_ALLOW_X0, RND_ALLOW_X1);
    bool deny  = rndOnButton(x, y, RND_DENY_X0, RND_DENY_X1) &&
                 rndOnButton(M5.tapEndX(), M5.tapEndY(), RND_DENY_X0, RND_DENY_X1);
    if (allow)     { rndDecisionAllow = true;  approvePrompt(); }
    else if (deny) { rndDecisionAllow = false; denyPrompt(); }
    return;
  }
  if (tama.promptId[0]) return;   // answered, waiting for the desktop to clear it
  if (rndPage == RND_PAGE_CONTROLS) {
    if (rndInCircle(x, y, RND_MUTE_X, RND_CTL_Y, RND_CTL_R + 6)) {
      settings().sound = !settings().sound;
      settingsSave();
      beep(1800, 40);   // only audible when turning sound back on
    } else if (rndInCircle(x, y, RND_BRIGHT_X, RND_CTL_Y, RND_CTL_R + 6)) {
      brightLevel = (brightLevel + 1) % 5;
      applyBrightness();
      beep(1800, 30);
    } else if (abs(y - RND_NIGHT_Y) <= 18 && abs(x - RND_C) <= 90) {
      rndNightOn = !rndNightOn;
      _prefs.begin("buddy", false);
      _prefs.putBool("night", rndNightOn);
      _prefs.end();
      applyBrightness();
      beep(1800, 30);
    } else if (rndInCircle(x, y, RND_SLEEP_X, RND_CTL_Y, RND_CTL_R + 6)) {
      M5.Axp.SetLDO2(false);
      screenOff = true;
    }
    return;
  }
  // Face: a tap anywhere gets heart eyes.
  if (rndPage == RND_PAGE_FACE && (int32_t)(millis() - oneShotUntil) >= 0) {
    triggerOneShot(P_HEART, 1500);
    beep(2093, 50);
  }
}

// Swipe left for the next page, right for the previous: face, stats, controls.
static void roundSwipe(int dir) {
  rndPage = (rndPage + (dir < 0 ? 1 : RND_PAGES - 1)) % RND_PAGES;
  beep(1800, 30);
}

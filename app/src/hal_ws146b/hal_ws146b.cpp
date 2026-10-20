// Waveshare ESP32-S3-Touch-LCD-1.46B hardware layer behind the M5StickCPlus
// API surface the buddy firmware uses. See M5StickCPlus.h in this folder.

#include "M5StickCPlus.h"
#include <Wire.h>
#include "driver/spi_master.h"
#include "driver/i2s_std.h"
#include "driver/usb_serial_jtag.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_sleep.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

extern "C" {
#include "esp_lcd_spd2010.h"
}

HalM5 M5;

static const int PIN_I2C_SDA = 11, PIN_I2C_SCL = 10;
static const int PIN_LCD_CS = 21, PIN_LCD_SCK = 40;
static const int PIN_LCD_D0 = 46, PIN_LCD_D1 = 45, PIN_LCD_D2 = 42, PIN_LCD_D3 = 41;
static const int PIN_BL = 5;
static const int PIN_BOOT = 0;
static const int PIN_PWR_KEY = 6, PIN_PWR_LATCH = 7;
static const int PIN_BAT_ADC = 8;
static const int PIN_I2S_BCK = 48, PIN_I2S_WS = 38, PIN_I2S_DOUT = 47;

static const uint8_t ADDR_EXIO  = 0x20;   // TCA9554
static const uint8_t ADDR_TOUCH = 0x53;   // SPD2010 touch
static const uint8_t ADDR_RTC   = 0x51;   // PCF85063
static const uint8_t ADDR_IMU   = 0x6B;   // QMI8658

static const int PANEL = 412;
static const int CHUNK_ROWS = 20;

static bool i2cWrite(uint8_t addr, uint8_t reg, const uint8_t* d, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  for (size_t i = 0; i < n; i++) Wire.write(d[i]);
  return Wire.endTransmission() == 0;
}

static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* d, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) d[i] = Wire.read();
  return true;
}

static void exioSet(uint8_t pin, bool high) {   // pin is 1-based like the schematic
  uint8_t out = 0;
  i2cRead(ADDR_EXIO, 0x01, &out, 1);
  if (high) out |= (1 << (pin - 1));
  else      out &= ~(1 << (pin - 1));
  i2cWrite(ADDR_EXIO, 0x01, &out, 1);
}

// Vendor init runs through esp_lcd, then pixels go out via polling SPI:
// this Arduino build's SPI ISR isn't in IRAM, so interrupt-driven transfers
// can hang if a completion lands during a flash write (NVS, LittleFS).
static spi_device_handle_t lcdDev = nullptr;
static volatile bool       panelOn = true;
static uint8_t             blLevel = 100;

// SPD2010 QSPI framing: 32-bit header on one line = opcode, 0, cmd, 0.
// 0x02 writes a register with one-line params; 0x32 writes pixels on four.
static void lcdTx(uint8_t opcode, uint8_t cmd, const void* data, size_t len) {
  spi_transaction_t t = {};
  t.addr = ((uint32_t)opcode << 24) | ((uint32_t)cmd << 8);
  t.length = len * 8;
  t.tx_buffer = data;
  if (opcode == 0x32) t.flags = SPI_TRANS_MODE_QIO;
  spi_device_polling_transmit(lcdDev, &t);
}

static void lcdDraw(int x0, int y0, int x1, int y1, const uint16_t* px) {
  uint8_t ca[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)((x1 - 1) >> 8), (uint8_t)(x1 - 1) };
  uint8_t ra[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)((y1 - 1) >> 8), (uint8_t)(y1 - 1) };
  lcdTx(0x02, 0x2A, ca, 4);
  lcdTx(0x02, 0x2B, ra, 4);
  lcdTx(0x32, 0x2C, px, (size_t)(x1 - x0) * (y1 - y0) * 2);
}

static void panelInit() {
  exioSet(2, false); delay(50); exioSet(2, true); delay(50);

  spi_bus_config_t bus = {};
  bus.data0_io_num = PIN_LCD_D0;
  bus.data1_io_num = PIN_LCD_D1;
  bus.sclk_io_num  = PIN_LCD_SCK;
  bus.data2_io_num = PIN_LCD_D2;
  bus.data3_io_num = PIN_LCD_D3;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = PANEL * CHUNK_ROWS * 2 + 64;
  bus.flags = SPICOMMON_BUSFLAG_MASTER;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = PIN_LCD_CS;
  io.dc_gpio_num = -1;
  io.spi_mode = 3;
  io.pclk_hz = 40 * 1000 * 1000;
  io.trans_queue_depth = 10;
  io.lcd_cmd_bits = 32;
  io.lcd_param_bits = 8;
  io.flags.quad_mode = 1;
  esp_lcd_panel_io_handle_t ioh = nullptr;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &ioh));

  static spd2010_vendor_config_t vendor = {};
  vendor.flags.use_qspi_interface = 1;
  esp_lcd_panel_dev_config_t dev = {};
  dev.reset_gpio_num = -1;
  dev.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  dev.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
  dev.bits_per_pixel = 16;
  dev.vendor_config = &vendor;
  esp_lcd_panel_handle_t panel = nullptr;
  ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(ioh, &dev, &panel));
  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_panel_disp_on_off(panel, true);
  esp_lcd_panel_del(panel);
  esp_lcd_panel_io_del(ioh);

  spi_device_interface_config_t dc = {};
  dc.address_bits = 32;
  dc.mode = 3;
  dc.clock_speed_hz = 40 * 1000 * 1000;
  dc.spics_io_num = PIN_LCD_CS;
  dc.queue_size = 1;
  dc.flags = SPI_DEVICE_HALFDUPLEX;
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dc, &lcdDev));
}

// 412x412 canvas in PSRAM, copied out in full rows through a small DMA
// buffer (the glass shows a few pixels beyond the drawn circle).
static TFT_eSPI  canvasSpr;
static uint16_t* dmaBuf = nullptr;

TFT_eSPI& HalM5::canvas() { return canvasSpr; }

// Blank panel memory once so power-up garbage never shows.
static void clearPanel() {
  if (!dmaBuf) return;
  memset(dmaBuf, 0, PANEL * CHUNK_ROWS * 2);
  for (int y = 0; y < PANEL; y += CHUNK_ROWS) lcdDraw(0, y, PANEL, min(PANEL, y + CHUNK_ROWS), dmaBuf);
}

void HalM5::present() {
  const uint16_t* fb = (const uint16_t*)canvasSpr.getBuffer();
  if (!panelOn || !fb || !dmaBuf) return;
  const int x0 = 0, x1 = PANEL, w = PANEL;
  for (int y = 0; y < PANEL; y += CHUNK_ROWS) {
    int rows = min(CHUNK_ROWS, PANEL - y);
#ifdef WS146B_ROTATE_180
    // Mounted upside down (hinged case): panel pixel (x, y) shows canvas
    // pixel (411 - x, 411 - y).
    for (int r = 0; r < rows; r++) {
      const uint16_t* src = fb + (PANEL - 1 - (y + r)) * PANEL + (PANEL - 1 - x0);
      uint16_t* dst = dmaBuf + r * w;
      for (int i = 0; i < w; i++) dst[i] = src[-i];
    }
#else
    for (int r = 0; r < rows; r++) memcpy(dmaBuf + r * w, fb + (y + r) * PANEL + x0, w * 2);
#endif
    lcdDraw(x0, y, x1, y + rows, dmaBuf);
  }
}

// SPD2010 touch speaks a small state machine over 16-bit-register I2C.
static bool tpWrite(uint16_t reg, uint8_t lo, uint8_t hi) {
  Wire.beginTransmission(ADDR_TOUCH);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF);
  Wire.write(lo); Wire.write(hi);
  bool ok = Wire.endTransmission() == 0;
  delayMicroseconds(200);
  return ok;
}

static bool tpRead(uint16_t reg, uint8_t* d, size_t n) {
  Wire.beginTransmission(ADDR_TOUCH);
  Wire.write(reg >> 8); Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  size_t got = Wire.requestFrom(ADDR_TOUCH, (uint8_t)n);
  for (size_t i = 0; i < got; i++) d[i] = Wire.read();
  return got == n;
}

// Returns 1 = finger down at (x,y), 0 = explicit lift, -1 = no news.
static int touchPoll(uint16_t* x, uint16_t* y) {
  uint8_t st[4];
  if (!tpRead(0x2000, st, 4)) return -1;
  bool ptExist  = st[0] & 0x01;
  bool gesture  = st[0] & 0x02;
  bool aux      = st[0] & 0x08;
  bool inBios   = st[1] & 0x40;
  bool inCpu    = st[1] & 0x20;
  bool cpuRun   = st[1] & 0x08;
  uint16_t len  = st[3] << 8 | st[2];

  if (inBios) { tpWrite(0x0200, 0x01, 0x00); tpWrite(0x0400, 0x01, 0x00); return -1; }
  if (inCpu)  { tpWrite(0x5000, 0x00, 0x00); tpWrite(0x4600, 0x00, 0x00); tpWrite(0x0200, 0x01, 0x00); return -1; }
  if (cpuRun && len == 0) { tpWrite(0x0200, 0x01, 0x00); return -1; }

  int result = -1;
  if (ptExist || gesture) {
    uint8_t d[4 + 10 * 6];
    if (len > sizeof(d)) len = sizeof(d);
    if (len >= 4 && tpRead(0x0003, d, len)) {
      if (d[4] <= 0x0A && ptExist && len >= 10) {
        *x = ((d[7] & 0xF0) << 4) | d[5];
        *y = ((d[7] & 0x0F) << 8) | d[6];
        result = d[8] ? 1 : 0;
      } else {
        result = 0;
      }
    }
    for (int guard = 0; guard < 8; guard++) {
      uint8_t hs[8];
      if (!tpRead(0xFC02, hs, 8)) break;
      if (hs[5] == 0x82) { tpWrite(0x0200, 0x01, 0x00); break; }
      if (hs[5] != 0x00) break;
      uint16_t nl = hs[2] | hs[3] << 8;
      uint8_t junk[32];
      tpRead(0x0003, junk, min<size_t>(nl, sizeof(junk)));
    }
  } else if (cpuRun && aux) {
    tpWrite(0x0200, 0x01, 0x00);
  }
  return result;
}

static void touchInit() {
  exioSet(1, false); delay(50); exioSet(1, true); delay(50);
  pinMode(4, INPUT_PULLUP);
}

void HalButton::set(bool pressed, uint32_t since) {
  _changed = pressed != _pressed;
  if (_changed) { _pressed = pressed; _since = since; }
}

bool HalM5::takeTap(int* x, int* y) {
  if (!_tap) return false;
  _tap = false;
  *x = _tapX;
  *y = _tapY;
  return true;
}

int HalM5::swipe() {
  int s = _swipe;
  _swipe = 0;
  return s;
}

static bool     pwrArmed = false;   // PWR key must be released once after boot
static bool     pwrDown = false;
static uint32_t pwrSince = 0;

// Touch gestures, resolved on lift so a swipe can never land as a tap:
//   tap        -> takeTap() reports where; with tapsAsButtons set it also
//                 presses+releases A (left half) or B (right half)
//   hold 600ms -> A held, so the firmware's long-press menu fires
//   swipe      -> swipe() returns -1 (leftwards) or +1 (rightwards)
static bool     touchDown = false;
static uint32_t touchSeen = 0, touchStart = 0;
static int      startX, startY, lastX, lastY;
static bool     longFired = false;
static uint8_t  tapPending = 0;     // 1 = A, 2 = B: pressed this frame, released next

void HalM5::update() {
  uint32_t now = millis();

  uint16_t tx, ty;
  int t = touchPoll(&tx, &ty);
  if (t == 1) {
#ifdef WS146B_TOUCH_MIRROR_X
    tx = PANEL - 1 - tx;
#endif
#ifdef WS146B_ROTATE_180
    tx = PANEL - 1 - tx;
    ty = PANEL - 1 - ty;
#endif
    if (!touchDown) {
      touchDown = true;
      touchStart = now;
      startX = tx;
      startY = ty;
      longFired = false;
    }
    lastX = tx;
    lastY = ty;
    touchSeen = now;
  }
  bool lifted = touchDown && (t == 0 || now - touchSeen > 120);
  int dx = lastX - startX, dy = lastY - startY;
  bool still = abs(dx) < 30 && abs(dy) < 30;

  if (tapPending) {
    (tapPending == 1 ? BtnA : BtnB).set(false, now);
    tapPending = 0;
  } else if (lifted) {
    touchDown = false;
    if (longFired) {
      BtnA.set(false, now);
    } else if (abs(dx) > 70 && abs(dx) > abs(dy) * 3 / 2) {
      _swipe = dx < 0 ? -1 : 1;
    } else if (still) {
      _tapX = startX;
      _tapY = startY;
      _tapEndX = lastX;
      _tapEndY = lastY;
      _tapMs = touchSeen - touchStart;
      _tap = true;
      if (tapsAsButtons) {
        tapPending = startX >= PANEL / 2 ? 2 : 1;
        (tapPending == 1 ? BtnA : BtnB).set(true, now);
      }
    }
  } else if (touchDown && !longFired && still && now - touchStart >= 600) {
    longFired = true;
    BtnA.set(true, touchStart);   // backdate so pressedFor(600) holds now
  } else {
    BtnA.set(BtnA.isPressed(), now);   // clear one-frame edges
    BtnB.set(BtnB.isPressed(), now);
  }

  // BOOT is the sleep button: a short press toggles the screen like PWR.
  static bool bootDown = false;
  bool boot = digitalRead(PIN_BOOT) == LOW;
  if (boot != bootDown) {
    bootDown = boot;
    if (!boot) Axp._btnEvent = 0x02;
  }

  bool key = digitalRead(PIN_PWR_KEY) == LOW;
  if (!pwrArmed) { if (!key) pwrArmed = true; return; }
  if (key && !pwrDown) { pwrDown = true; pwrSince = now; }
  if (key && now - pwrSince > 3000) Axp.PowerOff();
  if (!key && pwrDown) {
    pwrDown = false;
    if (now - pwrSince < 1500) Axp._btnEvent = 0x02;
  }
}

void HalImu::Init() {
  uint8_t v;
  v = 0x40; i2cWrite(ADDR_IMU, 0x02, &v, 1);   // CTRL1: address auto-increment
  v = 0x15; i2cWrite(ADDR_IMU, 0x03, &v, 1);   // CTRL2: +-4g, ~235 Hz
  v = 0x01; i2cWrite(ADDR_IMU, 0x08, &v, 1);   // CTRL7: accel on
}

void HalImu::getAccelData(float* ax, float* ay, float* az) {
  uint8_t d[6];
  if (!i2cRead(ADDR_IMU, 0x35, d, 6)) { *ax = 0; *ay = 0; *az = 1; return; }
  const float s = 4.0f / 32768.0f;
  float x = (int16_t)(d[1] << 8 | d[0]) * s;
  float y = (int16_t)(d[3] << 8 | d[2]) * s;
  float z = (int16_t)(d[5] << 8 | d[4]) * s;
  // Map the board's axes onto the stick's: portrait X across, Y along, Z out
  // of the screen. The QMI8658 sits on the back of the board, so Z (and X,
  // to stay right-handed) point the other way.
#ifndef WS146B_IMU_X
#define WS146B_IMU_X (-x)
#define WS146B_IMU_Y (y)
#define WS146B_IMU_Z (-z)
#endif
#ifdef WS146B_ROTATE_180
  *ax = -(WS146B_IMU_X);       // turned 180 degrees in the screen plane
  *ay = -(WS146B_IMU_Y);
#else
  *ax = WS146B_IMU_X;
  *ay = WS146B_IMU_Y;
#endif
  *az = WS146B_IMU_Z;
}

void HalAxp::ScreenBreath(uint8_t level) {
  if (level > 100) level = 100;
  blLevel = level;
  if (panelOn) ledcWrite(PIN_BL, level * 1023 / 100);
}

void HalAxp::SetLDO2(bool on) {
  panelOn = on;
  ledcWrite(PIN_BL, on ? blLevel * 1023 / 100 : 0);
}

// The battery as seen through its ADC line: the board has no VBUS sense and
// no charge gauge, so everything comes from the cell's voltage.
//
// A charger lifts the cell to ~4.2 V the moment it's plugged in and lets go
// the moment it's pulled, so plugging and unplugging show up as a sudden
// step in the voltage: 60 mV or more within a couple of seconds, where the
// backlight or the radio move it far less. Those steps flip the USB state
// straight away; a slow 10 s average gives the resting reading. A connected
// USB host settles it outright.
static float batSlow = 0, batFast = 0;
static float batWindow[8];            // batFast every 250 ms, the last 2 s
static int   batWindowAt = 0;
static bool  batCharger = false;
static uint32_t batLast = 0, batWindowLast = 0, batHighSince = 0;

static void batSample() {
  uint32_t now = millis();
  if (batSlow != 0 && now - batLast < 100) return;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(PIN_BAT_ADC);
  float v = sum / 16.0f * 3.0f / 1000.0f / 0.990476f;
  if (batSlow == 0) {
    batSlow = batFast = v;
    for (float& w : batWindow) w = v;
    batCharger = usb_serial_jtag_is_connected() || v < 2.5f || v >= 4.20f;
  } else {
    float dt = (float)(now - batLast);
    batFast += (v - batFast) * min(1.0f, dt / 1000.0f);
    batSlow += (v - batSlow) * min(1.0f, dt / 10000.0f);
  }
  batLast = now;
  if (now - batWindowLast >= 250) {
    batWindowLast = now;
    batWindow[batWindowAt] = batFast;
    batWindowAt = (batWindowAt + 1) % 8;
  }
  float lo = batWindow[0], hi = batWindow[0];
  for (float w : batWindow) lo = min(lo, w), hi = max(hi, w);

  bool host = usb_serial_jtag_is_connected();
  bool was = batCharger;
  if (host) batCharger = true;
  else if (batFast < 2.5f) batCharger = true;                  // no battery: running off USB
  else if (!batCharger && batFast - lo > 0.06f) batCharger = true;    // stepped up: plugged in
  else if (batCharger && hi - batFast > 0.06f) batCharger = false;    // stepped down: unplugged
  // Booted on a charger there's no step to see: a cell held at the
  // charge-termination voltage for a while can only be on one.
  if (batFast >= 4.19f) {
    if (!batHighSince) batHighSince = now;
    if (now - batHighSince > 5000) batCharger = true;
  } else {
    batHighSince = 0;
  }
  if (batCharger != was) {
    // Start the averages afresh at the new level instead of gliding over.
    batSlow = batFast;
    for (float& w : batWindow) w = batFast;
  }
}

float HalAxp::GetBatVoltage() {
  batSample();
  return batSlow;
}

float HalAxp::GetVBusVoltage() {
  batSample();
  return batCharger ? 5.0f : 0.0f;
}

// While charging the voltage says nothing about the charge (the charger holds
// it near 4.2 V), so the level counts up from where it was at a typical
// charge rate. On battery it follows the resting voltage, and only ever down.
int HalAxp::GetBatLevel() {
  static int shown = -1;
  static uint32_t lastStep = 0;
  batSample();
  int rest = (int)((batSlow - 3.2f) * 100);
  rest = rest < 0 ? 0 : rest > 100 ? 100 : rest;
  uint32_t now = millis();
  if (shown < 0) {
    shown = rest;
    lastStep = now;
  } else if (batCharger) {
    if (now - lastStep >= 40000) {
      lastStep = now;
      if (shown < 100) shown++;
    }
  } else {
    lastStep = now;
    if (rest < shown) shown = rest;
  }
  return shown;
}

uint8_t HalAxp::GetBtnPress() {
  uint8_t e = _btnEvent;
  _btnEvent = 0;
  return e;
}

void HalAxp::PowerOff() {
  ledcWrite(PIN_BL, 0);
  digitalWrite(PIN_PWR_LATCH, LOW);   // cuts battery power
  // Still alive => on USB. Sleep until the PWR key is pressed again.
  while (digitalRead(PIN_PWR_KEY) == LOW) delay(10);
  delay(50);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PWR_KEY, 0);
  esp_deep_sleep_start();
}

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

void HalRtc::GetTime(RTC_TimeTypeDef* t) {
  uint8_t d[3] = {0};
  i2cRead(ADDR_RTC, 0x04, d, 3);
  t->Seconds = bcd2bin(d[0] & 0x7F);
  t->Minutes = bcd2bin(d[1] & 0x7F);
  t->Hours   = bcd2bin(d[2] & 0x3F);
}

void HalRtc::GetDate(RTC_DateTypeDef* dt) {
  uint8_t d[4] = {0};
  i2cRead(ADDR_RTC, 0x07, d, 4);
  dt->Date    = bcd2bin(d[0] & 0x3F);
  dt->WeekDay = d[1] & 0x07;
  dt->Month   = bcd2bin(d[2] & 0x1F);
  dt->Year    = 2000 + bcd2bin(d[3]);
  // PCF85063 OS flag (seconds bit 7) means the clock stopped and the time is
  // stale: report year 2000 so the UI shows "--:--" until SetTime clears it.
  uint8_t sec = 0;
  i2cRead(ADDR_RTC, 0x04, &sec, 1);
  if (sec & 0x80) dt->Year = 2000;
}

void HalRtc::SetTime(const RTC_TimeTypeDef* t) {
  uint8_t d[3] = { bin2bcd(t->Seconds), bin2bcd(t->Minutes), bin2bcd(t->Hours) };
  i2cWrite(ADDR_RTC, 0x04, d, 3);
}

void HalRtc::SetDate(const RTC_DateTypeDef* dt) {
  uint8_t d[4] = { bin2bcd(dt->Date), (uint8_t)(dt->WeekDay & 0x07),
                   bin2bcd(dt->Month), bin2bcd((uint8_t)(dt->Year % 100)) };
  i2cWrite(ADDR_RTC, 0x07, d, 4);
}

// Notes through the PCM5101 DAC on a task so tone() never blocks; queued
// notes play back to back.
struct BeepReq { uint16_t freq, dur; };
static QueueHandle_t      beepQ = nullptr;
static i2s_chan_handle_t  i2sTx = nullptr;
static const int          SAMPLE_RATE = 16000;

// Sine plus a quiet octave partial, fast attack, exponential decay.
static void beepTask(void*) {
  static int16_t frames[256 * 2];
  BeepReq r;
  for (;;) {
    if (xQueueReceive(beepQ, &r, portMAX_DELAY) != pdTRUE) continue;
    uint32_t total = (uint32_t)SAMPLE_RATE * (r.dur + 60) / 1000;   // + ring-out
    uint32_t attack = SAMPLE_RATE * 4 / 1000;
    float w = 2.0f * PI * r.freq / SAMPLE_RATE;
    float decay = expf(-5.0f / total);
    float env = 1.0f;
    uint32_t n = 0;
    while (n < total) {
      size_t k = 0;
      for (; k < 256 && n < total; k++, n++) {
        float a = n < attack ? (float)n / attack : (env *= decay);
        float v = sinf(w * n) + 0.25f * sinf(2.0f * w * n);
        int16_t smp = (int16_t)(v * a * 5200.0f);
        frames[k * 2] = frames[k * 2 + 1] = smp;
      }
      size_t wrote;
      i2s_channel_write(i2sTx, frames, k * 4, &wrote, portMAX_DELAY);
    }
  }
}

void HalBeep::begin() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  cc.auto_clear = true;
  if (i2s_new_channel(&cc, &i2sTx, nullptr) != ESP_OK) return;
  i2s_std_config_t sc = {};
  sc.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
  sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  sc.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCK;
  sc.gpio_cfg.ws   = (gpio_num_t)PIN_I2S_WS;
  sc.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DOUT;
  sc.gpio_cfg.din  = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(i2sTx, &sc) != ESP_OK) return;
  i2s_channel_enable(i2sTx);
  beepQ = xQueueCreate(8, sizeof(BeepReq));
  xTaskCreatePinnedToCore(beepTask, "beep", 3072, nullptr, 2, nullptr, 0);
}

void HalBeep::tone(uint16_t freq, uint16_t durMs) {
  if (!beepQ) return;
  BeepReq r = { freq, durMs };
  xQueueSend(beepQ, &r, 0);
}

void HalM5::begin() {
  // Hold the power latch first so a battery boot survives releasing PWR.
  pinMode(PIN_PWR_LATCH, OUTPUT);
  digitalWrite(PIN_PWR_LATCH, HIGH);
  pinMode(PIN_PWR_KEY, INPUT);
  pinMode(PIN_BOOT, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.printf("hal: psram %u bytes, reset reason %d\n", (unsigned)ESP.getPsramSize(), (int)esp_reset_reason());

  ledcAttach(PIN_BL, 20000, 10);
  ledcWrite(PIN_BL, 0);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  uint8_t v = 0x00;
  i2cWrite(ADDR_EXIO, 0x03, &v, 1);   // all EXIO pins outputs
  v = 0xFF;
  i2cWrite(ADDR_EXIO, 0x01, &v, 1);

  v = 0x00;
  i2cWrite(ADDR_RTC, 0x00, &v, 1);    // PCF85063: running, 24h

  canvasSpr.setColorDepth(16);
  canvasSpr.createSprite(PANEL, PANEL);
  canvasSpr.fillScreen(0);
  dmaBuf = (uint16_t*)heap_caps_malloc(PANEL * CHUNK_ROWS * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  panelInit();
  clearPanel();
  touchInit();

  analogReadResolution(12);
}

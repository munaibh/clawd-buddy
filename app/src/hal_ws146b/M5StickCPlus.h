#pragma once
// Drop-in stand-in for the M5StickCPlus library on the Waveshare
// ESP32-S3-Touch-LCD-1.46B (412x412 round SPD2010 QSPI panel, SPD2010 touch,
// QMI8658 IMU, PCF85063 RTC, PCM5101 I2S DAC, TCA9554 IO expander).
// src/round_ui.h draws on M5.canvas() and calls M5.present(); M5.Lcd is only
// spr's parent and has no buffer. -DWS146B_ROTATE_180 turns display, touch
// and IMU for an upside-down mount (hinged case).

#include <Arduino.h>
#include <esp_mac.h>
#include <LovyanGFX.hpp>

// TFT_eSPI-shaped LovyanGFX sprites. Buffers live in PSRAM: once BLE is up
// there's no 64 KB block of internal DMA RAM left.
class TFT_eSPI : public lgfx::LGFX_Sprite {
 public:
  TFT_eSPI() : lgfx::LGFX_Sprite() { setPsram(true); }
  explicit TFT_eSPI(lgfx::LovyanGFX* parent) : lgfx::LGFX_Sprite(parent) { setPsram(true); }
};

class TFT_eSprite : public TFT_eSPI {
 public:
  explicit TFT_eSprite(TFT_eSPI* parent) : TFT_eSPI(parent) {}
};

#ifndef GREEN
#define GREEN 0x07E0
#endif
#ifndef RED
#define RED 0xF800
#endif

struct RTC_TimeTypeDef {
  uint8_t Hours;
  uint8_t Minutes;
  uint8_t Seconds;
};

struct RTC_DateTypeDef {
  uint8_t  WeekDay;
  uint8_t  Month;
  uint8_t  Date;
  uint16_t Year;
};

class HalButton {
 public:
  void set(bool pressed, uint32_t since);
  bool isPressed() const { return _pressed; }
  bool wasPressed() const { return _pressed && _changed; }
  bool wasReleased() const { return !_pressed && _changed; }
  bool pressedFor(uint32_t ms) const { return _pressed && millis() - _since >= ms; }

 private:
  bool     _pressed = false;
  bool     _changed = false;
  uint32_t _since = 0;
};

class HalImu {
 public:
  void Init();
  void getAccelData(float* ax, float* ay, float* az);
};

class HalAxp {
 public:
  void  ScreenBreath(uint8_t level);   // 0..100
  void  SetLDO2(bool on);              // backlight + panel refresh on/off
  float GetVBusVoltage();              // 5.0 when on USB, else 0
  float GetBatVoltage();
  int   GetBatLevel();                 // 0..100, steady while charging
  float GetBatCurrent() { return 0; }
  float GetTempInAXP192() { return temperatureRead(); }
  uint8_t GetBtnPress();               // 0x02 = short press of PWR key
  void  PowerOff();

  // internal
  uint8_t _btnEvent = 0;
};

class HalRtc {
 public:
  void GetTime(RTC_TimeTypeDef* t);
  void GetDate(RTC_DateTypeDef* d);
  void SetTime(const RTC_TimeTypeDef* t);
  void SetDate(const RTC_DateTypeDef* d);
};

class HalBeep {
 public:
  void begin();
  void tone(uint16_t freq, uint16_t durMs);
  void update() {}
};

class HalM5 {
 public:
  void begin();
  void update();

  TFT_eSPI& canvas();               // 412x412 frame, panel-order RGB565
  void present();                   // copy the canvas to the panel

  bool takeTap(int* x, int* y);     // true once per tap, with where it landed
  // Lift point and duration of the last tap.
  int  tapEndX() const { return _tapEndX; }
  int  tapEndY() const { return _tapEndY; }
  uint32_t tapMs() const { return _tapMs; }
  int  swipe();                     // -1 left, +1 right, 0 none; clears
  bool tapsAsButtons = true;        // also turn taps into A/B presses

  TFT_eSPI  Lcd;
  HalButton BtnA;
  HalButton BtnB;
  HalImu    Imu;
  HalAxp    Axp;
  HalRtc    Rtc;
  HalBeep   Beep;

  bool _tap = false;
  int  _tapX = 0, _tapY = 0, _swipe = 0;
  int  _tapEndX = 0, _tapEndY = 0;
  uint32_t _tapMs = 0;
};

extern HalM5 M5;

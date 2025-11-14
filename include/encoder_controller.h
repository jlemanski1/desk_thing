#pragma once

#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include <lvgl.h>

class EncoderController {
public:
  EncoderController(int pinA, int pinB, int pinSwitch);

  void begin();

  // Get encoder rotation delta
  int getRotationDelta();

  // Check if button was pressed
  bool isButtonPressed();

  // ISR handlers
  void handleRotation();
  void handleButton();

  void setLvglIndevTimer(lv_timer_t* timer) { _indevTimer = timer; }

  // For the read callback to check
  bool checkAndClearInterruptFlag();
  uint32_t getLastInterruptTick() const { return _lastInterruptTick; }
  void updateLastInterruptTick() { _lastInterruptTick = lv_tick_get(); }

private:
  int _pinA;
  int _pinB;
  int _pinSwitch;

  volatile int _encoderDiff;
  volatile int _lastEncoded;
  volatile bool _buttonPressed;
  volatile unsigned long _lastButtonTime;

  lv_timer_t* volatile _indevTimer = nullptr;
  volatile bool _interruptOccurred = false;
  uint32_t _lastInterruptTick = 0;
};

// Global instance for ISR access
extern EncoderController* g_encoder;
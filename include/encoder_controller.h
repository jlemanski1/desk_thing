#pragma once

#include <Arduino.h>
#include "config.h"
#include "debug.h"

class EncoderController {
public:
  EncoderController(int pinA, int pinB, int pinSwitch);

  void begin();

  // Get encoder rotation delta (resets after reading)
  int getRotationDelta();

  // Check if button was pressed (resets after reading)
  bool wasButtonPressed();

  // ISR handlers (must be public to attach)
  void handleRotation();
  void handleButton();

private:
  int _pinA;
  int _pinB;
  int _pinSwitch;

  volatile int _encoderDiff;
  volatile int _lastEncoded;
  volatile bool _buttonPressed;
  volatile unsigned long _lastButtonTime;
};

// Global instance for ISR access
extern EncoderController* g_encoder;
#include "encoder_controller.h"

EncoderController* g_encoder = nullptr;

// ISR wrapper functions
void IRAM_ATTR encoderRotationISR() {
  if (g_encoder) g_encoder->handleRotation();
}

void IRAM_ATTR encoderButtonISR() {
  if (g_encoder) g_encoder->handleButton();
}

EncoderController::EncoderController(int pinA, int pinB, int pinSwitch)
  : _pinA(pinA)
  , _pinB(pinB)
  , _pinSwitch(pinSwitch)
  , _encoderDiff(0)
  , _lastEncoded(0)
  , _buttonPressed(false)
  , _lastButtonTime(0)
{
  g_encoder = this;
}

void EncoderController::begin() {
  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);
  pinMode(_pinSwitch, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(_pinA), encoderRotationISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(_pinB), encoderRotationISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(_pinSwitch), encoderButtonISR, FALLING);

  DebugPrintln("EncoderController: Initialized");
}

int EncoderController::getRotationDelta() {
  int delta = _encoderDiff;
  _encoderDiff = 0;
  return delta;
}

bool EncoderController::wasButtonPressed() {
  bool pressed = _buttonPressed;
  _buttonPressed = false;
  return pressed;
}

/**
 * @brief Interrupt handler for the rotary g_encoder
 */
void IRAM_ATTR EncoderController::handleRotation() {
  int currentA = digitalRead(Pins::ENCODER_A);
  int currentB = digitalRead(Pins::ENCODER_B);

  // Combine the two pin readings into a single encoded value
  int encoded = (currentA << 1) | currentB;

  // Create a sum of previous and current readings
  int sum = (_lastEncoded << 2) | encoded;

  // Determine direction based on the pattern (which pin changes first)
  // This implements a state machine for reliable g_encoder reading
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    _encoderDiff++; // Clockwise rotation
  }
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    _encoderDiff--; // Counter-clockwise rotation
  }

  _lastEncoded = encoded;
}

/**
 * @brief Interrupt handler for the button switch
 */
void IRAM_ATTR EncoderController::handleButton() {
  unsigned long now = millis();

  // Debounce for 200ms
  if (now - _lastButtonTime > Timing::BUTTON_DEBOUNCE_MS) {
    if (digitalRead(Pins::ENCODER_SWITCH) == LOW) {
      _buttonPressed = true;
      _lastButtonTime = now;
    }
  }
}
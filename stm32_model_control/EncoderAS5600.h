#pragma once
#include <AS5600.h>

class EncoderAS5600
{
public:
  EncoderAS5600(TwoWire* wire);

  bool begin();
  void update();

  float getAngle();
  void calibrateZero();

private:
  AS5600 encoder;
  TwoWire* _wire;
  uint16_t lastRaw = 0;

  float offset = 0;

  bool readRaw(uint16_t &rawOut);
};
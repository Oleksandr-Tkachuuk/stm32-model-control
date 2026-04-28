#include "EncoderAS5600.h"

EncoderAS5600::EncoderAS5600(TwoWire* wire) : encoder(wire), _wire(wire) {}

bool EncoderAS5600::begin()
{
  return encoder.begin(AS5600_SW_DIRECTION_PIN);
}

void EncoderAS5600::update()
{
  uint16_t raw;

  if (readRaw(raw))
  {
    lastRaw = raw;
  }
}

float EncoderAS5600::getAngle()
{
  float angle = AS5600_RAW_TO_DEGREES(lastRaw) - offset;

  while (angle > 180.0f) angle -= 360.0f;
  while (angle <= -180.0f) angle += 360.0f;

  return angle;
}

void EncoderAS5600::calibrateZero()
{
  uint32_t sum = 0;
  int validSamples = 0;
  uint16_t tempRaw;

  for (int i = 0; i < 20; i++)
  {
    if (readRaw(tempRaw)) {
        sum += tempRaw;
        validSamples++;
    }
    delay(5);
  }

  if (validSamples > 0) {
    uint16_t avg = sum / validSamples;
    offset = AS5600_RAW_TO_DEGREES(avg);
  }
}

bool EncoderAS5600::readRaw(uint16_t &rawOut)
{
  encoder.readAngle();
  uint16_t raw = encoder.rawAngle();

  if (encoder.lastError() == AS5600_OK)
  {
    rawOut = raw;
    return true;
  }

  return false;
}
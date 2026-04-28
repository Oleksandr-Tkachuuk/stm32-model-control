#include "MotorESC.h"
#include <Arduino.h>

MotorESC::MotorESC(int pin) : pin(pin) {}

void MotorESC::begin()
{
  esc.attach(pin, PWM_MIN, PWM_MAX);
}

void MotorESC::arm()
{
  esc.writeMicroseconds(PWM_MIN);
  delay(3000);
}

void MotorESC::setPower(int percent)
{
  percent = constrain(percent, 0, 100);
  power = percent;

  int pulse = map(percent, 0, 100, PWM_MIN, PWM_MAX);
  esc.writeMicroseconds(pulse);
}

int MotorESC::getPower()
{
  return power;
}
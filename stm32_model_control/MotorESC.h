#pragma once
#include <Servo.h>

class MotorESC
{
public:
  MotorESC(int pin);

  void arm();
  void begin();
  void setPower(int percent);
  int getPower();

private:
  Servo esc;
  int pin;
  int power = 0;

  const int PWM_MIN = 1000;
  const int PWM_MAX = 2000;
};
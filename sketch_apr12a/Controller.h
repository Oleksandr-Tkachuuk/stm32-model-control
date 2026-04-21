#pragma once
#include "MotorESC.h"
#include "EncoderAS5600.h"

class Controller
{
public:
    Controller(MotorESC* m1, MotorESC* m2,
      EncoderAS5600* e1, EncoderAS5600* e2);

    void update();
    void setPower(int p);

    void startAutoTest();
    void startRhythm();
    void calibrateEncoders();

    float getAngle1();
    float getAngle2();

private:
  MotorESC* motor1;
  MotorESC* motor2;

  EncoderAS5600* enc1;
  EncoderAS5600* enc2;

  int power = 0;

  // ===== AUTO TEST =====
  int autoPower = 0;
  bool autoUp = true;
  unsigned long autoLast = 0;

  // ===== RHYTHM =====
  int rhythmIndex = 0;
  unsigned long rhythmLast = 0;
  bool rhythmActive = false;

  const int rhythmPower[8] = { 15,0,15,0,15,0,25,0 };
  const int rhythmTime[8] = { 150,100,150,100,150,100,400,100 };

  unsigned long autoHoldStart = 0;

  enum AutoState
  {
    AUTO_IDLE,
    AUTO_UP,
    AUTO_HOLD,
    AUTO_DOWN
  };

  AutoState autoState = AUTO_IDLE;
};
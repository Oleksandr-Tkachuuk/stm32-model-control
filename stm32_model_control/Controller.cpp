#include "Controller.h"

Controller::Controller(MotorESC* m1, MotorESC* m2,
  EncoderAS5600* e1, EncoderAS5600* e2)
{
  motor1 = m1;
  motor2 = m2;
  enc1 = e1;
  enc2 = e2;
}

void Controller::setPower(int p)
{
  power = constrain(p, 0, 100);

  autoState = AUTO_IDLE;
  rhythmActive = false;

  motor1->setPower(power);
  motor2->setPower(power);

  rhythmActive = false;
}

void Controller::startAutoTest()
{
  autoPower = motor1->getPower();
  autoState = AUTO_UP;
  autoLast = millis();
  rhythmActive = false;
}

void Controller::startRhythm()
{
  rhythmIndex = 0;
  rhythmLast = millis();
  rhythmActive = true;

  motor1->setPower(rhythmPower[0]);
  motor2->setPower(rhythmPower[0]);
}

void Controller::update()
{
  unsigned long now = millis();

  enc1->update();
  enc2->update();

  // === AUTO TEST ===
  if (autoState != AUTO_IDLE)
  {
    if (now - autoLast > 50)
    {
      autoLast = now;

      switch (autoState)
      {
        case AUTO_UP:
          autoPower++;
          if (autoPower >= 100)
          {
            autoPower = 100;
            autoState = AUTO_HOLD;
            autoHoldStart = now;
          }
          break;

        case AUTO_HOLD:
          if (now - autoHoldStart > 3000)
          {
              autoState = AUTO_DOWN;
              autoLast = now;
          }
          break;

        case AUTO_DOWN:
          autoPower--;
          if (autoPower <= 0)
          {
              autoPower = 0;
              autoState = AUTO_IDLE;
          }
          break;

        default:
          break;
      }

      motor1->setPower(autoPower);
      motor2->setPower(autoPower);
    }
  }

  // === RHYTHM ===
  if (rhythmActive)
  {
    if (now - rhythmLast > rhythmTime[rhythmIndex])
    {
      rhythmIndex++;

      if (rhythmIndex >= 8)
      {
        rhythmActive = false;
        motor1->setPower(0);
        motor2->setPower(0);
        return;
      }

      motor1->setPower(rhythmPower[rhythmIndex]);
      motor2->setPower(rhythmPower[rhythmIndex]);

      rhythmLast = now;
    }
  }
}

void Controller::calibrateEncoders()
{
  enc1->calibrateZero();
  enc2->calibrateZero();
}

float Controller::getAngle1()
{
  return enc1->getAngle();
}

float Controller::getAngle2()
{
  return enc2->getAngle();
}

void Controller::setPowerMotor1(int p)
{
    motor1->setPower(constrain(p,0,100));
}

void Controller::setPowerMotor2(int p)
{
    motor2->setPower(constrain(p,0,100));
}
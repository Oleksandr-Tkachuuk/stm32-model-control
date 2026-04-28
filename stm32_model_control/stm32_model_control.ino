#include <Wire.h>
#include <HardwareTimer.h>

#include "MotorESC.h"
#include "EncoderAS5600.h"
#include "Controller.h"

// ===== I2C =====
TwoWire I2C_1(PB9, PB8);
TwoWire I2C_2(PB11, PB10);

// ===== OBJECTS =====
MotorESC motor1(PC9);
MotorESC motor2(PC8);

EncoderAS5600 enc1(&I2C_1);
EncoderAS5600 enc2(&I2C_2);

Controller controller(&motor1, &motor2, &enc1, &enc2);

// ===== TIMER =====
// Sampling is driven by STM32 timer, not by MATLAB/PC time.
// The timer interrupt increments a sample counter once per sampling period.
constexpr float SAMPLE_RATE_HZ = 25.0f;
constexpr float TS_SEC = 1.0f / SAMPLE_RATE_HZ;

HardwareTimer *timer = new HardwareTimer(TIM2);
volatile uint32_t pendingSamples = 0;
volatile unsigned long sampleTick = 0;

// ===== SERIAL =====
char buffer[32];
uint8_t idx = 0;

bool sendData = false;

void setup()
{
    Serial.begin(115200);
    delay(500);

    I2C_1.begin();
    I2C_2.begin();

    I2C_1.setClock(50000);
    I2C_2.setClock(50000);

    delay(200);

    enc1.begin();
    enc2.begin();

    delay(200);

    controller.calibrateEncoders();

    motor1.begin();
    motor2.begin();

    motor1.arm();
    motor2.arm();

    // TIMER
    timer->setOverflow(SAMPLE_RATE_HZ, HERTZ_FORMAT);
    timer->attachInterrupt(onTimer);
    timer->resume();
}

void loop()
{
    // SERIAL (non-blocking)
    while (Serial.available())
    {
        char c = Serial.read();

        if (c == '\n')
        {
            buffer[idx] = '\0';
            processCommand(buffer);
            idx = 0;
        }
        else if (idx < sizeof(buffer)-1)
        {
            buffer[idx++] = c;
        }
    }

    // SAMPLING + STREAMING
    // One outgoing data line is produced only after a timer tick.
    // Format: time_s,motor1_power,motor2_power,encoder1_deg,encoder2_deg
    while (pendingSamples > 0)
    {
        unsigned long tickSnapshot;

        noInterrupts();
        pendingSamples--;
        tickSnapshot = sampleTick;
        interrupts();

        controller.update();

        if (sendData)
        {
            float timeSec = tickSnapshot * TS_SEC;

            Serial.print(timeSec, 4);
            Serial.print(",");
            Serial.print(motor1.getPower());
            Serial.print(",");
            Serial.print(motor2.getPower());
            Serial.print(",");
            Serial.print(controller.getAngle1(), 2);
            Serial.print(",");
            Serial.println(controller.getAngle2(), 2);
        }
    }
}

void onTimer()
{
    sampleTick++;
    pendingSamples++;
}

void processCommand(const char* cmd)
{
    if (cmd == nullptr || cmd[0] == '\0') return;

    char c = tolower(cmd[0]); 

    if (strcmp(cmd, "+") == 0) controller.setPower(motor1.getPower() + 5);
    else if (strcmp(cmd, "-") == 0) controller.setPower(motor1.getPower() - 5);
    else if (c == 's') sendData = true;
    else if (c == 'p') sendData = false;
    else if (c == 'x') controller.startAutoTest();
    else if (c == 'z') controller.startRhythm();
    else if (c == 'c') controller.calibrateEncoders();
    else
    {
        bool isNumeric = true;
        for (int i = 0; cmd[i]; i++)
        {
            if (!isDigit(cmd[i]) && !(i == 0 && (cmd[i]=='+' || cmd[i]=='-')))
            {
                isNumeric = false;
                break;
            }
        }

        if (isNumeric)
        {
            controller.setPower(atoi(cmd));
        }
    }
}
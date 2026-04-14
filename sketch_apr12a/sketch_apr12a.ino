#include <Servo.h>
#include <Wire.h>
#include <AS5600.h>
#include <HardwareTimer.h>

// ====== PINS ======
const int ESC1_PIN = PC9;
const int ESC2_PIN = PC8;

// ====== PWM SETTINGS ======
const int PWM_MIN = 1000;
const int PWM_MAX = 2000;

// ====== POWER SETTINGS ======
const int POWER_MIN = 0;
const int POWER_MAX = 100;

// ====== SYSTEM ======
const int BAUD_RATE = 115200;
const int ARM_DELAY = 3000;

// ====== I2C ======
TwoWire I2C_1(PB9, PB8);    // SDA, SCL
TwoWire I2C_2(PB11, PB10);  // SDA, SCL

// ====== AS5600 OBJECTS ======
AS5600 encoder1(&I2C_1);
AS5600 encoder2(&I2C_2);

// ====== ESC OBJECTS ======
Servo esc1;
Servo esc2;

// ====== TIMER ======
HardwareTimer *timer = new HardwareTimer(TIM2);
const int SAMPLE_FREQ = 25;   // Hz (25 samples per second)

volatile bool sampleFlag = false;

// ====== STATE ======
int currentPower = 0;
bool sendData = false;

float offset1 = 0.0f;
float offset2 = 0.0f;

uint16_t lastRaw1 = 0;
uint16_t lastRaw2 = 0;

bool encoder1Connected = false;
bool encoder2Connected = false;

uint16_t prevRaw1 = 0;
int freezeCounter1 = 0;

// ====== NON-BLOCKING SERIAL RX ======
char serialBuffer[64];
uint8_t serialIndex = 0;

// ====== NON-BLOCKING MODES ======
enum ModeState
{
    MODE_IDLE = 0,
    MODE_AUTOTEST,
    MODE_RHYTHM
};

ModeState activeMode = MODE_IDLE;

// --- Auto test state ---
int autoTestPower = 0;
bool autoTestGoingUp = true;
bool autoTestHolding = false;
unsigned long autoTestLastStepMs = 0;
unsigned long autoTestHoldStartMs = 0;
const unsigned long AUTO_STEP_INTERVAL_MS = 50;
const unsigned long AUTO_HOLD_MS = 3000;

// --- Rhythm state ---
const int RHYTHM_STEPS = 8;
int rhythmIndex = 0;
unsigned long rhythmLastStepMs = 0;
bool rhythmActive = false;

const int rhythmPowerSequence[RHYTHM_STEPS] = {15, 0, 15, 0, 15, 0, 25, 0};
const unsigned long rhythmTimeSequence[RHYTHM_STEPS] = {150, 100, 150, 100, 150, 100, 400, 100};

// ====== FUNCTION DECLARATIONS ======
void armESCs();

void handleSerial();
void processCommand(const char *cmd);

void setPowerAll(int percent);
void setPowerMotor(Servo &esc, int percent);
int powerToPulse(int percent);

void startAutoTest();
void updateAutoTest();

void startRhythm();
void updateRhythm();

void stopActiveMode();

bool readEncoderRawSafe(AS5600 &encoder, uint16_t &rawOut, uint16_t &lastGood, int &errorOut);

void calibrateZero();
void printEncoderStartupInfo();

float rawToDegrees(uint16_t raw);
float normalizeAngle(float angle);

void onTimer();
// =========================

void setup()
{
    Serial.begin(BAUD_RATE);
    Serial.setTimeout(5);
    delay(500);

    I2C_1.begin();
    I2C_2.begin();

    I2C_1.setClock(50000);
    I2C_2.setClock(50000);

    delay(200);

    encoder1Connected = encoder1.begin(AS5600_SW_DIRECTION_PIN);
    encoder2Connected = encoder2.begin(AS5600_SW_DIRECTION_PIN);

    printEncoderStartupInfo();

    delay(200);

    calibrateZero();

    esc1.attach(ESC1_PIN, PWM_MIN, PWM_MAX);
    esc2.attach(ESC2_PIN, PWM_MIN, PWM_MAX);

    armESCs();

    timer->setOverflow(SAMPLE_FREQ, HERTZ_FORMAT);
    timer->attachInterrupt(onTimer);
    timer->resume();
}

void loop()
{
    handleSerial();

    switch (activeMode)
    {
        case MODE_AUTOTEST:
            updateAutoTest();
            break;

        case MODE_RHYTHM:
            updateRhythm();
            break;

        case MODE_IDLE:
        default:
            break;
    }

    if (sampleFlag)
    {
        noInterrupts();
        sampleFlag = false;
        interrupts();

        uint16_t raw1 = 0;
        uint16_t raw2 = 0;
        int err1 = AS5600_OK;
        int err2 = AS5600_OK;

        readEncoderRawSafe(encoder1, raw1, lastRaw1, err1);
        delayMicroseconds(1000);

        if (raw1 == prevRaw1)
        {
            freezeCounter1++;
        }
        else
        {
            freezeCounter1 = 0;
        }

        prevRaw1 = raw1;

        if (freezeCounter1 > 10)
        {
            I2C_1.end();
            I2C_1.begin();
            I2C_1.setClock(50000);
            freezeCounter1 = 0;
        }

        readEncoderRawSafe(encoder2, raw2, lastRaw2, err2);

        if (sendData)
        {
            float angle1 = normalizeAngle(rawToDegrees(raw1) - offset1);
            float angle2 = normalizeAngle(rawToDegrees(raw2) - offset2);

            Serial.print(currentPower);
            Serial.print(",");
            Serial.print(currentPower);
            Serial.print(",");
            Serial.print(angle1, 2);
            Serial.print(",");
            Serial.println(angle2, 2);
        }
    }
}

// =========================
// ESC ARMING
// =========================
void armESCs()
{
    esc1.writeMicroseconds(PWM_MIN);
    esc2.writeMicroseconds(PWM_MIN);
    delay(ARM_DELAY);
}

// =========================
// SERIAL HANDLING (NON-BLOCKING)
// =========================
void handleSerial()
{
    while (Serial.available() > 0)
    {
        char ch = (char)Serial.read();

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            serialBuffer[serialIndex] = '\0';

            if (serialIndex > 0)
            {
                processCommand(serialBuffer);
            }

            serialIndex = 0;
            serialBuffer[0] = '\0';
        }
        else
        {
            if (serialIndex < sizeof(serialBuffer) - 1)
            {
                serialBuffer[serialIndex++] = ch;
            }
            else
            {
                serialIndex = 0;
                serialBuffer[0] = '\0';
            }
        }
    }
}

void processCommand(const char *cmd)
{
    if (cmd == nullptr || cmd[0] == '\0')
    {
        return;
    }

    if (strcmp(cmd, "+") == 0)
    {
        stopActiveMode();
        setPowerAll(currentPower + 5);
    }
    else if (strcmp(cmd, "-") == 0)
    {
        stopActiveMode();
        setPowerAll(currentPower - 5);
    }
    else if (strcmp(cmd, "x") == 0 || strcmp(cmd, "X") == 0)
    {
        startAutoTest();
    }
    else if (strcmp(cmd, "z") == 0 || strcmp(cmd, "Z") == 0)
    {
        startRhythm();
    }
    else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0)
    {
        sendData = true;
    }
    else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "P") == 0)
    {
        sendData = false;
    }
    else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "C") == 0)
    {
        stopActiveMode();
        calibrateZero();
    }
    else
    {
        bool numeric = true;
        uint8_t startIdx = 0;

        if (cmd[0] == '+' || cmd[0] == '-')
        {
            startIdx = 1;
        }

        for (uint8_t i = startIdx; cmd[i] != '\0'; i++)
        {
            if (!isDigit(cmd[i]))
            {
                numeric = false;
                break;
            }
        }

        if (numeric)
        {
            stopActiveMode();
            int value = atoi(cmd);
            setPowerAll(value);
        }
    }
}

// =========================
// MOTOR CONTROL
// =========================
void setPowerAll(int percent)
{
    percent = constrain(percent, POWER_MIN, POWER_MAX);
    currentPower = percent;

    setPowerMotor(esc1, percent);
    setPowerMotor(esc2, percent);
}

void setPowerMotor(Servo &esc, int percent)
{
    int pulse = powerToPulse(percent);
    esc.writeMicroseconds(pulse);
}

int powerToPulse(int percent)
{
    return map(percent, POWER_MIN, POWER_MAX, PWM_MIN, PWM_MAX);
}

// =========================
// AUTO TEST (NON-BLOCKING)
// =========================
void startAutoTest()
{
    activeMode = MODE_AUTOTEST;

    autoTestPower = POWER_MIN;
    autoTestGoingUp = true;
    autoTestHolding = false;
    autoTestLastStepMs = millis();
    autoTestHoldStartMs = 0;

    setPowerAll(autoTestPower);
}

void updateAutoTest()
{
    unsigned long now = millis();

    if (autoTestHolding)
    {
        if (now - autoTestHoldStartMs >= AUTO_HOLD_MS)
        {
            autoTestHolding = false;
            autoTestGoingUp = false;
            autoTestLastStepMs = now;
        }
        return;
    }

    if (now - autoTestLastStepMs < AUTO_STEP_INTERVAL_MS)
    {
        return;
    }

    autoTestLastStepMs = now;

    if (autoTestGoingUp)
    {
        if (autoTestPower < POWER_MAX)
        {
            autoTestPower++;
            setPowerAll(autoTestPower);
        }
        else
        {
            autoTestHolding = true;
            autoTestHoldStartMs = now;
        }
    }
    else
    {
        if (autoTestPower > POWER_MIN)
        {
            autoTestPower--;
            setPowerAll(autoTestPower);
        }
        else
        {
            activeMode = MODE_IDLE;
        }
    }
}

// =========================
// RHYTHM DEMO (NON-BLOCKING)
// =========================
void startRhythm()
{
    activeMode = MODE_RHYTHM;
    rhythmIndex = 0;
    rhythmLastStepMs = millis();
    rhythmActive = true;

    setPowerAll(rhythmPowerSequence[rhythmIndex]);
}

void updateRhythm()
{
    if (!rhythmActive)
    {
        activeMode = MODE_IDLE;
        return;
    }

    unsigned long now = millis();

    if (now - rhythmLastStepMs >= rhythmTimeSequence[rhythmIndex])
    {
        rhythmIndex++;

        if (rhythmIndex >= RHYTHM_STEPS)
        {
            rhythmActive = false;
            setPowerAll(0);
            activeMode = MODE_IDLE;
            return;
        }

        setPowerAll(rhythmPowerSequence[rhythmIndex]);
        rhythmLastStepMs = now;
    }
}

void stopActiveMode()
{
    activeMode = MODE_IDLE;
    rhythmActive = false;
}

// =========================
// ENCODER FUNCTIONS
// =========================
bool readEncoderRawSafe(AS5600 &encoder, uint16_t &rawOut, uint16_t &lastGood, int &errorOut)
{
    for (int i = 0; i < 3; i++)
    {
        encoder.readAngle();
        uint16_t raw = encoder.rawAngle();
        int err = encoder.lastError();

        if (err == AS5600_OK)
        {
            rawOut = raw;
            lastGood = raw;
            errorOut = err;
            return true;
        }
    }

    rawOut = lastGood;
    errorOut = encoder.lastError();
    return false;
}

void calibrateZero()
{
    uint16_t raw1 = 0;
    uint16_t raw2 = 0;
    int err1 = AS5600_OK;
    int err2 = AS5600_OK;

    bool ok1 = readEncoderRawSafe(encoder1, raw1, lastRaw1, err1);
    bool ok2 = readEncoderRawSafe(encoder2, raw2, lastRaw2, err2);

    if (ok1)
    {
        offset1 = rawToDegrees(raw1);
    }

    if (ok2)
    {
        offset2 = rawToDegrees(raw2);
    }
}

void printEncoderStartupInfo()
{
    // Silent on purpose for clean MATLAB stream
}

// =========================
// MATH & INTERRUPTS
// =========================
float rawToDegrees(uint16_t raw)
{
    return (raw * 360.0f) / 4096.0f;
}

float normalizeAngle(float angle)
{
    while (angle > 180.0f) angle -= 360.0f;
    while (angle <= -180.0f) angle += 360.0f;
    return angle;
}

void onTimer()
{
    sampleFlag = true;
}
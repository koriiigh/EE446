#include <PDM.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

short sampleBuffer[256];
volatile int samplesRead = 0;
volatile int micLevel = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  int count = bytesAvailable / 2;

  long sum = 0;
  for (int i = 0; i < count; i++) {
    sum += abs(sampleBuffer[i]);
  }
  if (count > 0) {
    micLevel = sum / count;
  }
  samplesRead = count;
}

const int SOUND_THRESHOLD = 200;
const int DARK_THRESHOLD = 50;
const float MOTION_THRESHOLD = 0.15;
const int NEAR_THRESHOLD = 20;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
    while (1);
  }

  Serial.println("Workspace situation classifier started");
}

void loop() {
  static int clearVal  = 0;
  static int proximity = 0;
  static float motion    = 0.0;

  int mic = micLevel;

  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    clearVal = c;
  }

  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
  }

  float ax, ay, az;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    float mag = sqrt(ax * ax + ay * ay + az * az);
    motion = fabs(mag - 1.0);
  }

  bool isSound = (mic > SOUND_THRESHOLD);
  bool isDark = (clearVal < DARK_THRESHOLD);
  bool isMoving = (motion > MOTION_THRESHOLD);
  bool isNear = (proximity > NEAR_THRESHOLD);

  String label;
  if (isMoving && isNear) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  } else if (isDark && isNear) {
    label = "QUIET_DARK_STEADY_NEAR";
  } else if (isSound && !isNear) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  } else {
    label = "QUIET_BRIGHT_STEADY_FAR";
  }

  Serial.print("mic="); Serial.print(mic);
  Serial.print(" clear="); Serial.print(clearVal);
  Serial.print(" motion="); Serial.print(motion, 3);
  Serial.print(" prox="); Serial.println(proximity);

  Serial.print("sound="); Serial.print(isSound);
  Serial.print(" dark="); Serial.print(isDark);
  Serial.print(" moving="); Serial.print(isMoving);
  Serial.print(" near="); Serial.println(isNear);

  Serial.print("FINAL_LABEL="); Serial.println(label);
  Serial.println();

  delay(300);
}

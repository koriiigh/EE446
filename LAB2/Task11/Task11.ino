#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

const float HUMID_JUMP_THRESHOLD = 8.0;
const float TEMP_RISE_THRESHOLD = 0.5;
const float MAG_SHIFT_THRESHOLD = 40.0;
const int   LIGHT_CHANGE_THRESHOLD = 40;

const float BASELINE_ALPHA = 0.02;

const unsigned long COOLDOWN_MS = 2000;
unsigned long lastEventTime = 0;

const unsigned long WARMUP_MS = 3000;
unsigned long startTime = 0;

float baseRH = 0, baseTemp = 0, baseMag = 0;
float baseR = 0, baseG = 0, baseB = 0, baseClear = 0;
bool  baseInit = false;

bool haveColor = false;
bool haveMag = false;

float mx = 0, my = 0, mz = 0;
int rr = 0, gg = 0, bb = 0, cc = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize humidity/temperature sensor.");
    while (1);
  }
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU/magnetometer.");
    while (1);
  }
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  Serial.println("Environmental event detector started");
}

void loop() {
  float rh = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
    haveMag = true;
  }
  float mag = sqrt(mx * mx + my * my + mz * mz);

  if (APDS.colorAvailable()) {
    APDS.readColor(rr, gg, bb, cc);
    haveColor = true;
  }

  if (!baseInit) {
    if (haveColor && haveMag) {
      baseRH = rh; baseTemp = temp; baseMag = mag;
      baseR = rr; baseG = gg; baseB = bb; baseClear = cc;
      baseInit = true;
      startTime = millis();
    }
    delay(250);
    return;
  }

  bool humid_jump = (rh - baseRH) > HUMID_JUMP_THRESHOLD;
  bool temp_rise = (temp - baseTemp) > TEMP_RISE_THRESHOLD;
  bool mag_shift = fabs(mag - baseMag) > MAG_SHIFT_THRESHOLD;

  bool light_or_color_change =
        (abs(cc - (int)baseClear) > LIGHT_CHANGE_THRESHOLD) ||
        (abs(rr - (int)baseR) > LIGHT_CHANGE_THRESHOLD) ||
        (abs(gg - (int)baseG) > LIGHT_CHANGE_THRESHOLD) ||
        (abs(bb - (int)baseB) > LIGHT_CHANGE_THRESHOLD);

  if (!(humid_jump || temp_rise || mag_shift || light_or_color_change)) {
    baseRH += BASELINE_ALPHA * (rh - baseRH);
    baseTemp += BASELINE_ALPHA * (temp - baseTemp);
    baseMag += BASELINE_ALPHA * (mag - baseMag);
    baseR += BASELINE_ALPHA * (rr - baseR);
    baseG += BASELINE_ALPHA * (gg - baseG);
    baseB += BASELINE_ALPHA * (bb - baseB);
    baseClear += BASELINE_ALPHA * (cc - baseClear);
  }

  String rawEvent = "BASELINE_NORMAL";
  if (mag_shift) {
    rawEvent = "MAGNETIC_DISTURBANCE_EVENT";
  } else if (humid_jump || temp_rise) {
    rawEvent = "BREATH_OR_WARM_AIR_EVENT";
  } else if (light_or_color_change) {
    rawEvent = "LIGHT_OR_COLOR_CHANGE_EVENT";
  }

  unsigned long now = millis();
  bool warmedUp = (now - startTime) > WARMUP_MS;

  String label = warmedUp ? rawEvent : "BASELINE_NORMAL";
  bool freshTrigger = false;

  if (rawEvent != "BASELINE_NORMAL" && warmedUp &&
      (now - lastEventTime) > COOLDOWN_MS) {
    freshTrigger = true;
    lastEventTime = now;
  }

  Serial.print("rh="); Serial.print(rh, 1);
  Serial.print(" temp="); Serial.print(temp, 1);
  Serial.print(" mag="); Serial.print(mag, 1);
  Serial.print(" r="); Serial.print(rr);
  Serial.print(" g="); Serial.print(gg);
  Serial.print(" b="); Serial.print(bb);
  Serial.print(" clear="); Serial.println(cc);

  Serial.print("humid_jump="); Serial.print(humid_jump);
  Serial.print(" temp_rise="); Serial.print(temp_rise);
  Serial.print(" mag_shift="); Serial.print(mag_shift);
  Serial.print(" light_or_color_change="); Serial.println(light_or_color_change);

  Serial.print("FINAL_LABEL="); Serial.print(label);

  Serial.println();

  delay(250);
}

// Arduino Nano ESP32
// IDE Board version 2.0.18-20240930.arduino3   

#include <dummy.h>
#include <WiFi.h>
#include <WiFiUdp.h>

WiFiUDP udp;

char packetBuffer[255];
unsigned int localPort = 8000;
char SERVER_IP[] = "your.IP.address.xxx";            // insert your IP address
unsigned int SERVER_PORT = 8000;
const char *ssid = "yourWiFiAddress";                // WiFi parameters as string
const char *password = "yourWiFiPassword";

float battery_power;
const unsigned long dataFetch = 500;        // milliseconds between each UDP data fetch
unsigned long lastDataFetch;                // time in millisecs of last UDP data fetch

const unsigned long readAC_interval = 500;  // microseconds between each sample of 50Hz sample
unsigned long lastACread;
const unsigned long averageAC_interval = 1000;  // milliseconds averaging interval
unsigned long lastACaverage;
int pinACread = A0;
int pinACout = A3;
const uint32_t freq = 5000;                 // DAC PMW frequency
const int resolution = 12;                  // DAC PMW resolution

float scale = 0.0001695;                    // scale for 12 bit (i.e. 4096 counts) on 0 - 3.3V  span          
float offset = 1241.2;
int AC_ref_mid = 1986;                      // midpoint will be updated during operation so less critical here.
int AC_ref_mid_est = 0;
float AC_ref_accumated = 0.;
float calibration_factor = 1.65;            // adjust using calibration tools

void setup() {
  Serial.begin(115200);
  // Connect to Wifi network.
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  udp.begin(localPort);
  Serial.printf("UDP Client : %s:%i \n", WiFi.localIP().toString().c_str(), localPort);

  ledcAttach(pinACout, freq, resolution);

  lastDataFetch = millis();
  lastACread = micros();
  lastACaverage = millis();
}

void request_data() {
  udp.beginPacket(SERVER_IP, SERVER_PORT);
  char buf[] = "battery power";
  udp.printf(buf);
  udp.endPacket();
  lastDataFetch = millis();
}


void loop() {
  if ((millis() - lastDataFetch) > dataFetch) {
    for(int i = 0; i < 20; ++i)
    {
        packetBuffer[i] = (char)0;
    }
    request_data();
    int packetSize = udp.parsePacket();
    if (packetSize) {
      int len = udp.read(packetBuffer, 255);
      battery_power = atof(packetBuffer);
    }
  }

  if ((micros() - lastACread) > readAC_interval) {
    float AC_reference = analogRead(pinACread);
    AC_ref_accumated = AC_ref_accumated + AC_reference;
    int CT = calibration_factor * (AC_reference - AC_ref_mid) * battery_power * scale + offset;
    analogWrite(pinACout, CT);
    lastACread = micros();
  }

  if ((millis() - lastACaverage) > averageAC_interval) {
    AC_ref_mid = (AC_ref_accumated*readAC_interval)/(averageAC_interval*1000);
    AC_ref_accumated = 0.;
    //Serial.println(battery_power);
    lastACaverage = millis();
  }
}

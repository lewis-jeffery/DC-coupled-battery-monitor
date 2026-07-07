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






#include <dummy.h>

// https://www.aranacorp.com/en/communication-between-two-esp32s-via-udp/
// https://www.circuitstate.com/tutorials/how-to-write-parallel-multitasking-applications-for-esp32-using-freertos-arduino/

// v1 Establish UDP link and echo Battery Current to screen
// v2 First basic PWM
// v3 Monitoring while attached to Zappi and set calibration factor
// v4 Timing estimates
// v5 Mean of AC_ref and clear packetBuffer before UDP call.
// v6 Read battery_power instead of battery_current
// v7 Set time by NTP for automatic reboot every day at 0400 local time
// v8 Check WiFi and reconnect if required.  https://www.electronicwings.com/esp32/reconnect-esp32-to-wi-fi-network-after-lost-connection
// v9 Combined WiFi reconnect + roam to strongest AP on our SSID.
//    Scans 2.4 GHz channels 1, 6, 11 only. Single periodic check handles
//    both reconnection (if down) and roaming (if a stronger AP exists).
// v10 Optional debug instrumentation with timing.  Define WIFI_DEBUG and/or
//    DATA_FETCH_DEBUG below to enable comprehensive timestamped messages for
//    each subsystem independently.

// ===== DEBUG SWITCHES =====
// Comment out a line to silence that debug channel; leave it defined to enable.
// Each is independent so you can profile just one subsystem at a time. All
// timing is in microseconds via micros(); elapsed deltas print for each
// major operation. When a switch is off its messages compile away entirely
// (zero runtime cost, no unused-variable warnings).
#define WIFI_DEBUG        // scan timing, per-channel results, roam decisions, assoc time
#define DATA_FETCH_DEBUG  // UDP request/parse round-trip timing and payload

// --- Debug helper macros (compile to nothing when the switch is undefined) ---
#ifdef WIFI_DEBUG
  #define WDBG(...)    Serial.printf(__VA_ARGS__)
  #define WDBG_TS(tag) Serial.printf("[WIFI %lu us] " tag "\n", (unsigned long)micros())
#else
  #define WDBG(...)
  #define WDBG_TS(tag)
#endif

#ifdef DATA_FETCH_DEBUG
  #define DDBG(...)    Serial.printf(__VA_ARGS__)
  #define DDBG_TS(tag) Serial.printf("[DATA %lu us] " tag "\n", (unsigned long)micros())
#else
  #define DDBG(...)
  #define DDBG_TS(tag)
#endif
// ==========================

#include <WiFi.h>
#include <WiFiUdp.h>
#include "time.h"

WiFiUDP udp;

// --- WiFi management ---
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 60000;  // How often to check link / scan for better AP (60 s)
const int roamThreshold = 8;                     // Only switch if candidate is >8 dBm stronger (hysteresis)
const int minConnectRSSI = -80;                  // If current signal weaker than this, roam more readily

// Chosen AP from the most recent scan (module scope).
uint8_t gBestBSSID[6];
int32_t gBestChannel;

char packetBuffer[255];
unsigned int localPort = 8000;
char SERVER_IP[] = "10.0.1.69";
unsigned int SERVER_PORT = 8000;
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
const char *ntpServer = "pool.ntp.org";
String timezone = "AEST-10AEDT,M10.1.0,M4.1.0/3";  // Set for Sydney

const int8_t Reboot_timeHour = 04;  // hours since midnight - [ 0 to 23 ]
const int8_t Reboot_timeMin = 00;

float battery_power;
const unsigned long dataFetch = 100;  // milliseconds between each UDP data fetch
unsigned long lastDataFetch;          // time in millisecs of last UDP data fetch

const unsigned long readAC_interval = 500;  // microseconds between each sample of 50Hz sample
unsigned long lastACread;
const unsigned long averageAC_interval = 1000;  // milliseconds averaging interval
unsigned long lastACaverage;
int pinACread = A0;
int pinACout = A3;
const uint32_t freq = 5000;  // DAC PMW frequency
const int resolution = 12;   // DAC PMW resolution

float scale = 0.0001695;
float offset = 1241.2;
int AC_ref_mid = 1986;
int AC_ref_mid_est = 0;
float AC_ref_accumated = 0.;
float calibration_factor = 1.65;

// Scan 2.4 GHz channels 1, 6, 11 only and either (re)connect to the strongest
// AP on our SSID, or roam to a materially stronger one if already connected.
void manageWiFi() {
  WDBG_TS("manageWiFi ENTER");
  [[maybe_unused]] unsigned long fnStart = micros();

  bool connected = (WiFi.status() == WL_CONNECTED);

  int currentRSSI = -999;
  uint8_t currentBSSID[6] = {0};
  if (connected) {
    currentRSSI = WiFi.RSSI();
    memcpy(currentBSSID, WiFi.BSSID(), 6);
    WDBG("[WIFI] connected, current RSSI %d dBm, BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
         currentRSSI, currentBSSID[0], currentBSSID[1], currentBSSID[2],
         currentBSSID[3], currentBSSID[4], currentBSSID[5]);
  } else {
    WDBG("[WIFI] not connected, will attempt to associate\n");
  }

  int bestRSSI = connected ? currentRSSI : -999;
  bool found = false;

  // Ensure the radio is in a clean, scannable state. When the STA is not
  // connected, the core keeps retrying the last AP in the background, and that
  // in-progress connect makes esp_wifi_scan_start return WIFI_SCAN_FAILED (-2):
  // "wifi still connecting" (see espressif/arduino-esp32 #8916). Aborting the
  // pending connect with disconnect() frees the driver to scan. We only do this
  // when NOT connected, so an active link is never dropped just to scan.
  WiFi.scanDelete();
  if (WiFi.getMode() != WIFI_STA) {
    WDBG("[WIFI] mode was %d, forcing WIFI_STA\n", WiFi.getMode());
    WiFi.mode(WIFI_STA);
  }
  if (!connected) {
    WiFi.disconnect();  // cancel background reconnect so the scan can start
    delay(50);
  }

  // Passive scan restricted to the 2.4 GHz channels actually in use (1, 6, 11).
  // Signature: scanNetworks(async, show_hidden, passive, max_ms_per_chan, channel)
  const int channels[] = {1, 6, 11};
  [[maybe_unused]] unsigned long scanStart = micros();
  for (int c = 0; c < 3; ++c) {
    int ch = channels[c];
    [[maybe_unused]] unsigned long chStart = micros();
    int n = WiFi.scanNetworks(false, false, false, 120, ch);
    [[maybe_unused]] unsigned long chElapsed = micros() - chStart;
    // Negative return = scan failed/busy (WIFI_SCAN_FAILED == -2,
    // WIFI_SCAN_RUNNING == -1), NOT a count of zero networks. Treat it as
    // a failed channel: clear scan state so the next call can start fresh.
    if (n < 0) {
      WDBG("[WIFI] ch %2d scan FAILED (rc %d) in %lu us\n", ch, n, chElapsed);
      WiFi.scanDelete();
      continue;
    }
    WDBG("[WIFI] ch %2d scan: %d nets in %lu us\n", ch, n, chElapsed);
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i) != String(ssid)) continue;
      // Skip the AP we're already on
      if (connected && memcmp(WiFi.BSSID(i), currentBSSID, 6) == 0) {
        WDBG("[WIFI]   match on ch %d RSSI %d = current AP (skipped)\n", ch, WiFi.RSSI(i));
        continue;
      }
      WDBG("[WIFI]   candidate ch %d RSSI %d BSSID %s\n",
           ch, WiFi.RSSI(i), WiFi.BSSIDstr(i).c_str());
      if (WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        memcpy(gBestBSSID, WiFi.BSSID(i), 6);
        gBestChannel = WiFi.channel(i);
        found = true;
      }
    }
    WiFi.scanDelete();  // free results for this channel before the next
  }
  [[maybe_unused]] unsigned long scanElapsed = micros() - scanStart;
  WDBG("[WIFI] total scan (3 ch) took %lu us\n", scanElapsed);

  if (!found) {
    if (!connected) Serial.println("No AP found for our SSID");
    WDBG("[WIFI] no better AP found; best stays %d dBm\n", bestRSSI);
    WDBG("[WIFI %lu us] manageWiFi EXIT (no action), fn total %lu us\n",
         (unsigned long)micros(), (unsigned long)(micros() - fnStart));
    return;
  }

  // Connect if down; roam only if improvement clears hysteresis (relaxed when weak).
  int required = (!connected || currentRSSI < minConnectRSSI) ? 0 : roamThreshold;
  WDBG("[WIFI] best candidate %d dBm, delta %d dBm, threshold %d dBm\n",
       bestRSSI, bestRSSI - currentRSSI, required);
  if (!connected || (bestRSSI - currentRSSI) > required) {
    Serial.printf("%s to BSSID %02x:%02x:%02x:%02x:%02x:%02x ch %d RSSI %d\n",
                  connected ? "Roaming" : "Connecting",
                  gBestBSSID[0], gBestBSSID[1], gBestBSSID[2],
                  gBestBSSID[3], gBestBSSID[4], gBestBSSID[5],
                  gBestChannel, bestRSSI);
    [[maybe_unused]] unsigned long assocStart = micros();
    WDBG_TS("assoc BEGIN");
    if (connected) WiFi.disconnect();
    WiFi.begin(ssid, password, gBestChannel, gBestBSSID);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
      delay(100);
      Serial.print(F("."));
    }
    Serial.println();
    [[maybe_unused]] unsigned long assocElapsed = micros() - assocStart;
    WDBG("[WIFI] assoc END: %s in %lu us\n",
         WiFi.status() == WL_CONNECTED ? "CONNECTED" : "TIMEOUT", assocElapsed);
  } else {
    WDBG("[WIFI] staying on current AP (improvement below threshold)\n");
  }

  WDBG("[WIFI %lu us] manageWiFi EXIT, fn total %lu us\n",
       (unsigned long)micros(), (unsigned long)(micros() - fnStart));
}

void setup() {
  Serial.begin(115200);
  // Nano ESP32 uses native USB-CDC: Serial only exists once the host opens the
  // port. Without this wait, everything printed during the first (blocking) WiFi
  // scan is discarded, which looks like "no output at all" when WIFI_DEBUG is on.
  while (!Serial && millis() < 3000) { delay(10); }
  delay(200);
  Serial.println("\n\n=== Boot v10 ===");
  Serial.flush();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);   // clear any retained association from a prior session
  delay(100);

  // Initial connection to the best available AP.
  while (WiFi.status() != WL_CONNECTED) {
    manageWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Retrying connect...");
      delay(1000);
    }
  }

  Serial.println("Contacting time server");
  initTime(timezone);

  udp.begin(localPort);
  Serial.printf("UDP Client : %s:%i \n", WiFi.localIP().toString().c_str(), localPort);

  ledcAttach(pinACout, freq, resolution);

  lastDataFetch = millis();
  lastACread = micros();
  lastACaverage = millis();
  lastWiFiCheck = millis();
}

void request_data() {
  udp.beginPacket(SERVER_IP, SERVER_PORT);
  char buf[] = "battery power";
  udp.printf(buf);
  udp.endPacket();
  lastDataFetch = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // Single combined WiFi check: reconnect if down, roam if a better AP exists.
  if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
    manageWiFi();
    lastWiFiCheck = currentMillis;
  }

  if ((currentMillis - lastDataFetch) > dataFetch) {
    DDBG_TS("fetch cycle ENTER");
    [[maybe_unused]] unsigned long fetchStart = micros();

    for (int i = 0; i < 20; ++i) {
      packetBuffer[i] = (char)0;
    }
    //printLocalTime();
    //Serial.printf("UDP Client : %s:%i \n", WiFi.localIP().toString().c_str(), localPort);

    [[maybe_unused]] unsigned long reqStart = micros();
    request_data();
    DDBG("[DATA] request_data() sent in %lu us\n", (unsigned long)(micros() - reqStart));

    [[maybe_unused]] unsigned long parseStart = micros();
    int packetSize = udp.parsePacket();
    [[maybe_unused]] unsigned long parseElapsed = micros() - parseStart;
    if (packetSize) {
      int len = udp.read(packetBuffer, 255);
      battery_power = atof(packetBuffer);
      //Serial.print("packet returned: ");
      //Serial.println(battery_power);
      DDBG("[DATA] rx %d bytes in %lu us, battery_power = %.3f\n",
           len, parseElapsed, battery_power);
    } else {
      DDBG("[DATA] no packet this cycle (parsePacket %lu us)\n", parseElapsed);
    }
    rebootIfTime();

    DDBG("[DATA %lu us] fetch cycle EXIT, total %lu us\n",
         (unsigned long)micros(), (unsigned long)(micros() - fetchStart));
  }

  if ((micros() - lastACread) > readAC_interval) {
    float AC_reference = analogRead(pinACread);
    AC_ref_accumated = AC_ref_accumated + AC_reference;
    int CT = calibration_factor * (AC_reference - AC_ref_mid) * battery_power * scale + offset;
    analogWrite(pinACout, CT);
    lastACread = micros();
  }

  if ((currentMillis - lastACaverage) > averageAC_interval) {
    AC_ref_mid = (AC_ref_accumated * readAC_interval) / (averageAC_interval * 1000);
    AC_ref_accumated = 0.;
    //Serial.println(battery_power);
    lastACaverage = millis();
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void rebootIfTime() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  if (timeinfo.tm_hour == Reboot_timeHour && timeinfo.tm_min == Reboot_timeMin) {
    Serial.println("Restarting in 20 seconds");
    delay(20000);
    ESP.restart();
  }
}

void initTime(String timezone) {
  struct tm timeinfo;

  Serial.println("Setting up time");
  configTime(0, 0, ntpServer);  // First connect to NTP server, with 0 TZ offset
  if (!getLocalTime(&timeinfo)) {
    Serial.println("  Failed to obtain time");
    return;
  }

  Serial.println("  Got the time from NTP");
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}


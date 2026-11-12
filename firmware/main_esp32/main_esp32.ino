/*
  Raksha — ESP32 main-controller starter
  Hardware: ESP32 DevKit + A9G GSM/GPS breakout

  GPIO16 = UART2 RX (connect to A9G TXD; level compatibility must be checked)
  GPIO17 = UART2 TX (connect to A9G RXD; level compatibility must be checked)
  GPIO27 = intrusion input (PIR/door contact)
  GPIO26 = siren-driver output (MOSFET/transistor input ONLY)

  This sketch sends an SMS on a debounced intrusion input and includes
  GPS coordinates if an RMC fix is received. It does not place calls to
  emergency services and does not actuate a door lock.
*/

#include <Arduino.h>

static constexpr int A9G_RX_PIN = 16;
static constexpr int A9G_TX_PIN = 17;
static constexpr int INTRUSION_PIN = 27;
static constexpr int SIREN_PIN = 26;

static constexpr uint32_t MODEM_BAUD = 115200; // Change to match your board firmware.
static constexpr uint32_t ALERT_COOLDOWN_MS = 60000;
static constexpr uint32_t INPUT_DEBOUNCE_MS = 60;
static constexpr uint32_t GPS_STALE_MS = 120000;

HardwareSerial Modem(2);

#ifndef RAKSHA_ALERT_PHONE
#define RAKSHA_ALERT_PHONE "+91XXXXXXXXXX"
#endif

String modemLine;
String gpsLatitude;
String gpsLongitude;
uint32_t lastGpsFixMs = 0;
uint32_t lastAlertMs = 0;
uint32_t candidateChangedMs = 0;
bool candidateInput = false;
bool stableInput = false;
bool alarmActive = false;

String coordinateFromNmea(const String &field, const String &hemisphere, bool isLatitude) {
  if (field.length() < (isLatitude ? 4 : 5) || hemisphere.length() != 1) return "";
  int dot = field.indexOf('.');
  if (dot < 0) return "";
  int degreeDigits = isLatitude ? 2 : 3;
  if (dot < degreeDigits + 2) return "";
  double degrees = field.substring(0, degreeDigits).toDouble();
  double minutes = field.substring(degreeDigits).toDouble();
  double decimal = degrees + minutes / 60.0;
  if (hemisphere == "S" || hemisphere == "W") decimal = -decimal;
  return String(decimal, 6);
}

bool parseRmc(const String &line) {
  if (!(line.startsWith("$GPRMC") || line.startsWith("$GNRMC"))) return false;
  String fields[12];
  int fieldCount = 0;
  int start = 0;
  for (int i = 0; i <= line.length() && fieldCount < 12; ++i) {
    if (i == line.length() || line[i] == ',' || line[i] == '*') {
      fields[fieldCount++] = line.substring(start, i);
      start = i + 1;
      if (i < line.length() && line[i] == '*') break;
    }
  }
  // RMC: 0 sentence, 1 time, 2 status, 3 lat, 4 N/S, 5 lon, 6 E/W
  if (fieldCount < 7 || fields[2] != "A") return false;
  String lat = coordinateFromNmea(fields[3], fields[4], true);
  String lon = coordinateFromNmea(fields[5], fields[6], false);
  if (lat.isEmpty() || lon.isEmpty()) return false;
  gpsLatitude = lat;
  gpsLongitude = lon;
  lastGpsFixMs = millis();
  return true;
}

void sendAT(const String &command, uint32_t waitMs = 250) {
  Modem.print(command);
  Modem.print("\r\n");
  uint32_t started = millis();
  while (millis() - started < waitMs) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      if (c == '\n') {
        if (modemLine.length()) Serial.println("[A9G] " + modemLine);
        modemLine = "";
      } else if (c != '\r' && modemLine.length() < 220) {
        modemLine += c;
      }
    }
    delay(1);
  }
}

void sendSms(const String &body) {
  Serial.println("[SMS] Sending alert...");
  Modem.println("AT+CMGF=1");
  delay(300);
  Modem.print("AT+CMGS=\"");
  Modem.print(RAKSHA_ALERT_PHONE);
  Modem.println("\"");
  delay(500);
  Modem.print(body);
  Modem.write(26); // Ctrl+Z terminates SMS body.
  delay(5000);
  Serial.println("[SMS] Submit command sent; verify modem response and delivery.");
}

String alertText() {
  String msg = "RAKSHA ALERT: Intrusion input triggered. Verify immediately.";
  if (millis() - lastGpsFixMs < GPS_STALE_MS && gpsLatitude.length() && gpsLongitude.length()) {
    msg += " Location: https://maps.google.com/?q=" + gpsLatitude + "," + gpsLongitude;
  } else {
    msg += " GPS fix unavailable or stale.";
  }
  return msg;
}

void drainModem() {
  while (Modem.available()) {
    char c = (char)Modem.read();
    if (c == '\n') {
      if (modemLine.length()) {
        if (modemLine.startsWith("$GP") || modemLine.startsWith("$GN")) parseRmc(modemLine);
        Serial.println("[A9G] " + modemLine);
      }
      modemLine = "";
    } else if (c != '\r' && modemLine.length() < 220) {
      modemLine += c;
    }
  }
}

void handleIntrusion(bool active) {
  if (!active) {
    alarmActive = false;
    digitalWrite(SIREN_PIN, LOW);
    return;
  }

  alarmActive = true;
  digitalWrite(SIREN_PIN, HIGH);

  if (lastAlertMs == 0 || millis() - lastAlertMs >= ALERT_COOLDOWN_MS) {
    sendSms(alertText());
    lastAlertMs = millis();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(INTRUSION_PIN, INPUT_PULLUP);
  pinMode(SIREN_PIN, OUTPUT);
  digitalWrite(SIREN_PIN, LOW);

  Modem.begin(MODEM_BAUD, SERIAL_8N1, A9G_RX_PIN, A9G_TX_PIN);
  delay(1200);
  Serial.println("\nRaksha main controller starting");

  sendAT("AT", 500);
  sendAT("ATE0", 250);
  sendAT("AT+CMGF=1", 250);

  // A9G firmware variants differ. Enable the NMEA output command appropriate
  // to your board/firmware after confirming it in that board's AT manual.
  // For example, some firmware uses AT+GPS=1 and/or AT+GPSRD=1.
  Serial.println("Enable GPS NMEA output using the command documented for your A9G firmware.");
}

void loop() {
  drainModem();

  bool raw = digitalRead(INTRUSION_PIN) == LOW; // Active-low input with INPUT_PULLUP.
  if (raw != candidateInput) {
    candidateInput = raw;
    candidateChangedMs = millis();
  }

  if (millis() - candidateChangedMs >= INPUT_DEBOUNCE_MS && stableInput != candidateInput) {
    stableInput = candidateInput;
    handleIntrusion(stableInput);
  }

  delay(2);
}

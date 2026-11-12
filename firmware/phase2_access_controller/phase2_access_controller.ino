/*
  Raksha Phase 2 — ESP32 LAN access controller

  Receives authenticated access events from the Raspberry Pi face service.
  This is a prototype interface. Keep relay disconnected during initial tests.

  GPIO33 -> relay/MOSFET driver input (not lock directly)
  GPIO27 -> intrusion input (active low)
  GPIO26 -> siren driver input (optional)
  Wi-Fi and API secret are local build-time settings; never commit real values.

  Door strike/lock needs its own correctly rated driver and supply, manual key
  override and safe egress. Relay state defaults OFF after reset.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define RAKSHA_WIFI_SSID "CHANGE_ME"
#define RAKSHA_WIFI_PASSWORD "CHANGE_ME"
#define RAKSHA_API_TOKEN "REPLACE_WITH_LONG_RANDOM_SECRET"

static const int LOCK_DRIVER_PIN = 33;
static const int INTRUSION_PIN = 27;
static const int SIREN_DRIVER_PIN = 26;
static const uint32_t UNLOCK_PULSE_MS = 2500;
static const uint32_t ALERT_COOLDOWN_MS = 60000;
static const bool LOCK_ACTIVE_HIGH = true;

WebServer server(80);
uint32_t unlockUntil = 0;
uint32_t lastIntrusionAlert = 0;
bool intrusionWasActive = false;
String lastEventId = "";

void setLockedOutput() {
  digitalWrite(LOCK_DRIVER_PIN, LOCK_ACTIVE_HIGH ? LOW : HIGH);
  unlockUntil = 0;
}

void pulseUnlock() {
  digitalWrite(LOCK_DRIVER_PIN, LOCK_ACTIVE_HIGH ? HIGH : LOW);
  unlockUntil = millis() + UNLOCK_PULSE_MS;
}

bool authorized() {
  String expected = String("Bearer ") + RAKSHA_API_TOKEN;
  return server.header("Authorization") == expected &&
         String(RAKSHA_API_TOKEN) != "REPLACE_WITH_LONG_RANDOM_SECRET";
}

void handleHealth() {
  server.send(200, "application/json", "{\"ok\":true,\"device\":\"Raksha ESP32 Phase 2\",\"lock\":\"default_locked\"}");
}

void handleAccess() {
  if (!authorized()) {
    server.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }

  String residentId = doc["resident_id"] | "";
  String eventId = doc["event_id"] | "";
  if (residentId.length() == 0 || eventId.length() == 0 ||
      residentId.length() > 32 || eventId.length() > 80) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing_or_invalid_fields\"}");
    return;
  }

  // Prevent immediate replay of the most recently accepted event.
  if (eventId == lastEventId) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"duplicate_event\"}");
    return;
  }

  // Optional resident allow-list: set RAKSHA_ALLOWED_RESIDENTS to a comma-separated
  // list in this sketch before deployment. Until configured, deny all unlocks.
  // Example: "alice,bob"
  const String allowedResidents = ""; // Intentionally empty: configure explicitly.
  bool residentAllowed = false;
  int start = 0;
  while (start < allowedResidents.length()) {
    int comma = allowedResidents.indexOf(',', start);
    if (comma < 0) comma = allowedResidents.length();
    if (allowedResidents.substring(start, comma) == residentId) residentAllowed = true;
    start = comma + 1;
  }
  if (!residentAllowed) {
    server.send(403, "application/json", "{\"ok\":false,\"error\":\"resident_not_allowlisted\"}");
    return;
  }

  lastEventId = eventId;
  pulseUnlock();
  server.send(200, "application/json", "{\"ok\":true,\"decision\":\"unlock_pulse\"}");
}

void setup() {
  Serial.begin(115200);
  pinMode(LOCK_DRIVER_PIN, OUTPUT);
  pinMode(SIREN_DRIVER_PIN, OUTPUT);
  pinMode(INTRUSION_PIN, INPUT_PULLUP);
  setLockedOutput();
  digitalWrite(SIREN_DRIVER_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.begin(RAKSHA_WIFI_SSID, RAKSHA_WIFI_PASSWORD);
  Serial.print("Connecting to Raksha LAN");
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi unavailable; access controller remains locked.");
    return;
  }

  const char* headerKeys[] = {"Authorization"};
  server.collectHeaders(headerKeys, 1);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/api/access", HTTP_POST, handleAccess);
  server.begin();
  Serial.print("\nRaksha Phase 2 controller: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) server.handleClient();

  if (unlockUntil && (int32_t)(millis() - unlockUntil) >= 0) {
    setLockedOutput();
  }

  bool intrusionActive = digitalRead(INTRUSION_PIN) == LOW;
  if (intrusionActive) {
    digitalWrite(SIREN_DRIVER_PIN, HIGH);
    if (!intrusionWasActive || millis() - lastIntrusionAlert > ALERT_COOLDOWN_MS) {
      Serial.println("INTRUSION: trigger A9G/SMS alert workflow from Phase 1 controller.");
      lastIntrusionAlert = millis();
    }
  } else {
    digitalWrite(SIREN_DRIVER_PIN, LOW);
  }
  intrusionWasActive = intrusionActive;
  delay(2);
}

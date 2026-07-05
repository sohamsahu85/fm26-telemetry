// ═══════════════════════════════════════════════════════════
//  Formula Manipal — ESP-NOW Receiver + WiFi AP Dashboard
//
//  • Receives 79-byte binary ESP-NOW packets from the car ESP
//  • Creates WiFi hotspot "FM-Telemetry" (pw: formula1)
//  • Serves the dashboard at http://192.168.4.1
//  • Broadcasts each raw packet to all connected browsers
//    via WebSocket on port 81
//  • Still logs to SPIFFS as before
//
//  Library required (install via Arduino Library Manager):
//    "WebSockets" by Markus Sattler  (arduinoWebSockets)
// ═══════════════════════════════════════════════════════════

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WebSocketsServer_Generic.h> // WebSockets_Generic by Markus Sattler
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>

#include "dashboard_html.h" // embedded dashboard page

// ── AP credentials ─────────────────────────────────────────
#define AP_SSID "FM-Telemetry"
#define AP_PASS ""   // open network — no password needed
#define AP_CHANNEL 1 // MUST match ESP-NOW channel

// ── Packet framing ─────────────────────────────────────────
#define LED_PIN 2
#define PACKET_SIZE 79
#define START_BYTE 0xAA
#define END_BYTE 0x55

// ── Global state ───────────────────────────────────────────
static volatile bool ledPending = false;
static uint32_t ledOffAt = 0;
static uint32_t pktCount = 0;
static uint32_t wsClients = 0; // connected browser count

// SPIFFS log state
static File logFile;
static bool spiffsInitialized = false;
static char currentFilename[32];
static uint32_t linesInFile = 0;
static uint32_t fileNumber = 0;
static const uint32_t MAX_LINES_PER_FILE = 10000;

// ── Pending packet for main-loop WebSocket broadcast ───────
static uint8_t wsTxBuf[PACKET_SIZE];
static volatile bool wsTxReady = false;

// ── Servers ────────────────────────────────────────────────
WebServer httpServer(80);
WebSocketsServer wsServer(81);

// ═══════════════════════════════════════════════════════════
//  SPIFFS helpers  (unchanged from previous version)
// ═══════════════════════════════════════════════════════════
bool initializeSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
    return false;
  }
  spiffsInitialized = true;
  return true;
}

void createNewLogFile() {
  if (!spiffsInitialized)
    return;
  if (logFile)
    logFile.close();
  fileNumber++;
  snprintf(currentFilename, sizeof(currentFilename),
           "/logs/fm_telemetry_%lu.csv", fileNumber);
  logFile = SPIFFS.open(currentFilename, FILE_WRITE);
  if (!logFile)
    return;
  logFile.println("Timestamp,BMS_Total_V,BMS_Avg_Temp_C,BMS_Current_A,"
                  "BMS_Low_Cell_V,Motor_Speed_RPM,Seg0_V,Seg1_V,Seg2_V,"
                  "Seg3_V,Seg4_V,VSM,PosX,PosY,PosZ,"
                  "WheelRPM,Speed_kmh,Yaw_deg,Pitch_deg,Roll_deg");
  linesInFile = 0;
}

void logDataToSPIFFS(const String &csvData) {
  if (!spiffsInitialized)
    return;
  if (!logFile || linesInFile >= MAX_LINES_PER_FILE)
    createNewLogFile();
  if (logFile) {
    logFile.println(csvData);
    logFile.flush();
    linesInFile++;
  }
}

// ═══════════════════════════════════════════════════════════
//  HTTP — serve the embedded dashboard
// ═══════════════════════════════════════════════════════════
void handleRoot() {
  // Serve dashboard from PROGMEM — no SPIFFS needed for the page itself
  httpServer.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleStatus() {
  String json = "{\"pkts\":" + String(pktCount) +
                ",\"clients\":" + String(wsClients) + ",\"file\":\"" +
                String(currentFilename) +
                "\",\"lines\":" + String(linesInFile) + "}";
  httpServer.send(200, "application/json", json);
}

void handleFileDownload() {
  String filename = httpServer.arg("file");
  if (!filename.startsWith("/logs/") || !filename.endsWith(".csv")) {
    httpServer.send(400, "text/plain", "Invalid request");
    return;
  }
  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    httpServer.send(404, "text/plain", "Not found");
    return;
  }
  httpServer.sendHeader("Content-Disposition",
                        "attachment; filename=" + filename.substring(6));
  String content = "";
  while (file.available())
    content += char(file.read());
  file.close();
  httpServer.send(200, "text/csv", content);
}

void setupHTTP() {
  httpServer.on("/", HTTP_GET, handleRoot);
  httpServer.on("/status", HTTP_GET, handleStatus);
  httpServer.on("/download", HTTP_GET, handleFileDownload);
  httpServer.begin();
}

// ═══════════════════════════════════════════════════════════
//  WebSocket events
// ═══════════════════════════════════════════════════════════
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    wsClients++;
    Serial.printf("WS client #%u connected  (total %lu)\n", num, wsClients);
    break;
  case WStype_DISCONNECTED:
    if (wsClients > 0)
      wsClients--;
    Serial.printf("WS client #%u disconnected (total %lu)\n", num, wsClients);
    break;
  default:
    break;
  }
}

// ═══════════════════════════════════════════════════════════
//  Parse packet → CSV for SPIFFS (unchanged)
// ═══════════════════════════════════════════════════════════
bool parseBinaryAndLog(const uint8_t *d) {
  if (d[0] != START_BYTE || d[PACKET_SIZE - 1] != END_BYTE)
    return false;

  uint32_t ts;
  float bmsTotal, bmsAvgTemp, bmsCurrent, bmsLowCell;
  float motorSpeed, seg0, seg1, seg2, seg3, seg4;
  uint8_t vsm;
  float posX, posY, posZ, wheelSpd, vehicleSpd, yaw, pitch, roll;

  memcpy(&ts, d + 1, 4);
  memcpy(&bmsTotal, d + 5, 4);
  memcpy(&bmsAvgTemp, d + 9, 4);
  memcpy(&bmsCurrent, d + 13, 4);
  memcpy(&bmsLowCell, d + 17, 4);
  memcpy(&motorSpeed, d + 21, 4);
  memcpy(&seg0, d + 25, 4);
  memcpy(&seg1, d + 29, 4);
  memcpy(&seg2, d + 33, 4);
  memcpy(&seg3, d + 37, 4);
  memcpy(&seg4, d + 41, 4);
  vsm = d[45];
  memcpy(&posX, d + 46, 4);
  memcpy(&posY, d + 50, 4);
  memcpy(&posZ, d + 54, 4);
  memcpy(&wheelSpd, d + 58, 4);
  memcpy(&vehicleSpd, d + 62, 4);
  memcpy(&yaw, d + 66, 4);
  memcpy(&pitch, d + 70, 4);
  memcpy(&roll, d + 74, 4);

  if (!isfinite(bmsTotal) || !isfinite(yaw) || !isfinite(motorSpeed))
    return false;

  // Serial JSON (for pit laptop serial monitor)
  Serial.printf("{\"timestamp\":%lu,\"bmsTotal\":%.2f,\"bmsPackAvgTemp\":%.1f,"
                "\"bmsCurrent\":%.1f,\"bmsLowestCellV\":%.3f,"
                "\"motorSpeed\":%.1f,"
                "\"segVolt0\":%.2f,\"segVolt1\":%.2f,\"segVolt2\":%.2f,"
                "\"segVolt3\":%.2f,\"segVolt4\":%.2f,"
                "\"vsmState\":%u,"
                "\"posX\":%.4f,\"posY\":%.4f,\"posZ\":%.4f,"
                "\"wheelSpeed\":%.1f,\"vehicleSpeed\":%.1f,"
                "\"yaw\":%.2f,\"pitch\":%.2f,\"roll\":%.2f}\n",
                (unsigned long)ts, bmsTotal, bmsAvgTemp, bmsCurrent, bmsLowCell,
                motorSpeed, seg0, seg1, seg2, seg3, seg4, vsm, posX, posY, posZ,
                wheelSpd, vehicleSpd, yaw, pitch, roll);

  // CSV → SPIFFS
  String csv = String(ts) + "," + String(bmsTotal, 2) + "," +
               String(bmsAvgTemp, 1) + "," + String(bmsCurrent, 1) + "," +
               String(bmsLowCell, 3) + "," + String(motorSpeed, 1) + "," +
               String(seg0, 2) + "," + String(seg1, 2) + "," + String(seg2, 2) +
               "," + String(seg3, 2) + "," + String(seg4, 2) + "," +
               String(vsm) + "," + String(posX, 4) + "," + String(posY, 4) +
               "," + String(posZ, 4) + "," + String(wheelSpd, 1) + "," +
               String(vehicleSpd, 1) + "," + String(yaw, 2) + "," +
               String(pitch, 2) + "," + String(roll, 2);
  logDataToSPIFFS(csv);
  return true;
}

// ═══════════════════════════════════════════════════════════
//  ESP-NOW receive callback
//  NOTE: runs in WiFi task context — keep it fast!
//  We copy the packet and set a flag; main loop does the rest.
// ═══════════════════════════════════════════════════════════
void OnDataReceived(const esp_now_recv_info_t *info, const uint8_t *data,
                    int len) {
  if (len <= 0)
    return;
  pktCount++;
  ledPending = true;

  if (len == PACKET_SIZE) {
    // Copy to TX buffer for WebSocket broadcast in main loop
    memcpy((void *)wsTxBuf, data, PACKET_SIZE);
    wsTxReady = true; // signal main loop
  } else {
    // Fallback raw text via Serial
    for (int i = 0; i < len; i++)
      Serial.write(data[i]);
    if (data[len - 1] != '\n')
      Serial.println();
  }
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(200);

  Serial.println("\n== FM ESP-NOW Receiver + AP Dashboard ==");

  // SPIFFS
  if (!initializeSPIFFS())
    Serial.println("WARNING: SPIFFS failed — logging disabled");

  // ── WiFi: AP_STA lets us run both the hotspot AND ESP-NOW ──
  WiFi.mode(WIFI_AP_STA);

  // Start access point on channel 1 (same as ESP-NOW)
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL);
  Serial.printf("AP: %-20s  IP: %s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());
  Serial.println("Team: connect to \"" AP_SSID "\" pw: \"" AP_PASS
                 "\" then open http://192.168.4.1");

  // Set STA interface to channel 1 for ESP-NOW
  esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE);

  // Max TX power for range
  esp_wifi_set_max_tx_power(84);

  // Long-range PHY (both ends must have LR enabled)
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                         WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

  Serial.print("STA MAC: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: esp_now_init failed");
    while (true) {
    }
  }
  esp_now_register_recv_cb(OnDataReceived);
  Serial.println("ESP-NOW listening on channel 1");

  // HTTP + WebSocket
  setupHTTP();
  wsServer.begin();
  wsServer.onEvent(onWsEvent);
  Serial.println("HTTP  on port 80");
  Serial.println("WS    on port 81");

  if (spiffsInitialized)
    createNewLogFile();

  Serial.println("System ready\n");
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
  httpServer.handleClient();
  wsServer.loop();

  // ── Broadcast packet to WebSocket clients ──
  if (wsTxReady) {
    wsTxReady = false;
    uint8_t localCopy[PACKET_SIZE];
    memcpy(localCopy, (void *)wsTxBuf, PACKET_SIZE);

    // Parse & log to SPIFFS
    parseBinaryAndLog(localCopy);

    // Broadcast raw binary — browsers parse it directly
    if (wsClients > 0)
      wsServer.broadcastBIN(localCopy, PACKET_SIZE);
  }

  // ── LED flash on packet ──
  if (ledPending) {
    digitalWrite(LED_PIN, HIGH);
    ledOffAt = millis() + 20;
    ledPending = false;
  }
  if (millis() >= ledOffAt)
    digitalWrite(LED_PIN, LOW);

  // ── Periodic stats ──
  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 3000) {
    Serial.printf("Stats: %lu pkts | %lu WS clients | %lu lines → %s\n",
                  pktCount, wsClients, linesInFile, currentFilename);
    lastReport = millis();
  }

  yield();
}
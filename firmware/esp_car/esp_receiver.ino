/* ESP32 UART -> ESP-NOW relay (improved)
   - Reads newline-terminated lines from a Teensy over UART
   - Queues them and forwards via ESP-NOW to a fixed peer MAC
   - Retries failed sends up to MAX_RETRIES
   - Uses wifi_tx_info_t send-callback to determine send result and advance
   queue
   - Non-blocking main loop
   - Requirements: Arduino-ESP32 core
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

HardwareSerial
    TeensySerial(2); // UART1
#define RXD 16
#define TXD 17
                     // unused for Teensy -> ESP direction, kept for begin()

// UART0 (we set pins in begin())

// The receiver peer MAC (the ESP in your earlier posts)
static const uint8_t receiverAddress[] = {0x90, 0x15, 0x06, 0x94,
                                          0xC6, 0x70}; // 90:15:06:94:C6:70

// Config
constexpr size_t MAX_PAYLOAD = 230;
constexpr size_t QUEUE_SIZE = 64; // larger buffer for 100 Hz burst
constexpr uint8_t MAX_RETRIES = 3;
constexpr uint16_t SEND_INTERVAL_MS = 2; // 2 ms → up to 500 send attempts/s

// Binary packet framing (must match Teensy's TestPayload struct)
constexpr size_t PKT_LEN = 79;
constexpr uint8_t PKT_START = 0xAA;
constexpr uint8_t PKT_END = 0x55;

struct PendingMsg {
  uint8_t len;
  uint8_t tries; // how many attempts already done
  uint8_t data[MAX_PAYLOAD];
};

static PendingMsg msgQueue[QUEUE_SIZE];
static volatile uint8_t q_head = 0; // index of item being sent / next to pop
static volatile uint8_t q_tail = 0; // index where next push will go
// note: queue is empty when head == tail, full when next tail == head

// State whether a send is in-flight waiting for callback
static volatile bool send_in_progress = false;
static portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

// Helper: convert mac to string
String macToString(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// Utility: queue helpers (protected with portENTER/EXIT_CRITICAL)
static inline uint8_t nextIndex(uint8_t idx) { return (idx + 1) % QUEUE_SIZE; }

bool queuePush(const uint8_t *data, uint8_t len) {
  if (len == 0 || len > MAX_PAYLOAD)
    return false;
  bool ok = false;
  portENTER_CRITICAL(&queueMux);
  uint8_t nt = nextIndex(q_tail);
  if (nt != q_head) { // not full
    msgQueue[q_tail].len = len;
    msgQueue[q_tail].tries = 0;
    memcpy(msgQueue[q_tail].data, data, len);
    q_tail = nt;
    ok = true;
  }
  portEXIT_CRITICAL(&queueMux);
  return ok;
}

bool queuePeek(PendingMsg &out) {
  bool ok = false;
  portENTER_CRITICAL(&queueMux);
  if (q_head != q_tail) {
    out = msgQueue[q_head];
    ok = true;
  }
  portEXIT_CRITICAL(&queueMux);
  return ok;
}

void queuePop() {
  portENTER_CRITICAL(&queueMux);
  if (q_head != q_tail) {
    q_head = nextIndex(q_head);
  }
  portEXIT_CRITICAL(&queueMux);
}

// ESP-NOW send callback - note signature uses wifi_tx_info_t (may vary with
// cores)
void OnSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  (void)info; // avoid unused warning
  // We use queue head as the message that was sent (if any)
  portENTER_CRITICAL(&queueMux);
  if (q_head == q_tail) {
    // No queued item, but callback arrived — just clear in_progress and exit
    send_in_progress = false;
    portEXIT_CRITICAL(&queueMux);
    return;
  }

  // If success -> pop and clear in-progress
  if (status == ESP_NOW_SEND_SUCCESS) {
    // Serial debug (avoid complex prints in callback, but small print ok)
    // Note: printing in ISR/callback may be unsafe on some cores; keep short.
    // Serial.println("OnSent: success");
    queuePop();
    send_in_progress = false;
  } else {
    // failure -> increment tries, and either retry or drop
    msgQueue[q_head].tries++;
    if (msgQueue[q_head].tries >= MAX_RETRIES) {
      // drop it
      // Serial.println("OnSent: failed -> dropping after retries");
      queuePop();
      send_in_progress = false;
    } else {
      // we'll leave it at head and allow main loop to retry
      send_in_progress = false;
    }
  }
  portEXIT_CRITICAL(&queueMux);
}

// Attempt to send the head-of-queue via esp_now_send
// Returns true if send request was issued (callback will decide success)
bool trySendHead() {
  bool didSend = false;
  // if already sending, skip
  portENTER_CRITICAL(&queueMux);
  bool empty = (q_head == q_tail);
  bool busy = send_in_progress;
  if (!empty && !busy) {
    // prepare
    PendingMsg &pm = msgQueue[q_head];
    // mark busy BEFORE calling esp_now_send so callback knows
    send_in_progress = true;
    // do not modify pm here
    portEXIT_CRITICAL(&queueMux);

    esp_err_t rc = esp_now_send((uint8_t *)receiverAddress, pm.data, pm.len);
    if (rc != ESP_OK) {
      // immediate failure to queue send — rollback busy and increment tries
      portENTER_CRITICAL(&queueMux);
      send_in_progress = false;
      msgQueue[q_head].tries++;
      if (msgQueue[q_head].tries >= MAX_RETRIES) {
        // drop it
        queuePop();
      }
      portEXIT_CRITICAL(&queueMux);
      didSend = false;
    } else {
      didSend = true;
    }
    return didSend;
  }
  portEXIT_CRITICAL(&queueMux);
  return false;
}

// Binary packet reader: hunts for 0xAA start byte, reads exactly PKT_LEN bytes,
// validates 0x55 end byte. Safe against 0x0A (newline) bytes inside float data.
size_t readBinaryPacketFromTeensy(uint8_t *outBuf, size_t maxLen) {
  static uint8_t pktBuf[PKT_LEN];
  static size_t idx = 0;
  static bool synced = false;
  static uint32_t totalBytesRead = 0;
  static uint32_t lastDebug = 0;

  if (maxLen < PKT_LEN)
    return 0;

  while (TeensySerial.available()) {
    int c = TeensySerial.read();
    if (c < 0)
      break;

    totalBytesRead++;

    if (!synced) {
      // Hunt for start byte
      if ((uint8_t)c == PKT_START) {
        pktBuf[0] = PKT_START;
        idx = 1;
        synced = true;
        Serial.printf("Found 0xAA start byte (after %lu total bytes)\n",
                      totalBytesRead);
      }
      continue;
    }

    pktBuf[idx++] = (uint8_t)c;

    if (idx == PKT_LEN) {
      synced = false;
      idx = 0;
      if (pktBuf[PKT_LEN - 1] == PKT_END) {
        memcpy(outBuf, pktBuf, PKT_LEN);
        return PKT_LEN;
      }
      // End byte mismatch — bad frame, resync
      Serial.printf("End byte mismatch: expected 0x55, got 0x%02X at pos %u\n",
                    pktBuf[PKT_LEN - 1], PKT_LEN - 1);
    }
  }

  // Periodic debug: show if any bytes are arriving at all
  if (millis() - lastDebug > 3000) {
    Serial.printf("UART debug: %lu bytes received total\n", totalBytesRead);
    lastDebug = millis();
  }
  return 0;
}

void setup() {
  pinMode(2, OUTPUT);

  Serial.begin(115200);
  delay(200);

  Serial.println("\n== ESP32 Sender (UART→ESP-NOW) - improved ==");

  // Initialize Teensy serial (read from Teensy). RXD is RX pin for this ESP32.
  TeensySerial.begin(460800, SERIAL_8N1, RXD,
                     TXD); // 460800 matches Teensy Serial7
  Serial.print("Teensy UART started on RXD=");
  Serial.println(RXD);

  // Init WiFi in station mode and lock channel to 1 (match receiver)
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // Boost TX power to maximum (20 dBm = 84 in units of 0.25 dBm)
  esp_wifi_set_max_tx_power(84);

  // Enable Long-Range PHY — both sender and receiver must have LR enabled
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                         WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);

  Serial.print("This ESP32 MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("ESP-NOW Sender: configured to channel 1");

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: esp_now_init() failed");
    while (true)
      delay(1000);
  }

  // Register send callback
  esp_err_t rc = esp_now_register_send_cb(OnSent);
  if (rc != ESP_OK) {
    Serial.printf("Warning: esp_now_register_send_cb returned %d\n", rc);
  }

  // Add receiver as peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverAddress, 6);
  peer.channel = 1;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Warning: esp_now_add_peer() failed (peer add). Send might "
                   "still work transiently.");
  } else {
    Serial.print("Peer added: ");
    Serial.println(macToString(receiverAddress));
  }

  Serial.println("Ready to relay telemetry from Teensy → ESP-NOW.");
}

void loop() {
  static unsigned long lastSendAttempt = 0;

  // 1) Read a complete binary packet from Teensy and push to queue
  uint8_t pkt[PKT_LEN];
  size_t len = readBinaryPacketFromTeensy(pkt, sizeof(pkt));
  if (len == PKT_LEN) {
    // Brief LED flash — non-blocking (GPIO write is <1µs)
    digitalWrite(2, HIGH);
    digitalWrite(2, LOW);

    // Debug: Print first few bytes to verify packet structure
    Serial.printf("Binary packet: %02X %02X %02X %02X ... %02X\n", pkt[0],
                  pkt[1], pkt[2], pkt[3], pkt[PKT_LEN - 1]);

    if (!queuePush(pkt, (uint8_t)len)) {
      Serial.println("Queue full: dropped packet");
    } else {
      Serial.printf("Queued %u-byte binary packet\n", (unsigned)len);
    }
  }

  // 2) Attempt to send head-of-queue if not in progress and enough time elapsed
  unsigned long now = millis();
  if (now - lastSendAttempt >= SEND_INTERVAL_MS) {
    if (trySendHead()) {
      // We initiated a send; OnSent callback will pop on success/failure
      lastSendAttempt = now;
    } else {
      // either no msg or send failed to start; wait
      lastSendAttempt = now;
    }
  }

  // 3) Optionally: print queue status occasionally
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 2000) {
    portENTER_CRITICAL(&queueMux);
    int qlen = (q_tail + QUEUE_SIZE - q_head) % QUEUE_SIZE;
    bool inprog = send_in_progress;
    portEXIT_CRITICAL(&queueMux);
    Serial.printf("Qlen=%d, in_progress=%d\n", qlen, inprog ? 1 : 0);
    lastStatus = millis();
  }

  delay(1);
}

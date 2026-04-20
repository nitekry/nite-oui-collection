#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <ctype.h>
#include <string.h>

// ============================================================
// CONFIG
// ============================================================

#define BUZZER_PIN 3
#define USE_BUZZER 1

#define CHANNEL_MODE_FULL_HOP   0
#define CHANNEL_MODE_CUSTOM     1
#define CHANNEL_MODE_SINGLE     2

#define CHANNEL_MODE CHANNEL_MODE_CUSTOM
#define CHANNEL_DWELL_MS 350
#define SINGLE_CHANNEL 1

static const uint8_t customChannels[]  = {1, 6, 11};
static const size_t  customChannelCount = sizeof(customChannels) / sizeof(customChannels[0]);

static const uint8_t fullHopChannels[] = {1,2,3,4,5,6,7,8,9,10,11};
static const size_t  fullHopChannelCount = sizeof(fullHopChannels) / sizeof(fullHopChannels[0]);

#define HEARTBEAT_MS    30000
#define RSSI_MIN        -95
#define ALERT_COOLDOWN_MS 5000

#define ENABLE_SSID_MATCH 0
#define CHECK_ADDR1 1   // dst/rx — catches Flock STAs receiving probe responses
#define CHECK_ADDR3 0   // bssid fallback for randomised addr2
static const char* target_ssid_keywords[] = { "flock" };
static const size_t SSID_KEYWORD_COUNT = sizeof(target_ssid_keywords) / sizeof(target_ssid_keywords[0]);

#define STOP_ON_SSID_HIT 0
#define STOP_ON_OUI_HIT  0
#define PROCESS_MGMT_FRAMES 1
#define PROCESS_DATA_FRAMES 1

// ============================================================
// TARGET OUI LIST  (all lowercase, colons only)
// ============================================================

static const char* target_ouis[] = {
"58:8e:81", "ec:1b:bd", "90:35:ea", "04:0d:84", "f0:82:c0",
"1c:34:f1", "38:5b:44", "94:34:69", "b4:e3:f9", "70:c9:4e",
"3c:91:80", "d8:f3:bc", "80:30:49", "14:5a:fc", "74:4c:a1",
"08:3a:88", "9c:2f:9d", "94:08:53", "e4:aa:ea", "b4:1e:52",
"b8:35:32", "c0:35:32", "f4:6a:dd", "24:b2:b9", "e0:0a:f6",
"e0:4f:43", "b8:1e:a4", "14:b5:cd", "6c:cd:d6", "94:2a:6f",
"f4:e2:c6", "70:08:94", "3c:71:bf", "58:00:e3", "5c:93:a2",
"64:6e:69", "f8:a2:d6", "00:f4:8d", "d0:39:57", "e8:d0:fc",
"d4:11:d6", "48:27:ea", "a4:cf:12"

};
static const size_t OUI_COUNT = sizeof(target_ouis) / sizeof(target_ouis[0]);

// Pre-compiled byte table — populated once in setup(), never touched again.
// Keeps matchOuiRaw entirely in IRAM with no flash-resident function calls.
static uint8_t oui_bytes[OUI_COUNT][3];

// ============================================================
// ALERT QUEUE  (callback → loop, avoids Serial in WiFi task)
// ============================================================

#define ALERT_QUEUE_SIZE 16

typedef enum : uint8_t {
  ALERT_OUI  = 0,
  ALERT_SSID = 1,
} AlertType;

typedef struct {
  AlertType type;
  uint8_t   mac[6];
  int8_t    rssi;
  uint8_t   channel;
  char      ssid[33];     // populated for SSID hits
  char      frameKind[12];
} AlertEntry;

static volatile AlertEntry alertQueue[ALERT_QUEUE_SIZE];
static volatile size_t alertHead = 0;  // written by callback
static volatile size_t alertTail = 0;  // read by loop()

static void IRAM_ATTR enqueueAlert(AlertType type, const uint8_t* mac, int8_t rssi,
                                    uint8_t ch, const char* ssid, const char* kind) {
  size_t next = (alertHead + 1) % ALERT_QUEUE_SIZE;
  if (next == alertTail) return;  // drop if full — loop() is behind

  AlertEntry* e = (AlertEntry*)&alertQueue[alertHead];
  e->type    = type;
  e->rssi    = rssi;
  e->channel = ch;
  memcpy((void*)e->mac, mac, 6);

  if (ssid)  { strncpy((char*)e->ssid,      ssid, 32); ((char*)e->ssid)[32] = '\0'; }
  else        { ((char*)e->ssid)[0] = '\0'; }

  if (kind)  { strncpy((char*)e->frameKind, kind, 11); ((char*)e->frameKind)[11] = '\0'; }
  else        { ((char*)e->frameKind)[0] = '\0'; }

  alertHead = next;
}

// ============================================================
// STATE
// ============================================================

static uint8_t  currentChannel = 1;
static size_t   customChannelIndex = 0;
static size_t   fullHopIndex = 0;
static unsigned long lastHop = 0;
static unsigned long lastHeartbeat = 0;
static volatile bool sniffingStopped = false;

// Dedupe table (small circular, avoids single-slot eviction bug)
#define DEDUPE_SLOTS 8
static struct {
  char mac[18];
  unsigned long ts;
} dedupeTable[DEDUPE_SLOTS];
static size_t dedupeIdx = 0;

// ============================================================
// 802.11 HEADER
// ============================================================

typedef struct __attribute__((packed)) {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t  addr1[6];
  uint8_t  addr2[6];
  uint8_t  addr3[6];
  uint16_t seq_ctrl;
} wifi_ieee80211_mac_hdr_t;

// ============================================================
// HELPERS
// ============================================================

static void buzzerBeep(unsigned int ms) {
#if USE_BUZZER
  digitalWrite(BUZZER_PIN, HIGH); delay(ms); digitalWrite(BUZZER_PIN, LOW);
#endif
}
static void startupBeep() {
#if USE_BUZZER
  buzzerBeep(80); delay(50); buzzerBeep(80);
#endif
}

static void macToStr(const uint8_t* mac, char* buf, size_t len) {
  snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
static void ouiFromMac(const uint8_t* mac, char* buf, size_t len) {
  snprintf(buf, len, "%02x:%02x:%02x", mac[0], mac[1], mac[2]);
}

static void precompileOuis() {
  for (size_t i = 0; i < OUI_COUNT; i++) {
    const char* o  = target_ouis[i];
    oui_bytes[i][0] = (uint8_t)strtol(o,     nullptr, 16);
    oui_bytes[i][1] = (uint8_t)strtol(o + 3, nullptr, 16);
    oui_bytes[i][2] = (uint8_t)strtol(o + 6, nullptr, 16);
  }
}

// Bit 0 of byte 0 set = multicast/broadcast — never a real device transmitter or receiver
// we care about. Guards addr1 checks against 01:xx, 33:33:xx, ff:ff:ff:ff:ff:ff etc.
static inline bool IRAM_ATTR isMulticast(const uint8_t* mac) {
  return mac[0] & 0x01;
}

static bool IRAM_ATTR matchOuiRaw(const uint8_t* mac) {
  // Locally-administered (randomised) MACs have bit 1 of byte 0 set.
  // Fixed infrastructure devices never use them — skip immediately.
  if (mac[0] & 0x02) return false;

  for (size_t i = 0; i < OUI_COUNT; i++) {
    if (mac[0] == oui_bytes[i][0] &&
        mac[1] == oui_bytes[i][1] &&
        mac[2] == oui_bytes[i][2]) return true;
  }
  return false;
}

static char* strcasestr_local(const char* haystack, const char* needle) {
  if (!*needle) return (char*)haystack;
  for (; *haystack; ++haystack) {
    const char* h = haystack; const char* n = needle;
    while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { ++h; ++n; }
    if (!*n) return (char*)haystack;
  }
  return nullptr;
}
static bool matchSsidKeyword(const char* ssid) {
  for (size_t i = 0; i < SSID_KEYWORD_COUNT; i++)
    if (strcasestr_local(ssid, target_ssid_keywords[i])) return true;
  return false;
}

static const char* channelModeName() {
  switch (CHANNEL_MODE) {
    case CHANNEL_MODE_FULL_HOP: return "FULL_HOP";
    case CHANNEL_MODE_CUSTOM:   return "CUSTOM";
    case CHANNEL_MODE_SINGLE:   return "SINGLE";
    default:                    return "UNKNOWN";
  }
}

static bool shouldSuppressDuplicate(const char* macStr) {
  unsigned long now = millis();
  for (size_t i = 0; i < DEDUPE_SLOTS; i++) {
    if (strcmp(dedupeTable[i].mac, macStr) == 0) {
      if ((now - dedupeTable[i].ts) < ALERT_COOLDOWN_MS) return true;
      dedupeTable[i].ts = now;
      return false;
    }
  }
  // Not found — insert into next slot
  strlcpy(dedupeTable[dedupeIdx].mac, macStr, 18);
  dedupeTable[dedupeIdx].ts = now;
  dedupeIdx = (dedupeIdx + 1) % DEDUPE_SLOTS;
  return false;
}

static void stopSniffing(const char* reason) {
  if (sniffingStopped) return;
  sniffingStopped = true;
  esp_wifi_set_promiscuous(false);
  Serial.printf("[flockyou] sniffing stopped: %s\n", reason);
}

static void applyInitialChannel() {
#if CHANNEL_MODE == CHANNEL_MODE_SINGLE
  currentChannel = SINGLE_CHANNEL;
#elif CHANNEL_MODE == CHANNEL_MODE_CUSTOM
  currentChannel = customChannels[0];
#else
  currentChannel = fullHopChannels[0];
#endif
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

static void updateChannelMode() {
  if (sniffingStopped) return;
#if CHANNEL_MODE == CHANNEL_MODE_SINGLE
  if (currentChannel != SINGLE_CHANNEL) {
    currentChannel = SINGLE_CHANNEL;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  }
  return;
#else
  if (millis() - lastHop < CHANNEL_DWELL_MS) return;
  #if CHANNEL_MODE == CHANNEL_MODE_CUSTOM
    customChannelIndex = (customChannelIndex + 1) % customChannelCount;
    currentChannel = customChannels[customChannelIndex];
  #else
    fullHopIndex = (fullHopIndex + 1) % fullHopChannelCount;
    currentChannel = fullHopChannels[fullHopIndex];
  #endif
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();
#endif
}

static void printHeartbeat() {
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    Serial.printf("[flockyou] The Toast is Clear (ch=%u mode=%s)\n",
                  currentChannel, channelModeName());
    lastHeartbeat = millis();
  }
}

// ============================================================
// PROMISCUOUS CALLBACK  — keep it fast, no Serial, no malloc
// ============================================================

static bool IRAM_ATTR extractSsidFromMgmtBody(const uint8_t* body, int len,
                                     char* outSsid, size_t outLen) {
  if (!body || len <= 0 || !outSsid || outLen == 0) return false;
  while (len >= 2) {
    uint8_t id = body[0], elen = body[1];
    if ((int)elen + 2 > len) break;
    if (id == 0) {
      size_t n = (elen < (outLen - 1)) ? elen : (outLen - 1);
      memcpy(outSsid, body + 2, n);
      outSsid[n] = '\0';
      return true;
    }
    body += elen + 2; len -= elen + 2;
  }
  return false;
}

static void IRAM_ATTR wifiSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf || sniffingStopped) return;

#if PROCESS_MGMT_FRAMES && PROCESS_DATA_FRAMES
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
#elif PROCESS_MGMT_FRAMES
  if (type != WIFI_PKT_MGMT) return;
#elif PROCESS_DATA_FRAMES
  if (type != WIFI_PKT_DATA) return;
#else
  return;  // nothing configured to process
#endif

  wifi_promiscuous_pkt_t*      pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_ieee80211_mac_hdr_t*    hdr = (wifi_ieee80211_mac_hdr_t*)pkt->payload;
  int8_t rssi = pkt->rx_ctrl.rssi;

  if (rssi < RSSI_MIN) return;

  uint8_t ch = (uint8_t)pkt->rx_ctrl.channel;  // actual rx channel from driver

  // --- OUI check: addr2 (transmitter/source) ---
  if (matchOuiRaw(hdr->addr2)) {
    enqueueAlert(ALERT_OUI, hdr->addr2, rssi, ch, nullptr, "addr2");
  }

#if CHECK_ADDR1
  // addr1 (receiver/destination): catches Flock STAs that appear only as the
  // dst of probe responses and data frames — never transmitting in the capture
  // window due to their burst-sleep duty cycle. Multicast guard is mandatory
  // here since addr1 is broadcast (ff:ff:ff:ff:ff:ff) in beacons/broadcasts.
  if (!isMulticast(hdr->addr1) && matchOuiRaw(hdr->addr1)) {
    enqueueAlert(ALERT_OUI, hdr->addr1, rssi, ch, nullptr, "addr1");
  }
#endif

#if CHECK_ADDR3
  // addr3 fallback: catches cases where addr2 is randomised but addr3
  // carries the real BSSID OUI (management frames only).
  if (type == WIFI_PKT_MGMT && matchOuiRaw(hdr->addr3)) {
    enqueueAlert(ALERT_OUI, hdr->addr3, rssi, ch, nullptr, "addr3");
  }
#endif

#if ENABLE_SSID_MATCH
  if (type == WIFI_PKT_MGMT) {
    uint8_t fc0     = hdr->frame_ctrl & 0xFF;
    uint8_t subtype = (fc0 >> 4) & 0x0F;
    uint8_t ftype   = (fc0 >> 2) & 0x03;

    if (ftype == 0) {
      int sigLen = pkt->rx_ctrl.sig_len - 4;  // strip 4-byte FCS
      if (sigLen < (int)sizeof(wifi_ieee80211_mac_hdr_t)) return;

      const uint8_t* mgmtBody    = nullptr;
      int            mgmtBodyLen = 0;
      const char*    frameKind   = nullptr;

      if (subtype == 8 || subtype == 5) {
        // Beacon / Probe Response: fixed params = 12 bytes after MAC hdr
        int off = sizeof(wifi_ieee80211_mac_hdr_t) + 12;
        if (sigLen > off) {
          frameKind   = (subtype == 8) ? "beacon" : "probe_resp";
          mgmtBody    = pkt->payload + off;
          mgmtBodyLen = sigLen - off;
        }
      } else if (subtype == 4) {
        // Probe Request: IEs follow directly after MAC hdr
        int off = sizeof(wifi_ieee80211_mac_hdr_t);
        if (sigLen > off) {
          frameKind   = "probe_req";
          mgmtBody    = pkt->payload + off;
          mgmtBodyLen = sigLen - off;
        }
      }

      if (mgmtBody && mgmtBodyLen > 0) {
        char ssid[33] = {0};
        if (extractSsidFromMgmtBody(mgmtBody, mgmtBodyLen, ssid, sizeof(ssid))) {
          if (matchSsidKeyword(ssid)) {
            enqueueAlert(ALERT_SSID, hdr->addr2, rssi, ch, ssid, frameKind);
          }
        }
      }
    }
  }
#endif
}

// ============================================================
// DRAIN QUEUE — called from loop(), safe to Serial.print here
// ============================================================

static void drainAlertQueue() {
  while (alertTail != alertHead) {
    AlertEntry e;
    memcpy(&e, (const void*)&alertQueue[alertTail], sizeof(AlertEntry));
    alertTail = (alertTail + 1) % ALERT_QUEUE_SIZE;

    char macStr[18];
    macToStr(e.mac, macStr, sizeof(macStr));

    if (shouldSuppressDuplicate(macStr)) continue;

    if (e.type == ALERT_OUI) {
      char oui[9];
      ouiFromMac(e.mac, oui, sizeof(oui));
      Serial.printf("[flockyou] DETECT-OUI mac=%s oui=%s rssi=%d ch=%u addr=%s\n",
                    macStr, oui, e.rssi, e.channel,
                    e.frameKind[0] ? e.frameKind : "addr2");
    } else {
      Serial.printf("[flockyou] DETECT-SSID type=%s mac=%s ssid=\"%s\" rssi=%d ch=%u\n",
                    e.frameKind, macStr, e.ssid, e.rssi, e.channel);
    }

#if USE_BUZZER
    buzzerBeep(60);
#endif

#if STOP_ON_OUI_HIT
    if (e.type == ALERT_OUI)  stopSniffing("OUI hit");
#endif
#if STOP_ON_SSID_HIT
    if (e.type == ALERT_SSID) stopSniffing("SSID hit");
#endif
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(300);

#if USE_BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

  startupBeep();
  precompileOuis();
  memset(dedupeTable, 0, sizeof(dedupeTable));

  WiFi.mode(WIFI_MODE_NULL);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();

  applyInitialChannel();

  wifi_promiscuous_filter_t filt = {
    .filter_mask = 0
#if PROCESS_MGMT_FRAMES
        | WIFI_PROMIS_FILTER_MASK_MGMT
#endif
#if PROCESS_DATA_FRAMES
        | WIFI_PROMIS_FILTER_MASK_DATA
#endif
  };
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&wifiSniffer);
  esp_wifi_set_promiscuous(true);

  Serial.println("[flockyou] merged WiFi detector started");
  Serial.printf("[flockyou] mode=%s dwell_ms=%u start_channel=%u rssi_min=%d\n",
                channelModeName(), CHANNEL_DWELL_MS, currentChannel, RSSI_MIN);

  lastHeartbeat = millis();
  lastHop       = millis();
}

void loop() {
  updateChannelMode();
  drainAlertQueue();   // Serial.printf happens here, not in callback
  printHeartbeat();
  delay(1);
}
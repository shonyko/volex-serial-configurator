#include <Arduino.h>
#include <MyWiFi.h>
#include <espnow.h>

#define BAUD_RATE 9600
#define VOLEX_REQUEST_PREFIX "[volex-conn]"
#define CMD_PREFIX "[cmd]"
#define LOG_PREFIX "[log]"
#define DATA_PREFIX "[tx]"

#define HEARTBEAT_INTERVAL 1000

unsigned long previousMillis;

void log(const String &s, bool newLine = true) {
  auto msg = LOG_PREFIX + s;
  if (newLine)
    Serial.println(msg);
  else
    Serial.print(msg);
}

void send(const String &s, bool flush = true) {
  auto msg = DATA_PREFIX + s;
  if (flush)
    Serial.println(msg);
  else
    Serial.print(msg);
}

bool isValidRequest(uint8_t *data, uint8_t len) {
  int prefix_len = sizeof(VOLEX_REQUEST_PREFIX) - 1;
  for (int i = 0; i < prefix_len; i++) {
    if (i >= len || VOLEX_REQUEST_PREFIX[i] != data[i]) {
      return false;
    }
  }

  return true;
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (!isValidRequest(incomingData, len)) {
    log("Invalid request: " + String((char *)incomingData));
    return;
  }

  char buffer[18] = {0};
  sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);

  String macStr(buffer);
  String dataStr((char *)(incomingData + sizeof(VOLEX_REQUEST_PREFIX) - 1));

  Serial.print(DATA_PREFIX);
  Serial.print(macStr);
  Serial.print("|");
  Serial.println(dataStr);
}

void OnDataSent(uint8_t *mac_addr, uint8_t status) {
  Serial.println(status == 0 ? "Delivery Success" : "Delivery Fail");
}

bool getMac(uint8_t *mac, const char *str) {
  char *endptr;
  for (int i = 0; i < 6; i++) {
    mac[i] = strtol(str, &endptr, 16);
    if (*endptr != ':' && i < 5) {
      log("Invalid MAC address format!");
      return false;
    }
    str = endptr + 1;
  }

  return true;
}

void handleCommand(const String &msg) {
  log("Got command: " + msg);
  if (!msg.startsWith(CMD_PREFIX)) {
    log("Invalid cmd: " + msg);
    return;
  }

  auto cmd = msg.substring(msg.indexOf(']') + 1);

  int endIdx;
  if ((endIdx = cmd.indexOf('|')) == -1) {
    log("Invalid cmd format: " + cmd);
    return;
  }

  auto macStr = cmd.substring(0, endIdx);
  auto payload = cmd.substring(endIdx + 1);

  uint8_t mac[6];
  if (!getMac(mac, macStr.c_str())) {
    return;
  }

  esp_now_add_peer(mac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_send(mac, (uint8_t *)payload.c_str(), payload.length() + 1);
  esp_now_del_peer(mac);
}

void setup() {
  Serial.begin(BAUD_RATE);
  Serial.println();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    log("Error initializing ESP-NOW.");
    ESP.reset();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
}

void loop() {
  auto currentMillis = millis();
  if (currentMillis - previousMillis >= HEARTBEAT_INTERVAL) {
    Serial.println("hearbeat");
    previousMillis = currentMillis;
  }

  if (Serial.available()) {
    auto data = Serial.readStringUntil('\n');
    data.trim();
    handleCommand(data);
  }
}
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

ESP8266WiFiMulti wifiMulti;

// ข้อมูล Server บน Render
const char* ws_host  = "smart-home-wemosd1.onrender.com";
const int   ws_port  = 443;
const char* ws_path  = "/esp";

// -------------------------------------------------------------------------
// กำหนดขา GPIO สำหรับ Wemos D1 (ESP8266) ทั้งหมด 8 ช่อง
// Ch 0: D1 (GPIO 5)   | Ch 1: D2 (GPIO 4)
// Ch 2: D5 (GPIO 14)  | Ch 3: D6 (GPIO 12)
// Ch 4: D7 (GPIO 13)  | Ch 5: D0 (GPIO 16)
// Ch 6: D3 (GPIO 0)   | Ch 7: D4 (GPIO 2)
// -------------------------------------------------------------------------
const int NUM_RELAYS = 8;
const int relayPins[NUM_RELAYS] = {D1, D2, D5, D6, D7, D0, D3, D4};
bool relayStates[NUM_RELAYS]    = {false, false, false, false, false, false, false, false};

WebSocketsClient webSocket;

// กำหนดสถานะสำหรับโมดูล Active LOW
#define RELAY_ON  LOW   // Active LOW: ติด = LOW
#define RELAY_OFF HIGH  // Active LOW: ดับ = HIGH

void applyRelays() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], relayStates[i] ? RELAY_ON : RELAY_OFF);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[Smart Office WS] ขาดการเชื่อมต่อกับ Server");
      break;

    case WStype_CONNECTED:
      Serial.println("[Smart Office WS] เชื่อมต่อกับ Server สำเร็จ!");
      // ส่งคำขอสถานะล่าสุดจาก Server ทันทีเมื่อต่อติด
      webSocket.sendTXT("{\"type\":\"GET_STATE\"}");
      break;

    case WStype_TEXT: {
      Serial.printf("[Smart Office WS] รับคำสั่งจาก Server: %s\n", payload);
      DynamicJsonDocument doc(768);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) return;

      const char* msgType = doc["type"];

      // รองรับทั้ง INIT_STATE, STATE_UPDATE หรือกรณีส่งอาร์เรย์ relays มา
      if (doc.containsKey("relays")) {
        JsonArray arr = doc["relays"];
        int count = arr.size() < NUM_RELAYS ? arr.size() : NUM_RELAYS;
        for (int i = 0; i < count; i++) {
          relayStates[i] = arr[i];
        }
        applyRelays();
        Serial.println("[WS] ซิงค์สถานะรีเลย์ตาม Server สำเร็จ");
      }
      else if (msgType != NULL && strcmp(msgType, "CONTROL") == 0) {
        int ch = doc["ch"];
        bool state = doc["state"];
        if (ch >= 0 && ch < NUM_RELAYS) {
          relayStates[ch] = state;
          digitalWrite(relayPins[ch], state ? RELAY_ON : RELAY_OFF);
        }
      }
      else if (msgType != NULL && strcmp(msgType, "CONTROL_ALL") == 0) {
        bool state = doc["state"];
        for (int i = 0; i < NUM_RELAYS; i++) {
          relayStates[i] = state;
        }
        applyRelays();
      }
      break;
    }

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // ตั้งค่าไฟ HIGH ไว้ล่วงหน้าก่อน pinMode เพื่อป้องกันรีเลย์เด้ง (Flicker) ตอนบูตเครื่อง
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relayPins[i], RELAY_OFF);
    pinMode(relayPins[i], OUTPUT);
  }

  wifiMulti.addAP("OPPO A6 5G d6ah", "vaxe2634");
  wifiMulti.addAP("Network Studio 2.4G", "65001321002");

  Serial.print("Connecting to WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

  webSocket.beginSSL(ws_host, ws_port, ws_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);

  // ส่ง Ping รักษาสาย WebSocket ทุก 15 วินาที
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  if (wifiMulti.run() == WL_CONNECTED) {
    webSocket.loop();
  } else {
    Serial.println("[WiFi] กำลังค้นหาและสลับไปใช้ Wi-Fi สำรอง...");
    delay(1000);
  }
}

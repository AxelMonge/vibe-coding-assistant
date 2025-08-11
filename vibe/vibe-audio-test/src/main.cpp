#include <Arduino.h>
#include <esp_bt.h>  // liberar BLE para dar más RAM al BT clásico
#include <WiFi.h>

#include "config/NetworkConfig.h"
#include "config/HardwareConfig.h"
#include "config/AudioOutputConfig"
#include "config/SystemResources.h"

#include "audio/output/AudioOutputA2DP.h"
#include "network/wifi/WiFiManager.h"
#include "network/websocket/WebSocketClient.h"

// IP de tu PC corriendo test_speaker.py
static const char*    WS_TEST_HOST = "192.168.0.4";
static const uint16_t WS_TEST_PORT = 8010;
static unsigned long lastWsAttempt = 0;

WiFiManager      wifiManager;
AudioOutputA2DP  audioOutput;
WebSocketClient  webSocketClient;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n--- Vibe Node: SPEAKER TEST MODE (demo / WROOM) ---");

  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);   // lo mantenemos

  audioOutput.begin(BT_SPEAKER_NAME, AUDIO_BUFFER_SIZE); // A2DP primero
  delay(300);

  wifiManager.connect();     // Wi-Fi después
  // NO llamar a WiFi.setSleep(false);  // <- quitarlo
  // sin delays extra

  // WS tal cual
  Serial.printf("Vibe Node: WS Speaker Test → ws://%s:%u/ws/speaker-test?file=audio_test.mp3&frame_ms=20&loop=1\n",
                WS_TEST_HOST, WS_TEST_PORT);
  webSocketClient.startSpeakerTest(WS_TEST_HOST, WS_TEST_PORT);

  Serial.println("--- Listo: frames BIN → A2DP ---");
}

void loop() {
  webSocketClient.loop();  // solo bombear WS
  delay(10);               // volvemos al 10 ms
}
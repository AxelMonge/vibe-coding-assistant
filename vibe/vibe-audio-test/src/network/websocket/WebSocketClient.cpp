#include "WebSocketClient.h"
#include <Arduino.h>
#include <WebSocketsClient.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

#include "../../config/SystemResources.h"  // extern RingbufHandle_t g_audio_buffer;

static WebSocketsClient g_ws;

static void ring_flush() {
  if (!g_audio_buffer) return;
  size_t sz; void* item;
  while ((item = xRingbufferReceive(g_audio_buffer, &sz, 0)) != nullptr) {
    vRingbufferReturnItem(g_audio_buffer, item);
  }
}

// Handler de eventos del WebSocket (Links2004)
static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] disconnected");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] connected: %s\n", (const char*)payload);
      break;
    case WStype_TEXT:
      Serial.printf("[WS] text: %.*s\n", (int)length, (const char*)payload);
      if (length >= 11 && strstr((const char*)payload, "audio.start")) {
        ring_flush();
        }
      break;
    case WStype_BIN:
      if (g_audio_buffer && length) {
        BaseType_t ok = xRingbufferSend(g_audio_buffer, payload, length, 0);
        if (ok != pdTRUE) {
          // overflow -> se descarta el frame (no bloquear)
          // Serial.printf("[WS] ring overflow (%u bytes)\n", (unsigned)length);
        }
      }
      break;
    default:
      break;
  }
}

// ===== Implementación de WebSocketClient =====

WebSocketClient::WebSocketClient() {
  // impl_ no usado con Links2004; lo dejamos nulo
  impl_ = nullptr;
}

WebSocketClient::~WebSocketClient() {
  stop();
}

void WebSocketClient::start() {
  // MODO NORMAL (backend principal) — no se usa en speaker test
}

void WebSocketClient::startSpeakerTest(const char* host, uint16_t port) {
  stop(); // estado limpio

  // Frames de 20 ms para menor jitter
  String path = F("/ws/speaker-test?file=audio_test.mp3&frame_ms=20&loop=1");

  Serial.printf("[WS] begin ws://%s:%u%s\n", host, port, path.c_str());

  g_ws.begin(host, port, path.c_str());   // WS plano (sin TLS)
  g_ws.onEvent(onWsEvent);
  g_ws.setReconnectInterval(5000);        // reconexión suave
  // g_ws.enableHeartbeat(...)  // ⛔️ desactivado para evitar writes salientes
}

void WebSocketClient::stop() {
  if (g_ws.isConnected()) {
    g_ws.disconnect();
  }
  // no hay recursos extra que destruir con Links2004
}

void WebSocketClient::loop() {
  g_ws.loop(); // debe llamarse frecuentemente desde loop()
}

bool WebSocketClient::isConnected() const {
  return g_ws.isConnected();
}

bool WebSocketClient::sendAudioChunk(const uint8_t* data, size_t length) {
  if (!data || !length || !g_ws.isConnected()) return false;
  g_ws.sendBIN(data, length);
  return true;
}

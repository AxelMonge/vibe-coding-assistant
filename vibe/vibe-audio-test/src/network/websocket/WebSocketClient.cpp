// ============================================================================
// File:    WebSocketClient.cpp
// Author:  Vibe / Jarvis Audio Node
// Purpose: Implementación del wrapper OO sobre esp_websocket_client con
//          control de flujo por "créditos" para streaming binario de audio.
// ============================================================================

#include <Arduino.h>                   // (orden importante para evitar choques inet.h)
#include "network/websocket/WebSocketClient.h"
#include <cstring>

extern "C" {
  #include "esp_log.h"
}

#include "../../config/NetworkConfig.h"   // WS_URL centralizado
#include "../../config/SystemResources.h" // extern RingbufHandle_t g_audio_buffer
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------
static const char* TAG_WS = "WS";

// Bandera estática de conexión (se toca desde el loop de eventos del WS)
volatile bool WebSocketClient::is_connected = false;

// -----------------------------------------------------------------------------
// Ctor
// -----------------------------------------------------------------------------
WebSocketClient::WebSocketClient() : client(nullptr) {}

// -----------------------------------------------------------------------------
// Tuning público
// -----------------------------------------------------------------------------
void WebSocketClient::setChunkBytes(size_t bytes) {
  if (bytes >= 256 && bytes <= 4096) {
    chunk_bytes = bytes;
  }
}

void WebSocketClient::setMaxCredits(uint8_t max) {
  if (max >= 1 && max <= 64) {
    max_credits = max;
    if (low_watermark >= max_credits) {
      low_watermark = max_credits > 1 ? (uint8_t)(max_credits / 3) : 0;
    }
  }
}

void WebSocketClient::setLowWatermark(uint8_t low) {
  if (low >= 1 && low < max_credits) {
    low_watermark = low;
  }
}

// -----------------------------------------------------------------------------
// Manejador de eventos (estático)
// -----------------------------------------------------------------------------
void WebSocketClient::eventHandler(void *handler_args,
                                   esp_event_base_t /*base*/,
                                   int32_t event_id,
                                   void *event_data) {
  WebSocketClient* self = reinterpret_cast<WebSocketClient*>(handler_args);
  auto *data = reinterpret_cast<esp_websocket_event_data_t*>(event_data);

  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      WebSocketClient::is_connected = true;
  ESP_LOGI(TAG_WS, "Conectado a %s (reinicio de créditos)", NC_WebSocketURL());
  if (self) self->outstanding_credits = 0;
      break;

    case WEBSOCKET_EVENT_DISCONNECTED:
      WebSocketClient::is_connected = false;
  ESP_LOGW(TAG_WS, "Desconectado de %s (se ponen créditos=0)", NC_WebSocketURL());
      if (self) self->outstanding_credits = 0;
      break;

    case WEBSOCKET_EVENT_DATA:
      if (!self || !data) break;
      if (data->op_code == 2) {
        // Binario
        self->handleBinaryMessage((const uint8_t*)data->data_ptr, data->data_len);
      } else if (data->op_code == 1) {
        // Texto (no null-terminated)
        self->handleTextMessage((const char*)data->data_ptr, data->data_len);
      }
      break;

    case WEBSOCKET_EVENT_ERROR:
      WebSocketClient::is_connected = false;
      ESP_LOGE(TAG_WS, "Error en WebSocket");
      if (self) self->outstanding_credits = 0;
      break;

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// API principal
// -----------------------------------------------------------------------------
void WebSocketClient::start() {
  if (client != nullptr) {
    ESP_LOGW(TAG_WS, "start() ignorado: cliente ya inicializado");
    return;
  }

  esp_websocket_client_config_t cfg = {};
  cfg.uri                    = NC_WebSocketURL();
  cfg.disable_auto_reconnect = false;  // queremos autoreconexión
  // NOTA: no usar reconnect_timeout_ms / network_timeout_ms (no existen en tu core)
  cfg.ping_interval_sec      = 15;
  cfg.pingpong_timeout_sec   = 30;

  client = esp_websocket_client_init(&cfg);
  if (!client) {
    ESP_LOGE(TAG_WS, "No se pudo inicializar el cliente WebSocket");
    return;
  }

  esp_websocket_register_events(client,
                                WEBSOCKET_EVENT_ANY,
                                &WebSocketClient::eventHandler,
                                this);

  esp_err_t err = esp_websocket_client_start(client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_WS, "Fallo al iniciar WebSocket: %d", (int)err);
    esp_websocket_client_destroy(client);
    client = nullptr;
    return;
  }

  ESP_LOGI(TAG_WS, "Iniciando conexión a %s (auto-reconnect ON, ping=15s)", NC_WebSocketURL());
}

bool WebSocketClient::isConnected() const {
  return (client != nullptr) && WebSocketClient::is_connected;
}

void WebSocketClient::sendText(const char* text) {
  if (!isConnected() || !text) return;
  int len = strlen(text);
  if (len <= 0) return;

  int sent = esp_websocket_client_send_text(client, text, len, pdMS_TO_TICKS(1000));
  if (sent < 0) {
  ESP_LOGW(TAG_WS, "Fallo al enviar texto (%d) -> '%s'", sent, text);
  }
}

void WebSocketClient::sendAudioChunk(const int16_t* buffer, size_t length) {
  if (!isConnected() || !buffer || length == 0) return;

  size_t byte_len = length * sizeof(int16_t);
  const uint8_t* p = (const uint8_t*)buffer;

  while (byte_len > 0) {
    size_t chunk = (byte_len > chunk_bytes) ? chunk_bytes : byte_len;
    int sent = esp_websocket_client_send_bin(client, (const char*)p, chunk, pdMS_TO_TICKS(1000));
    if (sent < 0) {
  ESP_LOGW(TAG_WS, "Fallo al enviar chunk audio=%uB (timeout o socket cerrado)", (unsigned)chunk);
      break;
    }
    p        += chunk;
    byte_len -= chunk;
  }
}

// -----------------------------------------------------------------------------
// Protocolo de créditos
// -----------------------------------------------------------------------------
void WebSocketClient::sendCredits(uint8_t n) {
  if (!isConnected() || n == 0) return;

  char msg[32];
  int len = snprintf(msg, sizeof(msg), "CREDIT %u", (unsigned)n);
  int sent = esp_websocket_client_send_text(client, msg, len, pdMS_TO_TICKS(500));
  if (sent >= 0) {
    outstanding_credits += n;
    unsigned long now = millis();
    if (now - last_credit_log_ms > 1000) {
      last_credit_log_ms = now;
  ESP_LOGI(TAG_WS, "CREDIT enviados=%u | ahora pendientes=%u (max=%u, chunk=%uB)", (unsigned)n, (unsigned)outstanding_credits, (unsigned)max_credits, (unsigned)chunk_bytes);
    }
  } else {
  ESP_LOGW(TAG_WS, "No se pudo enviar CREDIT (%d) (WS conectado=%d)", sent, (int)isConnected());
  }
}

uint8_t WebSocketClient::computeInitialCredits() {
  if (!g_audio_buffer) return max_credits;

  // Calcula cuántos chunks de chunk_bytes caben ahora mismo
  size_t free_sz = xRingbufferGetCurFreeSize(g_audio_buffer);
  uint8_t can_hold = (uint8_t)(free_sz / chunk_bytes);
  if (can_hold == 0) can_hold = 1;
  if (can_hold > max_credits) can_hold = max_credits;
  return can_hold;
}

void WebSocketClient::maybeTopUpCredits() {
  if (!g_audio_buffer) return;

  if (outstanding_credits <= low_watermark) {
    size_t free_sz = xRingbufferGetCurFreeSize(g_audio_buffer);
    uint8_t can_hold = (uint8_t)(free_sz / chunk_bytes);

    if (can_hold == 0) return;

    // No exceder max_credits totales
    uint8_t want = (outstanding_credits >= max_credits)
                   ? 0
                   : (uint8_t)(max_credits - outstanding_credits);

    if (want == 0) return;
    if (want > can_hold) want = can_hold;

    sendCredits(want);
  }
}

// -----------------------------------------------------------------------------
// Handlers de mensajes
// -----------------------------------------------------------------------------
void WebSocketClient::handleTextMessage(const char* data, size_t len) {
  // `data` no está null-terminated. Copiamos a un buffer pequeño y terminamos.
  char tmp[64];
  size_t n = (len < sizeof(tmp) - 1) ? len : (sizeof(tmp) - 1);
  if (n > 0) {
    memcpy(tmp, data, n);
  }
  tmp[n] = '\0';

  // Normalizamos espacios
  String s;
  s.reserve(n + 1);
  for (size_t i = 0; i < n; ++i) s += tmp[i];
  s.trim();

  // Comandos básicos
  if (s.equalsIgnoreCase("START_STREAM")) {
    uint8_t init = computeInitialCredits();
  ESP_LOGI(TAG_WS, "RX START_STREAM -> calculo créditos iniciales=%u (buffer libre / chunk)", (unsigned)init);
  sendCredits(init);
  } else if (s.equalsIgnoreCase("END_STREAM")) {
  ESP_LOGI(TAG_WS, "RX END_STREAM -> no se pedirán más créditos nuevos");
    // Podemos decidir no pedir más créditos; el servidor dejará de enviar.
  } else {
    ESP_LOGD(TAG_WS, "Texto RX: %s", s.c_str());
  }
}

void WebSocketClient::handleBinaryMessage(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;

  // Encolar en el ringbuffer por bloques de chunk_bytes (por si el server manda grande)
  size_t wrote_total = 0;
  const uint8_t* p = data;
  size_t remain = len;

  while (remain > 0) {
    size_t blk = (remain > chunk_bytes) ? chunk_bytes : remain;
    BaseType_t ok = xRingbufferSend(g_audio_buffer, p, blk, pdMS_TO_TICKS(2));
    if (ok != pdTRUE) {
      ESP_LOGW(TAG_WS, "Ringbuffer lleno, descartando %u bytes", (unsigned)remain);
      break;
    }
    wrote_total += blk;
    p           += blk;
    remain      -= blk;
  }

  if (wrote_total > 0) {
    // 1 crédito por chunk/process del servidor; si este nos manda más de 1 blk,
    // contamos 1 crédito consumido por mensaje WS recibido (ajustable si fuese necesario).
    if (outstanding_credits > 0) outstanding_credits--;

  size_t free_sz = xRingbufferGetCurFreeSize(g_audio_buffer);
  ESP_LOGD(TAG_WS, "Audio RX encolado=%uB | créditos_pend=%u | libre_ring=%uB", (unsigned)wrote_total, (unsigned)outstanding_credits, (unsigned)free_sz);

    // Reponer si caemos bajo el umbral y hay espacio
    maybeTopUpCredits();
  }
}

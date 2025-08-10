// ============================================================================
// File:    src/main.cpp
// Author:  Vibe / Jarvis Audio Node
// Purpose: Punto de entrada del firmware. Orquesta:
//            - Conexión Wi-Fi
//            - Arranque de A2DP (Bluetooth Source) hacia un altavoz
//            - Conexión WebSocket al servidor Python
//            - Máquina de estados para handshake y streaming de audio
//
// Notas clave de diseño:
//   * Se conecta por MAC al altavoz (BD_ADDR) para evitar discovery (más estable).
//   * No hacemos end()/restart de A2DP en caliente (evita carreras en bluedroid).
//   * Control de flujo WS tipo "pull" vía créditos se maneja dentro de WebSocketClient.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>  // WiFi.localIP(), WiFi.status()

extern "C" {
  #include "nvs_flash.h"        // nvs_flash_init
  #include "esp_bt.h"           // esp_bt_controller_mem_release
  #include "esp_bt_device.h"    // esp_bt_dev_get_address
}

#include "audio/output/AudioOutputA2DP.h"
#include "network/websocket/WebSocketClient.h"
#include "network/wifi/WiFiManager.h"
#include "config/SystemResources.h"   // extern RingbufHandle_t g_audio_buffer
#include "config/NetworkConfig.h"     // Credenciales y constantes centralizadas

// --- Estado global del sistema ---
enum SystemState : uint8_t {
  STATE_AWAITING_WIFI = 0,
  STATE_AWAITING_WEBSOCKET,
  STATE_AWAITING_BLUETOOTH,
  STATE_SENDING_HANDSHAKE,
  STATE_STREAMING_FROM_SERVER,
  STATE_LISTENING_TO_MIC  // (pendiente: estabilizar salida primero)
};

volatile SystemState currentState = STATE_AWAITING_WIFI;

// --- Recursos globales ---
WiFiManager      wifiManager;
WebSocketClient  webSocketClient;
AudioOutputA2DP  audioOutput;

// Definición del ring buffer global (declarado extern en SystemResources.h)
RingbufHandle_t g_audio_buffer = nullptr;

// --- Log de estado periódico enriquecido ---
static unsigned long status_last_log_ms = 0;
static size_t g_audio_buffer_capacity = 0; // se fija al crear el ring buffer

static const char* stateToStr(SystemState st) {
  switch (st) {
    case STATE_AWAITING_WIFI: return "ESPERANDO_WIFI";
    case STATE_AWAITING_WEBSOCKET: return "ESPERANDO_WS";
    case STATE_AWAITING_BLUETOOTH: return "ESPERANDO_BT";
    case STATE_SENDING_HANDSHAKE: return "ENVIANDO_HANDSHAKE";
    case STATE_STREAMING_FROM_SERVER: return "STREAMING_SRV";
    case STATE_LISTENING_TO_MIC: return "CAPTURANDO_MIC";
    default: return "(?)";
  }
}

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

static void log_status_every(unsigned long interval_ms = 5000) {
  unsigned long now = millis();
  if (now - status_last_log_ms < interval_ms) return;
  status_last_log_ms = now;

  bool ws_on = webSocketClient.isConnected();
  bool bt_on = audioOutput.isConnected();
  size_t free_sz = g_audio_buffer ? xRingbufferGetCurFreeSize(g_audio_buffer) : 0;
  size_t used = (g_audio_buffer_capacity > free_sz) ? (g_audio_buffer_capacity - free_sz) : 0;
  uint8_t cred = webSocketClient.getOutstandingCredits();
  uint8_t credMax = webSocketClient.getMaxCredits();
  size_t  chunkB = webSocketClient.getChunkBytes();

  float fillPct = (g_audio_buffer_capacity == 0) ? 0.0f : (100.0f * used / (float)g_audio_buffer_capacity);

  Serial.printf(
    "[STATUS] t=%lums | Estado=%s | WiFi=%s | WS=%s (cred=%u/%u chunk=%uB) | BT=%s | RingBuf=%u/%uB (%.1f%%) | Heap=%uB\n",
    now,
    stateToStr(currentState),
    (WiFi.status() == WL_CONNECTED ? "OK" : "NO"),
    (ws_on ? "ON" : "OFF"),
    (unsigned)cred, (unsigned)credMax, (unsigned)chunkB,
    (bt_on ? "ON" : "OFF"),
    (unsigned)used, (unsigned)g_audio_buffer_capacity, (double)fillPct,
    (unsigned)ESP.getFreeHeap()
  );
}

// ------------------- Arduino setup -------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  // Inicializa NVS temprano (evita warnings de NVS con BT/A2DP)
  nvs_flash_init();

  Serial.println("[INFO] --- Vibe Node: Jarvis Edition (Final Architecture) ---");
  Serial.println("[INFO] State -> AWAITING_WIFI");

  // Liberar memoria reservada para BLE (no se usa) — mejora coexistencia
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  // Conexión Wi-Fi (modo estación + power save MIN_MODEM aplicado en WiFiManager)
  wifiManager.connect();
  Serial.printf("[INFO] WiFi Conectado. IP: %s\n", WiFi.localIP().toString().c_str());

  // MAC local de Bluetooth (diagnóstico)
  if (const uint8_t* btmac = esp_bt_dev_get_address()) {
    Serial.printf("[INFO] BT MAC local: %02X:%02X:%02X:%02X:%02X:%02X\n",
      btmac[0], btmac[1], btmac[2], btmac[3], btmac[4], btmac[5]);
  }

  // Crear ring buffer para audio PCM recibido por WebSocket
  if (!g_audio_buffer) {
    g_audio_buffer = xRingbufferCreate(A2DP_AUDIO_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!g_audio_buffer) {
      Serial.println("[FATAL] No se pudo crear el ring buffer de audio");
      while (true) { delay(1000); }
    }
    g_audio_buffer_capacity = A2DP_AUDIO_BUFFER_SIZE;
  }

  // Inicializa A2DP Source: conectar directamente por MAC del sink (más estable que por nombre)
  Serial.printf("[BT] Iniciando A2DP hacia '%s' (buffer interno=%uB) ...\n", BT_SPEAKER_NAME, (unsigned)A2DP_AUDIO_BUFFER_SIZE);
  audioOutput.begin(BT_SPEAKER_NAME, A2DP_AUDIO_BUFFER_SIZE);
  audioOutput.setVolume(A2DP_DEFAULT_VOLUME);

  // Arranca WebSocket (autoreconexión activada en el wrapper)
  Serial.println("[INFO] State -> AWAITING_WEBSOCKET");
  currentState = STATE_AWAITING_WEBSOCKET;
  webSocketClient.start();
}

// ------------------- Arduino loop: máquina de estados -------------------
void loop() {
  log_status_every(5000);

  switch (currentState) {
    case STATE_AWAITING_WIFI: {
      // (Normalmente ya estamos conectados; esto es solo un fallback)
      if (WiFi.status() == WL_CONNECTED) {
        currentState = STATE_AWAITING_WEBSOCKET;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    }

    case STATE_AWAITING_WEBSOCKET: {
      if (webSocketClient.isConnected()) {
  Serial.println("[FSM] WebSocket arriba -> esperando Bluetooth");
        currentState = STATE_AWAITING_BLUETOOTH;
      }
      vTaskDelay(pdMS_TO_TICKS(20));
      break;
    }

    case STATE_AWAITING_BLUETOOTH: {
      // Cuando A2DP reporte CONNECTED (stream listo), pasamos a handshake
      if (audioOutput.isStreamReady()) {
  Serial.println("[FSM] Bluetooth listo -> enviando handshake al servidor");
        currentState = STATE_SENDING_HANDSHAKE;
        break;
      }

      // Mientras esté CONNECTING, no tocar nada (evita carreras/crashes).
      if (audioOutput.isConnecting()) {
        vTaskDelay(pdMS_TO_TICKS(50));
        break;
      }

      // Sin reintentos destructivos: la librería auto-reconecta y ya conectamos por MAC.
      vTaskDelay(pdMS_TO_TICKS(50));
      break;
    }

    case STATE_SENDING_HANDSHAKE: {
      if (webSocketClient.isConnected()) {
  Serial.println("[FSM] Enviando 'READY' (handshake WS)...");
        webSocketClient.sendText("READY");
        vTaskDelay(pdMS_TO_TICKS(50)); // da tiempo a vaciar el buffer de red
  Serial.println("[FSM] Handshake confirmado local -> entrando a STREAMING_FROM_SERVER");
        currentState = STATE_STREAMING_FROM_SERVER;
      } else {
        // Si WS cayó, vuelve a esperar WS
        currentState = STATE_AWAITING_WEBSOCKET;
      }
      break;
    }

    case STATE_STREAMING_FROM_SERVER: {
      // El callback WS mete PCM en g_audio_buffer; el callback A2DP lo consume.
      // Monitorea desconexiones para reentrar a estados previos.
      if (!webSocketClient.isConnected()) {
  Serial.println("[FSM][WARN] WebSocket se perdió -> regresando a ESPERANDO_WS");
        currentState = STATE_AWAITING_WEBSOCKET;
      } else if (!audioOutput.isConnected()) {
  Serial.println("[FSM][WARN] Bluetooth se perdió -> regresando a ESPERANDO_BT");
        currentState = STATE_AWAITING_BLUETOOTH;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      break;
    }

    case STATE_LISTENING_TO_MIC: {
      // (Reservado para fase de entrada; desactivado por ahora)
      vTaskDelay(pdMS_TO_TICKS(10));
      break;
    }

    default:
      vTaskDelay(pdMS_TO_TICKS(10));
      break;
  }
}

#include "AudioOutputA2DP.h"
#include <Arduino.h>
#include <cstring>  // memset, memcpy, memmove

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
  #include "esp_bt_defs.h"
}

// Recurso global de tu proyecto (debe declarar: extern RingbufHandle_t g_audio_buffer;)
#include "../../config/SystemResources.h"

// Logs de estado BT (opcionales)
static void on_conn_state(esp_a2d_connection_state_t state, void*) {
  // 0=Disconnected, 1=Connecting, 2/3=Connected (según lib), 4=Disconnecting
  Serial.printf("[A2DP] conn_state=%d\n", (int)state);
}
static void on_audio_state(esp_a2d_audio_state_t state, void*) {
  // 0=Stopped, 1=Started, 2=Suspended
  Serial.printf("[A2DP] audio_state=%d\n", (int)state);
}

AudioOutputA2DP* AudioOutputA2DP::instance = nullptr;

AudioOutputA2DP::AudioOutputA2DP()
: audio_buffer(nullptr) {
  instance = this;
}

AudioOutputA2DP::~AudioOutputA2DP() {
  this->end(); // evitar colisión con std::end
}

void AudioOutputA2DP::begin(const char* device_name, size_t buffer_size_bytes) {
  // Configurar callbacks antes de arrancar BT
  a2dp_source.set_data_callback(AudioOutputA2DP::audio_data_callback);
  a2dp_source.set_on_connection_state_changed(on_conn_state);
  a2dp_source.set_on_audio_state_changed(on_audio_state);
  a2dp_source.set_auto_reconnect(true);
  a2dp_source.set_volume(80); // 0..127
  a2dp_source.set_pin_code("0000", ESP_BT_PIN_TYPE_FIXED);  // prueba "1234" si no enlaza

  Serial.printf("[A2DP] start → '%s'\n", device_name);
  a2dp_source.start(device_name);     // ⬅️ arranca BT cuando aún hay memoria libre
  delay(150);                         // respiro corto

  // Ahora sí: crea el ring (más pequeño para probar) y expón global
  audio_buffer = xRingbufferCreate(buffer_size_bytes, RINGBUF_TYPE_BYTEBUF);
  if (!audio_buffer) {
    Serial.println("[A2DP] FATAL: no se pudo crear ring buffer");
    delay(100);
    ESP.restart();
  }
  g_audio_buffer = audio_buffer;

  Serial.printf("[A2DP] ring listo (%u bytes)\n", (unsigned)buffer_size_bytes);
}

void AudioOutputA2DP::end() {
  // Detener BT
  a2dp_source.end();

  // Liberar ring local
  if (audio_buffer) {
    vRingbufferDelete(audio_buffer);
    audio_buffer = nullptr;
  }

  // Limpiar handle global si apuntaba a este ring
  if (g_audio_buffer) g_audio_buffer = nullptr;
}

size_t AudioOutputA2DP::write(const uint8_t* data, size_t len) {
  if (!audio_buffer || !data || !len) return 0;
  // Envío no bloqueante; si está lleno, descarta el frame (caller puede contar drops)
  BaseType_t ok = xRingbufferSend(audio_buffer, (void*)data, len, 0);
  return (ok == pdTRUE) ? len : 0;
}

// -----------------------------------------------------------------------------
//  B.9–B.12: A2DP CALLBACK
//  - Drena el ring (alimentado por el WebSocket) y rellena con silencio si falta.
//  - Usa un pequeño "stash" para manejar sobrantes cuando un item del ring es
//    más grande que 'len' solicitado por A2DP.
//  - No bloquea: si no hay datos, produce silencio (evita pops).
// -----------------------------------------------------------------------------
int32_t AudioOutputA2DP::audio_data_callback(uint8_t* out, int32_t len) {
  static uint8_t  stash[8192];
  static size_t   stash_len = 0;
  static uint32_t underruns = 0;

  if (!instance || !instance->audio_buffer || !out || len <= 0) {
    std::memset(out, 0, (size_t)len);
    return len;
  }

  size_t filled = 0;

  // 1) Consumir lo que haya en stash primero
  if (stash_len) {
    size_t take = (stash_len < (size_t)len) ? stash_len : (size_t)len;
    std::memcpy(out, stash, take);
    filled += take;
    stash_len -= take;
    if (stash_len) {
      // compactar remanente
      std::memmove(stash, stash + take, stash_len);
    }
  }

  // 2) Sacar items del ring hasta llenar o vaciar
  while (filled < (size_t)len) {
    size_t item_size = 0;
    uint8_t* item = (uint8_t*) xRingbufferReceive(instance->audio_buffer, &item_size, 0 /* no bloquear */);
    if (!item || item_size == 0) break;

    size_t need = (size_t)len - filled;
    if (item_size <= need) {
      std::memcpy(out + filled, item, item_size);
      filled += item_size;
      vRingbufferReturnItem(instance->audio_buffer, item);
    } else {
      // Cabeza al out, cola al stash
      std::memcpy(out + filled, item, need);
      size_t leftover = item_size - need;
      if (leftover > sizeof(stash)) leftover = sizeof(stash);
      std::memcpy(stash, item + need, leftover);
      stash_len = leftover;
      filled += need;
      vRingbufferReturnItem(instance->audio_buffer, item);
      break;
    }
  }

  // 3) Silencio si faltó data
  if (filled < (size_t)len) {
    std::memset(out + filled, 0, (size_t)len - filled);
    if ((++underruns % 200) == 0) { // log ocasional para no saturar
      Serial.printf("[A2DP] underruns total=%u\n", underruns);
    }
  }
  return len;
}

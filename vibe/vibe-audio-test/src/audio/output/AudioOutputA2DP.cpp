// ============================================================================
// File:    AudioOutputA2DP.cpp
// Author:  Vibe / Jarvis Audio Node
// Purpose: Implementación de salida Bluetooth A2DP (perfil "source") para
//          reproducir audio en un altavoz/auricular desde ESP32. Extrae PCM
//          de un ring buffer global y maneja estados de conexión de forma
//          segura (sin carreras con el stack BT).
// ============================================================================

#include "audio/output/AudioOutputA2DP.h"

#include <Arduino.h>
#include <cstring>
#include <algorithm>

#include "config/SystemResources.h" // extern RingbufHandle_t g_audio_buffer

// Instancia global accesible desde callbacks C de la librería
AudioOutputA2DP* AudioOutputA2DP::instance = nullptr;

// -----------------------------------------------------------------------------
// Callbacks de estado de conexión/audio (invocados por la librería A2DP)
// -----------------------------------------------------------------------------

/**
 * @brief Notificación de cambio de estado de conexión A2DP.
 *
 * Importante: este callback corre en el hilo interno de BT (btc_task).
 * No llamar aquí a end()/start() ni a funciones que destruyan recursos.
 * Sólo marcar flags y estados para ser atendidos luego en el loop principal.
 */
static const char* connStateToStr(esp_a2d_connection_state_t st) {
    switch (st) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:   return "DESCONECTADO";
        case ESP_A2D_CONNECTION_STATE_CONNECTING:     return "CONECTANDO";
        case ESP_A2D_CONNECTION_STATE_CONNECTED:      return "CONECTADO";
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:  return "DESCONECTANDO";
        default: return "(estado?)";
    }
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
    (void)ptr;
    if (!AudioOutputA2DP::instance) return;

    AudioOutputA2DP::instance->conn_state = state;
    Serial.printf("[BT] Estado conexión A2DP -> %s (%d)\n", connStateToStr(state), (int)state);

    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.println("[BT] Altavoz conectado (stream listo para handshake)");
            AudioOutputA2DP::instance->setStreamReady(true);
            break;

        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.println("[BT] Altavoz desconectado (se limpiará stream_ready)");
            AudioOutputA2DP::instance->setStreamReady(false);
            // Señalamos deseo de reinicio; se decidirá en el loop principal
            AudioOutputA2DP::instance->onDisconnectedCallback();
            break;

        default:
            break;
    }
}

/**
 * @brief Notificación de cambio de estado de audio (STARTED/SUSPENDED/STOPPED).
 * Sólo para logging/telemetría.
 */
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
    (void)ptr;
    Serial.printf("[BT] Estado de audio A2DP: %d\n", (int)state);
}

// -----------------------------------------------------------------------------
// Ciclo de vida
// -----------------------------------------------------------------------------

AudioOutputA2DP::AudioOutputA2DP() {}

AudioOutputA2DP::~AudioOutputA2DP() {
    // Cierre limpio si estaba iniciado
    if (started) {
        a2dp_source.end();
        started = false;
        stream_is_ready = false;
        conn_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    }
}

/**
 * @brief Inicia el A2DP Source y la conexión al sink.
 *
 * Si device_name contiene ':' se interpreta como MAC y se conecta directo,
 * evitando discovery (recomendado para estabilidad/latencia).
 */
void AudioOutputA2DP::begin(const char* device_name, size_t buffer_size_bytes) {
    if (started) {
        Serial.println("[BT] begin() ignorado: A2DP ya estaba iniciado.");
        return;
    }

    instance = this;
    app_buffer_size = buffer_size_bytes;

    // Registrar callbacks
    a2dp_source.set_data_callback(&AudioOutputA2DP::audio_data_callback);
    a2dp_source.set_on_connection_state_changed(connection_state_changed, this);
    a2dp_source.set_on_audio_state_changed(audio_state_changed, this);

    // Dejar autoreconectar a la librería (evita que nosotros forcemos end() en caliente)
    a2dp_source.set_auto_reconnect(true);

    // Iniciar conexión (por nombre o MAC)
    a2dp_source.start(device_name);

    started = true;
    conn_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED; // hasta recibir callback
    Serial.printf("[BT] A2DP Source iniciado hacia '%s'\n", device_name);
}

/**
 * @brief Ajusta volumen vía AVRCP (0..100 aprox, mapeado por la lib).
 */
void AudioOutputA2DP::setVolume(uint8_t volume) {
    a2dp_source.set_volume(volume);
}

/**
 * @brief Consulta si el sink está conectado (consulta directa a la lib).
 */
bool AudioOutputA2DP::isConnected() {
    return a2dp_source.is_connected();
}

/**
 * @brief Flag de “listo para handshake” con el servidor (cuando ya hay conexión BT).
 */
void AudioOutputA2DP::setStreamReady(bool ready) {
    stream_is_ready = ready;
}

bool AudioOutputA2DP::isStreamReady() const {
    return stream_is_ready;
}

/**
 * @brief Se invoca desde el callback de desconexión. No destruye nada.
 * Sólo marca intención de reiniciar y limpia flags.
 */
void AudioOutputA2DP::onDisconnectedCallback() {
    stream_is_ready = false;
    want_restart = true; // el loop decidirá si reintenta (nunca durante CONNECTING)
}

/**
 * @brief Reintento seguro: end()+begin() sólo si NO estamos en CONNECTING.
 * Úsalo desde el loop principal, no desde callbacks.
 */
bool AudioOutputA2DP::restartIfSafe(const char* device_name, size_t buffer_size_bytes) {
    if (conn_state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
        // Evitamos carreras: no reiniciar a mitad de CONNECTING
        return false;
    }
    if (started) {
        a2dp_source.end();
        started = false;
        stream_is_ready = false;
        conn_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
        vTaskDelay(pdMS_TO_TICKS(150)); // respiro al controlador BT
    }
    want_restart = false;
    begin(device_name, buffer_size_bytes);
    return true;
}

// -----------------------------------------------------------------------------
// Callback de datos (PCM → A2DP). La librería solicita 'len' bytes.
// Esta función debe devolver exactamente 'len' (zeros si no hay suficientes).
// -----------------------------------------------------------------------------

int32_t AudioOutputA2DP::audio_data_callback(uint8_t* data, int32_t len) {
    // Pequeño "carry" por si recibimos más bytes de los que caben en esta
    // llamada; lo guardamos para la siguiente invocación.
    static uint8_t carry_buf[4096];
    static size_t  carry_len = 0;

    if (len <= 0) return 0;

    int32_t produced = 0;

    // 1) Atender primero restos pendientes en carry_buf
    if (carry_len > 0) {
        size_t to_copy = (carry_len < (size_t)len) ? carry_len : (size_t)len;
        memcpy(data, carry_buf, to_copy);
        produced += (int32_t)to_copy;

        if (to_copy < carry_len) {
            // Aún queda "carry" para la próxima
            memmove(carry_buf, carry_buf + to_copy, carry_len - to_copy);
            carry_len -= to_copy;
        } else {
            carry_len = 0;
        }
    }

    // 2) Drenar del ringbuffer global hasta completar 'len' bytes
    while (produced < len) {
        size_t item_size = 0;
        uint8_t* item = (uint8_t*) xRingbufferReceive(g_audio_buffer, &item_size, pdMS_TO_TICKS(1));
        if (!item || item_size == 0) {
            break; // sin datos ahora mismo
        }

        size_t space   = (size_t)(len - produced);
        size_t to_copy = (item_size < space) ? item_size : space;

        memcpy(data + produced, item, to_copy);
        produced += (int32_t)to_copy;

        if (item_size > to_copy) {
            // Sobra parte del item: guardarlo para la próxima llamada
            size_t remain = item_size - to_copy;
            size_t spill  = std::min(remain, sizeof(carry_buf));
            memcpy(carry_buf, item + to_copy, spill);
            carry_len = spill;
        }

        // Devolver bloque al ringbuffer
        vRingbufferReturnItem(g_audio_buffer, (void*)item);

        if (produced >= len) break;
    }

    // 3) Si no llenamos, completar con silencios para evitar underrun SBC
    if (produced < len) {
        memset(data + produced, 0, (size_t)(len - produced));
        produced = len;
    }

    return produced;
}

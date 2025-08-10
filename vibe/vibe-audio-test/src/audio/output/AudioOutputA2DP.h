// ============================================================================
// File:    AudioOutputA2DP.h
// Author:  Vibe / Jarvis Audio Node
// Purpose: Encapsula la salida de audio Bluetooth A2DP (perfil "source")
//          para reproducir en un altavoz/auricular Bluetooth desde un ESP32.
//          Integra un callback de datos que extrae PCM de un ring buffer
//          global (g_audio_buffer) y gestiona de forma segura los estados
//          de conexión evitando carreras con el stack BT (Bluedroid).
//
// Diseño (resumen):
//  - begin(device, bufsize): inicializa A2DP Source y arranca la conexión.
//      * Si 'device' contiene ':', se interpreta como MAC/BD_ADDR y se conecta
//        directamente sin discovery (recomendado para estabilidad).
//  - audio_data_callback(): solicitado por la librería A2DP; copia del ring
//    buffer al buffer de salida que envía SBC por A2DP. Rellena con cero si
//    faltan bytes para evitar underruns.
//  - Callbacks de estado (connection_state_changed / audio_state_changed):
//      * NO tocan el stack (no llaman end/start); solo marcan flags/estado.
//  - Métodos de consulta: isConnected(), isConnecting(), isDisconnected(),
//    isStreamReady().
//  - restartIfSafe(): helper para reintentos controlados desde el loop
//    principal, nunca desde dentro de callbacks, evitando carreras.
//
// Notas de concurrencia:
//  - Los callbacks BT se ejecutan en el hilo interno 'btc_task' de Bluetooh.
//    Evitamos operaciones destructivas ahí (como end()) para no provocar
//    LoadProhibited por estructuras liberadas mientras el hilo las usa.
//
// Dependencias externas relevantes (no incluidas aquí):
//  - g_audio_buffer (RingbufHandle_t) declarado en SystemResources.h y creado
//    en setup() de main.cpp.
// ============================================================================

#ifndef AUDIO_OUTPUT_A2DP_H
#define AUDIO_OUTPUT_A2DP_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h"

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
  #include "esp_a2dp_api.h"
}

// -----------------------------------------------------------------------------
// Forward declarations de callbacks C-style requeridos por la librería
// -----------------------------------------------------------------------------
class AudioOutputA2DP;
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr);
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr);

// -----------------------------------------------------------------------------
// Clase AudioOutputA2DP
// -----------------------------------------------------------------------------
class AudioOutputA2DP {
public:
    // Instancia global opcional para acceso desde callbacks C
    static AudioOutputA2DP* instance;

    AudioOutputA2DP();
    ~AudioOutputA2DP();

    /**
     * @brief Inicia el A2DP Source y arranca la conexión al sink.
     *
     * @param device_name  Nombre del dispositivo destino o su MAC (BD_ADDR).
     *                     Si contiene ':', se asume formato MAC y se conecta
     *                     directamente (sin discovery).
     * @param buffer_size_bytes  Tamaño del ring buffer de audio de la app
     *                           (solo informativo/diagnóstico aquí).
     */
    void begin(const char* device_name, size_t buffer_size_bytes);

    /**
     * @brief Ajusta volumen vía AVRCP (0..100 aprox. mapeado por la lib).
     */
    void setVolume(uint8_t volume);

    /**
     * @brief Estado de conexión al sink (consulta a la lib).
     */
    bool isConnected();

    /**
     * @brief Estado inicializado/arrancado del A2DP Source.
     */
    bool isStarted() const { return started; }

    /**
     * @brief Señala si el stack reporta CONNECTING (evita reintentos destructivos).
     */
    bool isConnecting() const { return conn_state == ESP_A2D_CONNECTION_STATE_CONNECTING; }

    /**
     * @brief Señala si el stack reporta DISCONNECTED.
     */
    bool isDisconnected() const { return conn_state == ESP_A2D_CONNECTION_STATE_DISCONNECTED; }

    /**
     * @brief Marca/consulta si ya podemos enviar el handshake al servidor
     *        (se pone a true al entrar en CONNECTED).
     */
    void setStreamReady(bool ready);
    bool isStreamReady() const;

    /**
     * @brief Señalado desde el callback de desconexión. NO llama end()/start().
     *        Solo marca flags para que el loop decida qué hacer.
     */
    void onDisconnectedCallback();

    /**
     * @brief Reintento seguro desde el loop principal (nunca desde callbacks).
     *        Hace end()+begin() únicamente si NO estamos en CONNECTING.
     *
     * @return true si se realizó el restart, false si se omitió por estado.
     */
    bool restartIfSafe(const char* device_name, size_t buffer_size_bytes);

private:
    // -------------------------------------------------------------------------
    // Callback de datos A2DP (estático): pide 'len' bytes PCM al ring buffer.
    // Devuelve exactamente 'len' (rellenando con ceros si faltan bytes).
    // -------------------------------------------------------------------------
    static int32_t audio_data_callback(uint8_t* data, int32_t len);

    // Permitir a los callbacks C acceder a campos privados de la instancia
    friend void connection_state_changed(esp_a2d_connection_state_t state, void *ptr);
    friend void audio_state_changed(esp_a2d_audio_state_t state, void *ptr);

private:
    BluetoothA2DPSource a2dp_source;

    // Flags/estado de la app
    volatile bool  stream_is_ready = false; // true cuando CONNECTED y listo para handshake
    bool           started         = false; // begin() ejecutado
    size_t         app_buffer_size = 0;     // informativo/diagnóstico

    // Estado de conexión reportado por callbacks BT
    volatile esp_a2d_connection_state_t conn_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;

    // Pedido de restart diferido (marcado en callback, atendido en el loop)
    volatile bool  want_restart = false;
};

#endif // AUDIO_OUTPUT_A2DP_H

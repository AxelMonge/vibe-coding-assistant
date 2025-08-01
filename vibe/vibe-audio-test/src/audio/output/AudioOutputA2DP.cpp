#include "AudioOutputA2DP.h"
#include <Arduino.h>

// Es necesario inicializar el puntero estático a nullptr.
// Este puntero permitirá que la función de callback estática acceda a los
// miembros de la instancia de nuestra clase (como el búfer de audio).
AudioOutputA2DP* AudioOutputA2DP::instance = nullptr;

/**
 * @brief Constructor. Se asegura de que la instancia estática apunte a este objeto.
 */
AudioOutputA2DP::AudioOutputA2DP() : audio_buffer(nullptr) {
    instance = this;
}

/**
 * @brief Destructor. Libera los recursos: detiene el A2DP y borra el búfer.
 */
AudioOutputA2DP::~AudioOutputA2DP() {
    a2dp_source.end();
    if (audio_buffer != nullptr) {
        vRingbufferDelete(audio_buffer);
    }
}

/**
 * @brief Inicializa el sistema de salida de audio.
 */
void AudioOutputA2DP::begin(const char* device_name, size_t buffer_size) {
    // 1. Crear el Ring Buffer de FreeRTOS para almacenar el audio de forma segura.
    //    Usamos RINGBUF_TYPE_BYTEBUF porque tratamos con un flujo de bytes sin formato.
    audio_buffer = xRingbufferCreate(buffer_size, RINGBUF_TYPE_BYTEBUF);
    if (audio_buffer == nullptr) {
        Serial.println("FATAL: No se pudo crear el búfer de audio A2DP. Reiniciando...");
        ESP.restart();
    }

    // 2. Establecer la función de callback. La librería A2DP llamará a esta función
    //    automáticamente cuando necesite más datos de audio para enviar.
    a2dp_source.set_data_callback(audio_data_callback);

    // 3. Iniciar el servicio A2DP y comenzar a buscar el altavoz.
    Serial.printf("Vibe Node: Iniciando Bluetooth A2DP para conectar a '%s'...\n", device_name);
    a2dp_source.start(device_name);

    // Opcional: Establecer un volumen por defecto (0-127)
    a2dp_source.set_volume(80);
}

/**
 * @brief Función para escribir datos en el búfer (Productor).
 * Esta función será llamada por el WebSocket cuando reciba un chunk de audio.
 */
size_t AudioOutputA2DP::write(const uint8_t* data, size_t length) {
    if (audio_buffer == nullptr) return 0;

    // xRingbufferSend es 'thread-safe', por lo que podemos llamarla de forma segura
    // desde el hilo del WebSocket sin interferir con el hilo de Bluetooth.
    // Le damos un pequeño timeout para no bloquear el sistema si el búfer está lleno.
    BaseType_t result = xRingbufferSend(audio_buffer, data, length, pdMS_TO_TICKS(10));

    if (result != pdTRUE) {
        // Esto puede ocurrir si el audio llega más rápido de lo que se puede reproducir.
        // Serial.println("ADVERTENCIA: Búfer de A2DP lleno, paquete descartado.");
        return 0;
    }

    return length;
}

/**
 * @brief Devuelve si el cliente Bluetooth está conectado a un altavoz.
 */
bool AudioOutputA2DP::isConnected() {
    return a2dp_source.is_connected();
}

/**
 * @brief Callback estático de datos de audio (Consumidor).
 * Esta es la función más importante. Es llamada por la librería A2DP en su propio hilo.
 */
int32_t AudioOutputA2DP::audio_data_callback(uint8_t* data, int32_t len) {
    if (instance == nullptr || instance->audio_buffer == nullptr) {
        // Si no hay instancia o búfer, llenar con silencio para evitar ruido.
        memset(data, 0, len);
        return len;
    }

    size_t item_size = 0;
    // Intentar recibir datos del búfer. 'xTicksToWait' se pone en 0 para no bloquear nunca.
    uint8_t* item = (uint8_t*)xRingbufferReceive(instance->audio_buffer, &item_size, 0);

    if (item != nullptr) {
        // Datos recibidos con éxito.
        // Copiar los datos del búfer al búfer de la librería A2DP.
        memcpy(data, item, item_size);
        
        // Devolver el item al búfer para que pueda ser reutilizado.
        vRingbufferReturnItem(instance->audio_buffer, (void*)item);

        // Si los datos que teníamos no llenan todo el búfer solicitado,
        // llenar el resto con silencio para evitar audio "chopeado".
        if (item_size < len) {
            memset(data + item_size, 0, len - item_size);
        }
        return len; // Siempre debemos decir que llenamos todo el búfer.

    } else {
        // No hay datos disponibles en el búfer (underflow).
        // Es CRÍTICO enviar silencio para evitar chasquidos y pops en el altavoz.
        memset(data, 0, len);
        return len;
    }
}
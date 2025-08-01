#ifndef AUDIO_OUTPUT_A2DP_H
#define AUDIO_OUTPUT_A2DP_H

#include <Arduino.h>
#include "BluetoothA2DPSource.h" // Incluir la librería principal de A2DP
#include "freertos/ringbuf.h"   // SOLUCIÓN 1: Incluir para RingbufHandle_t

// SOLUCIÓN 2 y 3: Se eliminaron todas las dependencias de AudioTools (StreamConverter, ResampleStream)

class AudioOutputA2DP {
public:
    AudioOutputA2DP();
    ~AudioOutputA2DP();

    /**
     * @brief Inicializa el búfer y el servicio Bluetooth A2DP.
     * @param device_name El nombre del altavoz Bluetooth al que se conectará.
     * @param buffer_size El tamaño en bytes para el búfer de audio.
     */
    void begin(const char* device_name, size_t buffer_size = 8192);

    /**
     * @brief Escribe datos de audio en el búfer para ser reproducidos.
     * Esta función es segura para ser llamada desde diferentes hilos (thread-safe).
     * @param data Puntero a los datos de audio PCM.
     * @param length Longitud de los datos en bytes.
     * @return El número de bytes que fueron escritos exitosamente en el búfer.
     */
    size_t write(const uint8_t* data, size_t length);

    /**
     * @brief Verifica si el altavoz Bluetooth está conectado.
     * @return true si está conectado, false en caso contrario.
     */
    bool isConnected();

private:
    BluetoothA2DPSource a2dp_source;
    RingbufHandle_t audio_buffer; // El "handle" para nuestro búfer de audio

    // Función de callback estática que se pasa a la librería A2DP
    static int32_t audio_data_callback(uint8_t* data, int32_t len);

    // Puntero estático a la instancia actual para poder usarlo en el callback
    static AudioOutputA2DP* instance;
};

#endif // AUDIO_OUTPUT_A2DP_H
#pragma once
#include <Arduino.h>
#include <BluetoothA2DPSource.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

/**
 * Clase de salida A2DP para el speaker test.
 * - Crea un ring buffer propio para PCM (audio_buffer).
 * - Expone ese ring en g_audio_buffer (definido en SystemResources.h) para que el WS lo alimente.
 * - Por ahora el callback devuelve silencio (B.6–B.8). En B.9 leerá del ring.
 */
class AudioOutputA2DP {
public:
    static AudioOutputA2DP* instance;

    AudioOutputA2DP();
    ~AudioOutputA2DP();

    // Inicia A2DP como SOURCE hacia el dispositivo BT (p.ej. "XH-A158")
    void begin(const char* device_name, size_t buffer_size_bytes);

    // Detiene A2DP y libera el ring
    void end();

    // Encola bytes PCM en el ring (no bloqueante). Retorna bytes aceptados o 0 si ring lleno.
    size_t write(const uint8_t* data, size_t len);

    // Callback que el stack A2DP llama para pedir más PCM
    static int32_t audio_data_callback(uint8_t* data, int32_t len);

private:
    BluetoothA2DPSource a2dp_source;
    RingbufHandle_t     audio_buffer;   // ring local de salida BT
};

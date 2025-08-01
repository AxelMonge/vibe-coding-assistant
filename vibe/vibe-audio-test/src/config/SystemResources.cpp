#ifndef SYSTEM_RESOURCES_H
#define SYSTEM_RESOURCES_H

#include <Arduino.h> // <-- AÑADE ESTA LÍNEA AQUÍ
#include "freertos/ringbuf.h"

// Handle global para el búfer de audio compartido entre WebSocket (que escribe en él)
// y AudioOutputA2DP (que lee de él).
extern RingbufHandle_t g_audio_buffer;
#endif // SYSTEM_RESOURCES_H
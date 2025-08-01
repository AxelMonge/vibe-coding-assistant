#ifndef SYSTEM_RESOURCES_H
#define SYSTEM_RESOURCES_H

#include "freertos/ringbuf.h"

// Handle global para el búfer de audio compartido entre WebSocket (que escribe en él)
// y AudioOutputA2DP (que lee de él).
// 'extern' le dice al compilador que esta variable existe, pero se definirá en otro archivo (main.cpp).
extern RingbufHandle_t g_audio_buffer;

#endif // SYSTEM_RESOURCES_H
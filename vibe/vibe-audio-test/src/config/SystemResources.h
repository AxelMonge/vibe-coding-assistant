#pragma once

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

// Handle global del ring de salida (WS ↓ → A2DP)
extern RingbufHandle_t g_audio_buffer;

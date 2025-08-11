extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/ringbuf.h"
}

// Definición (una sola vez en todo el proyecto)
RingbufHandle_t g_audio_buffer = nullptr;

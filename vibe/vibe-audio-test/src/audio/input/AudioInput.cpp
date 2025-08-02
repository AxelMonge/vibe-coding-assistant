#include "AudioInput.h"
#include <Arduino.h>
#include "../../config/HardwareConfig.h" // Usamos nuestro nuevo archivo de config

AudioInput::AudioInput() : audio_chunk_buffer(nullptr) {}

AudioInput::~AudioInput() {
    if (audio_chunk_buffer != nullptr) {
        delete[] audio_chunk_buffer;
    }
}

void AudioInput::init() {
    // Asignar memoria para el buffer de audio usando las constantes del config.
    audio_chunk_buffer = new int16_t[SAMPLES_PER_CHUNK];
    if (!audio_chunk_buffer) {
        Serial.println("FATAL: Fallo al asignar memoria para el buffer de audio. Reiniciando...");
        ESP.restart();
    }

    // Configurar el conversor Analógico-Digital (ADC)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
    Serial.println("Vibe Node: Módulo de entrada de audio (ADC) inicializado.");
}

void AudioInput::captureChunk() {
    if (!audio_chunk_buffer) return;

    // Calcula el intervalo de tiempo necesario entre cada muestra para lograr la frecuencia de muestreo deseada.
    long sample_interval_us = 1000000L / CAPTURE_SAMPLE_RATE;
    long start_time = micros();

    for (int i = 0; i < SAMPLES_PER_CHUNK; ++i) {
        // Espera activa hasta que sea el momento de tomar la siguiente muestra.
        while (micros() < start_time + (i * sample_interval_us)) {
            // yield() cede tiempo a otras tareas del sistema operativo para evitar bloqueos.
            yield(); 
        }
        // Lee el valor del pin del micrófono y lo almacena.
        // Se resta 2048 para centrar la señal de audio en 0 (eliminar el offset de DC).
        audio_chunk_buffer[i] = adc1_get_raw(ADC_CHANNEL) - 2048;
    }
}

const int16_t* AudioInput::getBuffer() const {
    return audio_chunk_buffer;
}
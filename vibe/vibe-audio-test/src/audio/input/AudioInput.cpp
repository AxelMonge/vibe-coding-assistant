#include "AudioInput.h"
#include <Arduino.h>
#include "../../config/HardwareConfig.h"

AudioInput::AudioInput() {}

AudioInput::~AudioInput() {}

void AudioInput::init() {
    // Configurar el conversor Analógico-Digital (ADC)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
    Serial.println("Vibe Node: Módulo de entrada de audio (ADC) inicializado.");
}

void AudioInput::captureChunk(int16_t* buffer) {
    if (!buffer) return;

    // Calcula el intervalo de tiempo necesario entre cada muestra.
    long sample_interval_us = 1000000L / SAMPLE_RATE;
    long start_time = micros();

    for (int i = 0; i < SAMPLES_PER_CHUNK; ++i) {
        // Espera activa hasta que sea el momento de tomar la siguiente muestra.
        while (micros() < start_time + (i * sample_interval_us)) {
            yield(); // Cede tiempo a otras tareas del sistema operativo.
        }
        // Lee el valor del pin del micrófono y lo centra en 0 para que sea con signo.
        buffer[i] = adc1_get_raw(ADC_CHANNEL) - 2048;
    }
}
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <driver/adc.h>

// --- PARÁMETROS DE AUDIO ---
const int SAMPLE_RATE = 16000;

// --- PARÁMETROS DE CAPTURA ANALÓGICA ---
const adc1_channel_t ADC_CHANNEL = ADC1_CHANNEL_6; // Pin IO34

// --- PARÁMETROS DE STREAMING (CONTRATO microwakeword) ---
// microwakeword requiere un tamaño de frame de 512 muestras.
const int SAMPLES_PER_CHUNK = 512;
const int CHUNK_BUFFER_BYTES = SAMPLES_PER_CHUNK * sizeof(int16_t); // 1024 bytes

#endif // HARDWARE_CONFIG_H
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <driver/adc.h>

// --- PARÁMETROS DE AUDIO PARA CAPTURA ---
const int CAPTURE_SAMPLE_RATE = 16000;
const adc1_channel_t ADC_CHANNEL = ADC1_CHANNEL_6; // Pin IO34

// --- PARÁMETROS DE STREAMING (CONTRATO WAKE-WORD) ---
// El motor Porcupine requiere un tamaño de frame de 512 muestras.
const int SAMPLES_PER_CHUNK = 512; 
// El tamaño del buffer en bytes será el número de muestras por el tamaño de cada muestra (2 bytes para int16_t)
const int CHUNK_BUFFER_BYTES = SAMPLES_PER_CHUNK * sizeof(int16_t);

#endif // HARDWARE_CONFIG_H
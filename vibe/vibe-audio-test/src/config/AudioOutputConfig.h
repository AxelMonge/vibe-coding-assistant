#ifndef AUDIO_OUTPUT_CONFIG_H
#define AUDIO_OUTPUT_CONFIG_H

// --- PARÁMETROS DE CONEXIÓN BLUETOOTH A2DP ---

// Nombre del dispositivo Bluetooth (altavoz) al que nos conectaremos.
// Debe coincidir EXACTAMENTE con el nombre que anuncia el altavoz XH-A158.
#define BT_SPEAKER_NAME "HCW Music"

// --- PARÁMETROS DEL BÚFER DE AUDIO ---

// Tamaño del búfer de anillo de FreeRTOS en bytes. Un búfer de 16KB (16 * 1024)
// proporciona un excelente equilibrio entre la absorción de jitter de la red
// y la latencia de audio. Puede ajustarse según las condiciones de la red.
#define A2DP_AUDIO_BUFFER_SIZE (16 * 1024)

// --- PARÁMETROS DE REPRODUCCIÓN ---

// Volumen por defecto para la salida A2DP (0-127).
// Se establece al iniciar la conexión para una experiencia de usuario consistente.
#define A2DP_DEFAULT_VOLUME 80

#endif // AUDIO_OUTPUT_CONFIG_H
#include <Arduino.h>
#include "micro_wake_word.h"

// --- Módulos del Sistema ---
#include "config/HardwareConfig.h"
#include "config/NetworkConfig.h"
#include "audio/input/AudioInput.h"
#include "network/wifi/WiFiManager.h"
#include "network/websocket/WebSocketClient.h"

// --- Modelos de Wake Word ---
// Usaremos el modelo pre-entrenado para "Okay, Nabu".
#include "okay_nabu_0.7_v1.h"

// --- Instancias de Módulos ---
WiFiManager wifiManager;
AudioInput audioInput;
WebSocketClient webSocketClient;

// --- Búfer de Audio ---
int16_t* audio_buffer = nullptr;

// --- Máquina de Estados ---
enum VibeState { STATE_IDLE, STATE_CONVERSING };
volatile VibeState currentState = STATE_IDLE;

// --- Tarea de Detección ---
void wakeWordTask(void *pvParameters);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Vibe Node: Modo Conversación con microWakeWord (MIC ANALÓGICO) v3.1 ---");

    // Asignar memoria para nuestro búfer de audio principal
    audio_buffer = (int16_t*)malloc(CHUNK_BUFFER_BYTES);
    if (!audio_buffer) {
        Serial.println("FATAL: Fallo al asignar memoria para el buffer de audio.");
        ESP.restart();
    }

    audioInput.init(); // Inicializa el micrófono ADC
    wifiManager.connect();

    // Crear y lanzar la tarea dedicada para la detección.
    xTaskCreatePinnedToCore(
        wakeWordTask, "WakeWordTask", 4096, NULL, 1, NULL, 1);

    Serial.println("Vibe Node: microWakeWord inicializado. Tarea de escucha activa. Di 'Okay, Nabu'...");
}

void loop() {
    delay(1000);
}

void wakeWordTask(void *pvParameters) {
    // Inicializar el detector de microWakeWord
    // Los parámetros del modelo "Okay, Nabu" están definidos en su archivo .h
    mww_init(audio_buffer, MWW_SAMPLE_RATE, MWW_CHUNK_SIZE_SAMPLES, MWW_PRE_SAMPLES, MWW_POST_SAMPLES);
    
    while (true) {
        // 1. Capturar un chunk de audio desde el ADC.
        audioInput.captureChunk(audio_buffer);

        if (currentState == STATE_IDLE) {
            // 2. En estado IDLE, procesar con el motor de wakeword.
            int detected = mww_detect(audio_buffer);
            if (detected) {
                Serial.println(">>> PALABRA DE ACTIVACIÓN DETECTADA <<<");
                Serial.println("Cambiando a modo CONVERSING y conectando WebSocket...");
                currentState = STATE_CONVERSING;
                webSocketClient.start();
            }
        } else if (currentState == STATE_CONVERSING) {
            // 3. En estado CONVERSING, enviar el audio al servidor.
            if (webSocketClient.isConnected()) {
                webSocketClient.sendAudioChunk(audio_buffer, CHUNK_BUFFER_BYTES);
            } else {
                Serial.println("Conexión perdida. Volviendo a modo IDLE.");
                currentState = STATE_IDLE;
            }
        }
    }
}
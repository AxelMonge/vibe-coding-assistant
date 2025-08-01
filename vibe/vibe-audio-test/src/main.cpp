#include <Arduino.h>

// --- Inclusión de Módulos ---
#include "config/NetworkConfig.h"

#include "config/SystemResources.h"
#include "audio/input/AudioInput.h"
#include "audio/output/AudioOutputA2DP.h"
#include "network/wifi/WiFiManager.h"
#include "network/websocket/WebSocketClient.h"

// --- Definición de la Variable Global ---
// Aquí se define y se asigna la memoria para el handle del búfer de audio.
RingbufHandle_t g_audio_buffer = nullptr;

// --- Módulos del Sistema ---
WiFiManager wifiManager;
AudioInput audioInput;
AudioOutputA2DP audioOutput; // Módulo de salida de audio
WebSocketClient webSocketClient;

/**
 * @brief Función de configuración principal.
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Vibe Node: Sistema de Audio Bidireccional ---");

    // 1. Conectar a la red WiFi.
    wifiManager.connect();

    // 2. Inicializar el hardware de entrada de audio (micrófono).
    audioInput.init();

    // 3. Inicializar la salida de audio Bluetooth A2DP.
    // IMPORTANTE: "XH-A158" es el nombre Bluetooth que intentará encontrar.
    // Si tu altavoz aparece con otro nombre en tu teléfono, debes cambiarlo aquí.
    audioOutput.begin("XH-A158");

    // 4. Iniciar el cliente WebSocket para conectar al servidor.
    webSocketClient.start();

    Serial.println("\n--- Sistema listo. Esperando conexión del servidor... ---");
}

/**
 * @brief Bucle principal.
 */
void loop() {
    // La mayor parte del trabajo ocurre en tareas de fondo (callbacks de A2DP y WebSocket).
    // El bucle principal puede usarse para monitorear el estado del sistema.

    static unsigned long last_check = 0;
    if (millis() - last_check > 5000) {
        if (webSocketClient.isConnected()) {
            Serial.printf("[STATUS] Conectado a WebSocket. Altavoz BT: %s.\n", audioOutput.isConnected() ? "CONECTADO" : "desconectado");
        } else {
            Serial.println("[STATUS] Desconectado de WebSocket. El cliente intentará reconectar...");
        }
        last_check = millis();
    }
    
    // Dejamos la lógica de envío de audio por si se quiere probar en el futuro,
    // pero no es necesaria para la prueba de recepción de audio con test_speaker.py.
    // if (webSocketClient.isConnected() && audioOutput.isConnected()) {
    //     audioInput.captureChunk();
    //     webSocketClient.sendAudioChunk(audioInput.getBuffer(), CHUNK_BUFFER_BYTES);
    // }

    // Pequeña pausa para no saturar la CPU con el bucle.
    delay(100);
}
#include <Arduino.h>

// --- Inclusión de Módulos ---
// Mantenemos todas las inclusiones de la nueva arquitectura.
#include "config/NetworkConfig.h"
#include "config/HardwareConfig.h"
#include "config/SystemResources.h" // Necesario para el RingBuffer global
#include "audio/input/AudioInput.h"
#include "audio/output/AudioOutputA2DP.h"
#include "network/wifi/WiFiManager.h"
#include "network/websocket/WebSocketClient.h"

// --- Definición de la Variable Global ---
// El RingBuffer se sigue creando, aunque no lo usemos activamente en este modo.
RingbufHandle_t g_audio_buffer = nullptr;

// --- Módulos del Sistema ---
WiFiManager wifiManager;
AudioInput audioInput;
AudioOutputA2DP audioOutput; // El objeto se crea, pero no lo usaremos activamente.
WebSocketClient webSocketClient;

/**
 * @brief Función de configuración principal.
 */
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Vibe Node: MODO DE CAPTURA DE AUDIO (Arquitectura Bidireccional Latente) ---");

    // 1. Conectar a la red WiFi (sin cambios).
    wifiManager.connect();

    // 2. Inicializar el hardware de entrada de audio (micrófono).
    //    Esta línea es CRÍTICA para que la captura de audio funcione.
    audioInput.init();

    // 3. Inicializar la salida de audio Bluetooth A2DP.
    //    COMENTAMOS ESTA LÍNEA TEMPORALMENTE.
    //    Al no llamar a begin(), el servicio Bluetooth no se iniciará,
    //    ahorrando memoria y ciclos de CPU, y evitando que se conecte al altavoz.
    // audioOutput.begin("XH-A158");

    // 4. Iniciar el cliente WebSocket para conectar al servidor (sin cambios).
    webSocketClient.start();

    Serial.println("\n--- Sistema listo en modo de solo captura. ---");
}

/**
 * @brief Bucle principal.
 *        RESTAURAMOS LA LÓGICA DEL FIRMWARE ORIGINAL.
 */
void loop() {
    // La lógica es idéntica a la de tu firmware original.
    // Comprueba si el WebSocket está conectado antes de intentar enviar datos.
    if (webSocketClient.isConnected()) {
        // 1. Capturar un chunk de audio desde el micrófono.
        audioInput.captureChunk();
        
        // 2. Enviar el chunk de audio crudo directamente al servidor.
        //    Usamos CHUNK_BUFFER_BYTES del HardwareConfig.h, que debe existir.
        webSocketClient.sendAudioChunk(audioInput.getBuffer(), CHUNK_BUFFER_BYTES);
    } else {
        // Si no hay conexión, esperar. El cliente nativo gestiona la reconexión.
        // Esto previene que el loop se ejecute a máxima velocidad si se pierde la conexión.
        delay(500);
        Serial.println("[STATUS] Desconectado de WebSocket. Esperando reconexión...");
    }
    
    // Es una buena práctica añadir un pequeño delay o yield() en el loop principal
    // para dar tiempo a las tareas de fondo del ESP32 (como WiFi y TCP/IP).
    // La función captureChunk() ya tiene un yield(), por lo que esto es redundante
    // si la conexión está activa, pero es una salvaguarda si está desconectado.
    delay(1); 
}
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

/**
 * WebSocketClient
 * Wrapper del cliente WebSocket del Vibe Node.
 *
 * Notas:
 * - Mantiene la API existente (`start()`, `loop()`, `isConnected()`, `sendAudioChunk(...)`).
 * - Añade `startSpeakerTest(host, port)` para el modo de prueba (B.9),
 *   que conecta al endpoint: /ws/speaker-test?file=audio_test.mp3&frame_ms=30&loop=1
 *   usando WS sin TLS (ws://).
 *
 * La implementación concreta (esp_websocket_client o WebSocketsClient) queda en el .cpp.
 */
class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    // Conexión al servidor principal (API existente en tu proyecto)
    void start();

    // B.9: Conexión al servidor de prueba de audio (ws://<host>:<port>/ws/speaker-test?...).
    void startSpeakerTest(const char* host, uint16_t port);

    // Detener/limpiar la conexión actual
    void stop();

    // Debe llamarse frecuentemente desde loop()
    void loop();

    // Estado de conexión
    bool isConnected() const;

    // Uplink (si se usa en otros modos): envía audio de mic al backend
    bool sendAudioChunk(const uint8_t* data, size_t length);

private:
    // No copiable
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    // Puntero opaco a la implementación concreta (definido en el .cpp)
    void* impl_ = nullptr;

    // Flag de conexión de alto nivel
    bool connected_ = false;
};

#include <Arduino.h>
#include "WebSocketClient.h"

#include "../../config/NetworkConfig.h"
#include "../../config/SystemResources.h"

// Definición de Opcodes de WebSocket (RFC 6455)
#define WEBSOCKET_OPCODE_TEXT   0x01
#define WEBSOCKET_OPCODE_BINARY 0x02

// Inicialización de la variable estática
volatile bool WebSocketClient::is_connected = false;

static void sendToAudioBuffer(const uint8_t* buffer, size_t length) {
    if (g_audio_buffer == nullptr) return;

    BaseType_t result = xRingbufferSend(g_audio_buffer, (void*)buffer, length, pdMS_TO_TICKS(10));
    if (result!= pdTRUE) {
        Serial.println("ADVERTENCIA: Búfer de audio lleno. Descartando paquete de audio.");
    }
}

WebSocketClient::WebSocketClient() : client(nullptr) {}

void WebSocketClient::eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            Serial.println("Vibe Node: Conectado al Vibe Server.");
            is_connected = true;
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            Serial.println("Vibe Node: Desconectado del Vibe Server.");
            is_connected = false;
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WEBSOCKET_OPCODE_BINARY) {
                sendToAudioBuffer((const uint8_t*)data->data_ptr, data->data_len);
            } else if (data->op_code == WEBSOCKET_OPCODE_TEXT) {
                Serial.printf(">>> SERVIDOR DICE (TEXTO): %.*s\n", data->data_len, (char*)data->data_ptr);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            Serial.println("Vibe Node: Error en WebSocket.");
            break;
    }
}

void WebSocketClient::start() {
    // --- CORRECCIÓN: Declarar ws_uri como un array de caracteres ---
    // Un tamaño de 100 es más que suficiente para la URI.
    char ws_uri[100];
    sprintf(ws_uri, "ws://%s:%d%s", SERVER_IP, SERVER_PORT, WEBSOCKET_PATH);
    
    const esp_websocket_client_config_t ws_cfg = {.uri = ws_uri };

    Serial.printf("Vibe Node: Inicializando WebSocket NATIVO hacia %s\n", ws_uri);
    client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, eventHandler, (void *)client);
    esp_websocket_client_start(client);
}

void WebSocketClient::sendAudioChunk(const int16_t* buffer, size_t length) {
    if (is_connected && client) {
        esp_websocket_client_send_bin(client, (const char *)buffer, length, portMAX_DELAY);
    }
}

bool WebSocketClient::isConnected() const {
    return is_connected;
}
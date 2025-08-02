#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include "esp_websocket_client.h"
#include <stdint.h>

class WebSocketClient {
public:
    WebSocketClient();
    void start();
    void sendAudioChunk(const int16_t* buffer, size_t length);
    bool isConnected() const;

private:
    static void eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    esp_websocket_client_handle_t client;
    static volatile bool is_connected; // Volatile para acceso seguro entre ISR y loop
};

#endif // WEBSOCKET_CLIENT_H
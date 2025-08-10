// ============================================================================
// File:    WiFiManager.h
// Author:  Vibe / Jarvis Audio Node
// Purpose: Declaración mínima del gestor de Wi-Fi para ESP32.
//          Implementación en WiFiManager.cpp
// ============================================================================

#pragma once

class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    // Conecta al Wi-Fi usando credenciales de NetworkConfig.h (bloqueante con timeout)
    void connect();

    // ¿Está asociado y con IP? (equivale a WiFi.status() == WL_CONNECTED)
    bool isConnected() const;
};

#include "WiFiManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include "../../config/NetworkConfig.h" // Ruta relativa a la nueva config

void WiFiManager::connect() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Vibe Node: Conectando a WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nVibe Node: WiFi Conectado.");
    Serial.print("Vibe Node: IP Local: ");
    Serial.println(WiFi.localIP());
}
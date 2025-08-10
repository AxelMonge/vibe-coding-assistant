// ============================================================================
// File:    src/network/wifi/WiFiManager.cpp
// Author:  Vibe / Jarvis Audio Node
// Purpose: Gestor de conexión Wi-Fi robusto para ESP32 con coexistencia BT.
//          - País "CR" -> habilita canales 1–13 (evita NO_AP_FOUND en 12/13)
//          - Solo 2.4 GHz (11b/g/n) -> el ESP32 no soporta 5 GHz
//          - Power Save MIN_MODEM -> mejor coexistencia con A2DP
//          - Escaneo y conexión dirigida (BSSID + canal) si se encuentra el SSID
// ============================================================================

#include "network/wifi/WiFiManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstring>   // strlen

extern "C" {
  #include <esp_wifi.h>        // esp_wifi_set_country, esp_wifi_set_protocol, esp_wifi_set_ps
  #include <esp_wifi_types.h>
}

#include "config/NetworkConfig.h"  // Define WIFI_SSID / WIFI_PASS centrales

// Tiempo máximo de espera para obtener WL_CONNECTED (ms)
static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;

// -----------------------------------------------------------------------------
// Ctor/Dtor
// -----------------------------------------------------------------------------
WiFiManager::WiFiManager() {}
WiFiManager::~WiFiManager() {}

// -----------------------------------------------------------------------------
// Conexión Wi-Fi robusta (país + 2.4GHz + escaneo + BSSID + PS MIN_MODEM)
// -----------------------------------------------------------------------------
void WiFiManager::connect() {
  // Reset limpio de estado Wi-Fi
  WiFi.persistent(false);
  WiFi.disconnect(true, true);     // borra credenciales temporales y desconecta
  delay(50);
  WiFi.mode(WIFI_STA);

  // País: Costa Rica (canales 1–13). Evita NO_AP_FOUND cuando el AP usa canal 12/13.
  wifi_country_t cr = { "CR", 1, 13, WIFI_COUNTRY_POLICY_AUTO };
  esp_wifi_set_country(&cr);

  // Fuerza 2.4 GHz (b/g/n). El ESP32 no soporta 5 GHz.
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  // Power save moderado para coexistencia con Bluetooth clásico (A2DP)
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  // Validación básica de credenciales
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("Vibe Node: WIFI_SSID vacío. Define credenciales en config/NetworkConfig.h o via -D.");
  }

  // Escaneo para localizar el AP exacto y, si se encuentra, conectar por BSSID/canal
  Serial.println("Vibe Node: Escaneando redes...");
  int n = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  int index = -1;
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == WIFI_SSID) { index = i; break; }
  }

  if (index < 0) {
    // No se vio el SSID durante el escaneo: intentamos por nombre (el core reintenta solo)
    Serial.printf("Vibe Node: SSID '%s' no encontrado en el escaneo. Conectando por nombre...\n", WIFI_SSID);
    Serial.print("Vibe Node: Conectando a WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  } else {
    // Conexión dirigida: más rápida y fiable (maneja SSID oculto y evita band-steering)
    const uint8_t* bssid = WiFi.BSSID((uint8_t)index); // API correcta: devuelve puntero
    const int channel    = WiFi.channel(index);

    if (bssid) {
      Serial.printf(
        "Vibe Node: Conectando a '%s' (canal %d, BSSID %02X:%02X:%02X:%02X:%02X:%02X)\n",
        WIFI_SSID, channel, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]
      );
      Serial.print("Vibe Node: Conectando a WiFi");
      // firma: begin(ssid, pass, channel, bssid, connect_now)
      WiFi.begin(WIFI_SSID, WIFI_PASS, channel, bssid, true);
    } else {
      // Si por alguna razón el puntero es nulo, degradamos a conexión por nombre
      Serial.printf("Vibe Node: BSSID nulo para '%s'. Conectando por nombre...\n", WIFI_SSID);
      Serial.print("Vibe Node: Conectando a WiFi");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  // Espera bloqueante con timeout (el auto-reconnect seguirá en background)
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    Serial.print(".");
    delay(150);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Vibe Node: WiFi Conectado.");
    Serial.printf("Vibe Node: IP Local: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Vibe Node: WiFi NO conectado (timeout). Continuará reintentando en background.");
  }

  // Limpieza de resultados de escaneo (buena práctica)
  WiFi.scanDelete();
}

// -----------------------------------------------------------------------------
// Estado actual
// -----------------------------------------------------------------------------
bool WiFiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

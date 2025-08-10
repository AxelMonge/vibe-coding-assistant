// ============================================================================
// File:    src/config/NetworkConfig.h
// Author:  Vibe / Jarvis Audio Node
// Purpose: Parámetros de red y audio a nivel de proyecto (credenciales Wi-Fi,
//          endpoint WebSocket y destino Bluetooth A2DP).
//
// Nota: Todos son macros para facilitar override desde platformio.ini con -D.
// ============================================================================

#pragma once

// --------------------------- Wi-Fi -------------------------------------------
// Define aquí o vía build_flags de PlatformIO (-DWIFI_SSID="..." -DWIFI_PASS="...")
#ifndef WIFI_SSID
  #define WIFI_SSID  "LIB-8369781"
#endif

#ifndef WIFI_PASS
  #define WIFI_PASS  "acB3zZ7eD"
#endif

// ------------------------ WebSocket Server -----------------------------------
// Dirección del servidor Python (test_speaker.py). Ajusta según tu red.
#ifndef WS_HOST
  #define WS_HOST "192.168.0.4"
#endif

#ifndef WS_PORT
  #define WS_PORT 81
#endif

#ifndef WS_PATH
  #define WS_PATH "/ws"
#endif

// Helpers para formar "ws://<host>:<port><path>"
#define __NC_STR_HELPER(x) #x
#define __NC_STR(x) __NC_STR_HELPER(x)

#ifndef WS_URL
  #define WS_URL "ws://" WS_HOST ":" __NC_STR(WS_PORT) WS_PATH
#endif

// ---------------------- Bluetooth A2DP (Source) ------------------------------
// Conecta por MAC (BD_ADDR) para evitar discovery (más estable).
// Usa la MAC real de tu altavoz; ejemplo visto en tus logs:
#ifndef BT_SPEAKER_NAME
  #define BT_SPEAKER_NAME "64:bb:a0:59:a1:fa"
#endif

// ---------------------------- Audio ------------------------------------------
#ifndef A2DP_AUDIO_BUFFER_SIZE
  #define A2DP_AUDIO_BUFFER_SIZE 16384
#endif

#ifndef A2DP_DEFAULT_VOLUME
  #define A2DP_DEFAULT_VOLUME 90
#endif

// --------------------- Helpers opcionales (declaraciones) --------------------
// No son obligatorios para el resto del código, pero útiles si los quieres usar
// como funciones en vez de macros. Están definidos en NetworkConfig.cpp.
extern const char* NC_WebSocketURL();      // devuelve WS_URL
extern const char* NC_BluetoothTarget();   // devuelve BT_SPEAKER_NAME

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

// Este archivo de cabecera actúa como un punto central para compartir
// definiciones y declaraciones entre diferentes partes de nuestra aplicación.

// --- 1. DECLARACIÓN DE LA MÁQUINA DE ESTADOS ---
// Definimos los posibles estados de nuestra aplicación.
// Al estar en un .h, cualquier .cpp que lo incluya conocerá este 'enum'.
enum SystemState {
  STATE_AWAITING_WIFI,
  STATE_AWAITING_WEBSOCKET,
  STATE_AWAITING_BLUETOOTH,
  STATE_SENDING_HANDSHAKE,
  STATE_STREAMING_FROM_SERVER,
  STATE_LISTENING_TO_MIC
};

// --- 2. DECLARACIÓN DE VARIABLES GLOBALES ---
// Usamos 'extern' para decirle al compilador: "Estas variables existen,
// están definidas en algún otro archivo .cpp. Confía en mí y deja que el
// linker las encuentre."
extern volatile SystemState currentState;
extern volatile bool g_server_stream_complete;
// Indica que acaba de comenzar el streaming desde el servidor y debemos "precargar" el búfer.
extern volatile bool g_stream_priming;

// --- 3. DECLARACIÓN DE FUNCIONES GLOBALES (PROTOTIPOS) ---
// Declaramos nuestras funciones de logging para que cualquier archivo
// que incluya este .h sepa de su existencia y cómo llamarlas.
void app_log_i(const char* format, ...);
void app_log_e(const char* format, ...);

#endif // SYSTEM_STATE_H
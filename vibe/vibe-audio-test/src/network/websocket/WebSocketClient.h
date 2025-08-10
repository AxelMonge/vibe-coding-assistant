// ============================================================================
// File:    WebSocketClient.h
// Author:  Vibe / Jarvis Audio Node
// Purpose: Pequeño wrapper OO sobre el cliente WebSocket del ESP-IDF
//          (esp_websocket_client) para:
//            - Gestionar conexión WS desde Arduino/ESP32.
//            - Implementar un protocolo de "créditos" (pull-based flow control)
//              para streaming binario de audio desde el servidor.
//            - Integrarse con un ring buffer global (g_audio_buffer) donde se
//              almacenan los chunks de audio entrantes que luego drena A2DP.
//
// Contexto del proyecto:
//   - El servidor envía un comando de texto "START_STREAM" cuando está listo.
//   - El ESP32 contesta con "CREDIT N" (N = # de chunks que puede recibir sin
//     desbordar el ring buffer).
//   - Por cada chunk binario recibido, se consume 1 crédito; cuando los
//     créditos pendientes bajan del umbral (low_watermark), el cliente envía
//     más créditos según el espacio libre del ring buffer.
//
// Notas importantes de compatibilidad:
//   - Este wrapper evita usar campos nuevos de `esp_websocket_client_config_t`
//     que no existen en algunos cores Arduino (p. ej. `reconnect_timeout_ms`,
//     `network_timeout_ms`). Solo usamos los miembros ampliamente disponibles
//     (`uri`, `disable_auto_reconnect`, `ping_interval_sec`,
//      `pingpong_timeout_sec`).
//
// Hilos y concurrencia:
//   - `eventHandler(...)` es llamado por la tarea del loop de eventos del
//     componente WebSocket (no por el loop de Arduino). No hacer trabajo
//     pesado ahí. Nos limitamos a encolar datos, actualizar flags y pedir
//     créditos.
//
// Integración con audio:
//   - El almacenamiento real de bytes de audio se hace en un ring buffer
//     FreeRTOS (g_audio_buffer), declarado extern en SystemResources.h y creado
//     en `setup()` de `main.cpp`. Este header NO lo incluye para no acoplar.
//
// Uso típico (resumen):
//   WebSocketClient ws;
//   ws.start();                      // inicia la conexión a WS_URL (definida en .cpp o NetworkConfig.h)
//   // En algún punto recibirás "START_STREAM" -> el cliente enviará "CREDIT N".
//   // Los binarios entrantes se encolarán en g_audio_buffer automáticamente.
//
// ============================================================================

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

// Arduino primero para evitar conflictos con INADDR_NONE / inet.h (orden crítico)
#include <Arduino.h>

extern "C" {
  #include "esp_event.h"
  #include "esp_websocket_client.h"
}

// -----------------------------------------------------------------------------
// Clase WebSocketClient
// -----------------------------------------------------------------------------
/**
 * @brief Wrapper minimalista y seguro de esp_websocket_client para streaming de audio con control por créditos.
 *
 * Diseño:
 *  - start(): inicializa y arranca el cliente WS con autoreconexión activada.
 *  - isConnected(): señala si la sesión está abierta.
 *  - sendText()/sendAudioChunk(): utilidades para enviar mensajes al servidor (entrada de mic, control, etc.).
 *
 *  - eventHandler(): procesa eventos CONNECTED/DISCONNECTED/DATA/ERROR.
 *    * DATA (op_code=1, texto) -> handleTextMessage()
 *      - Detecta "START_STREAM" y responde con "CREDIT N" (computeInitialCredits()).
 *    * DATA (op_code=2, binario) -> handleBinaryMessage()
 *      - Encola los bytes recibidos en el ring buffer g_audio_buffer.
 *      - Decrementa créditos pendientes y, si cae bajo low_watermark, envía más (maybeTopUpCredits()).
 *
 * Notas:
 *  - `chunk_bytes` define el tamaño esperado de cada chunk binario (coherente con el servidor).
 *  - `max_credits` limita cuántos chunks simultáneos se pueden pedir.
 *  - `low_watermark` dispara la solicitud de más créditos al caer el contador por debajo.
 */
class WebSocketClient {
public:
    // -------------------------------------------------------------------------
    // Construcción
    // -------------------------------------------------------------------------
    WebSocketClient();

    // -------------------------------------------------------------------------
    // Ciclo de vida
    // -------------------------------------------------------------------------
    /**
     * @brief Inicializa y arranca el cliente WebSocket.
     *
     * Configuración:
     *  - Usa WS_URL (definida en el .cpp o en NetworkConfig.h).
     *  - Activa autoreconexión (disable_auto_reconnect=false).
     *  - Ajusta PING/PONG con timeouts conservadores.
     */
    void start();

    /**
     * @brief ¿El WS está conectado?
     * @return true si el cliente está conectado y listo para enviar/recibir.
     */
    bool isConnected() const;

    // --- Getters para métricas de depuración ---
    inline uint8_t getOutstandingCredits() const { return outstanding_credits; }
    inline uint8_t getMaxCredits() const { return max_credits; }
    inline size_t  getChunkBytes() const { return chunk_bytes; }

    // -------------------------------------------------------------------------
    // Envío al servidor
    // -------------------------------------------------------------------------
    /**
     * @brief Envía texto por WebSocket.
     * @param text Cadena ASCII/UTF-8 terminada en '\0'.
     */
    void sendText(const char* text);

    /**
     * @brief Envía un buffer PCM o arbitrario como binario, troceándolo en
     *        bloques de tamaño `chunk_bytes`.
     *
     * @param buffer puntero a int16_t (u8 válido también si lo casteas).
     * @param length número de muestras (int16_t). Internamente se multiplica por 2 para bytes.
     */
    void sendAudioChunk(const int16_t* buffer, size_t length);

    // -------------------------------------------------------------------------
    // Parámetros de control de créditos (tuning)
    // -------------------------------------------------------------------------
    /**
     * @brief Fija el tamaño del chunk binario esperado (en bytes).
     *        Rango recomendado: [256, 4096]. Por defecto: 1024.
     */
    void setChunkBytes(size_t bytes);

    /**
     * @brief Fija el máximo de créditos simultáneos.
     *        Rango recomendado: [1, 64]. Por defecto: 12.
     */
    void setMaxCredits(uint8_t max);

    /**
     * @brief Umbral para reponer créditos. Si `outstanding_credits <= low_watermark`,
     *        se evalúa el espacio libre y se piden más.
     *        Debe ser < max_credits. Por defecto: 4.
     */
    void setLowWatermark(uint8_t low);

private:
    // -------------------------------------------------------------------------
    // Eventos / Interna
    // -------------------------------------------------------------------------
    /**
     * @brief Manejador estático de eventos de esp_websocket_client.
     *
     * @param handler_args puntero a `this`.
     * @param base         base del evento (no usado aquí).
     * @param event_id     id del evento (CONNECTED/DISCONNECTED/DATA/ERROR).
     * @param event_data   puntero a esp_websocket_event_data_t.
     */
    static void eventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

    /**
     * @brief Procesa un mensaje de texto (p. ej. "START_STREAM").
     * @param data puntero al buffer (NO null-terminated).
     * @param len  longitud en bytes.
     */
    void handleTextMessage(const char* data, size_t len);

    /**
     * @brief Procesa datos binarios (chunks de audio u otros).
     *        Encola en g_audio_buffer y gestiona créditos.
     *
     * @param data puntero a los bytes recibidos.
     * @param len  longitud en bytes.
     */
    void handleBinaryMessage(const uint8_t* data, size_t len);

    // -------------------------------------------------------------------------
    // Protocolo de Créditos
    // -------------------------------------------------------------------------
    /**
     * @brief Envía "CREDIT N" al servidor y aumenta `outstanding_credits`.
     * @param n número de créditos a pedir.
     */
    void sendCredits(uint8_t n);

    /**
     * @brief Calcula los créditos iniciales según el espacio libre en el ring buffer.
     * @return número de chunks a solicitar (capado por max_credits).
     */
    uint8_t computeInitialCredits();

    /**
     * @brief Repone créditos cuando `outstanding_credits` cae por debajo de `low_watermark`,
     *        teniendo en cuenta el espacio libre actual del ring buffer.
     */
    void maybeTopUpCredits();

private:
    // -------------------------------------------------------------------------
    // Estado del cliente
    // -------------------------------------------------------------------------
    esp_websocket_client_handle_t client = nullptr;

    // NOTA: `is_connected` es estática porque el event handler es estático.
    //       Mantenerla `volatile` ya que se toca desde otra tarea (event loop).
    static volatile bool is_connected;

    // -------------------------------------------------------------------------
    // Parámetros de control de flujo por créditos
    // -------------------------------------------------------------------------
    size_t   chunk_bytes      = 1024;  ///< Tamaño esperado por chunk binario (bytes).
    uint8_t  max_credits      = 12;    ///< Máximo de créditos simultáneos.
    uint8_t  low_watermark    = 4;     ///< Umbral para reponer créditos.
    volatile uint8_t outstanding_credits = 0; ///< Créditos emitidos aún no consumidos.

    // Sólo para logging de créditos, evita spam
    unsigned long last_credit_log_ms = 0;
};

#endif // WEBSOCKET_CLIENT_H

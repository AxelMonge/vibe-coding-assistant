# Resumen del Proyecto de Audio Bidireccional para IA (Memoria de objetivo)

Última actualización: 2025-08-08

## Objetivo actual

Estabilizar la comunicación de audio bidireccional en tiempo real entre un microcontrolador ESP32 y un servidor Python a través de WebSockets, con el propósito final de crear un asistente de voz tipo "Jarvis".

En la fase actual del test, nos centramos en perfeccionar el flujo de salida:
1. El servidor Python (`test_speaker.py`) envía un archivo de audio (la futura respuesta de Jarvis).
2. El ESP32 (`main.cpp`) recibe este audio.
3. El ESP32 lo reproduce de forma fluida y sin interrupciones en un amplificador Bluetooth externo.

En paralelo, el ESP32 debe ser capaz de capturar audio de un micrófono y enviarlo al servidor, aunque la gestión de este flujo de entrada es secundaria hasta que la reproducción sea estable.

## Idea original

Arquitectura basada en un flujo PUSH/PULL simultáneo y en tiempo real usando WebSockets:
- ESP32 -> Servidor (PUSH): El ESP32 capturaría continuamente audio del micrófono y lo empujaría al servidor.
- Servidor -> ESP32 (PULL controlada por el cliente): El servidor tendría una respuesta de audio lista. El ESP32, usando un sistema de créditos, tiraría de trozos de este audio solo cuando tuviera espacio en su búfer interno, para luego enviarlo al amplificador Bluetooth.

Se implementó una máquina de estados en el ESP32 para gestionar la secuencia de conexiones (WiFi, WebSocket, Bluetooth) y la lógica de comunicación (handshake y streaming).

## Retos enfrentados

1) Errores de compilación y lógica:
   - Colisiones de macros (`log_i`).
   - Variables fuera de alcance entre archivos `.cpp`.
   - Creación de `SystemState.h` para gestionar el estado global de la aplicación.

2) Inestabilidad del servidor Python:
   - La librería `websockets` en Windows presentaba inestabilidad (`OSError: semaphore timeout`) cuando el cliente se desconectaba abruptamente.
   - Solución: política de bucle de eventos específica para Windows.

3) Saturación de recursos del ESP32 (mayor reto):
   - Desbordamiento de búfer: el push de audio desde el servidor saturaba el búfer de 16 KB del ESP32, causando pérdida de datos y crashes (`assert failed: SbcAnalysisInit`).
   - Agotamiento de memoria de red: hacer PUSH (micrófono) y PULL (servidor) simultáneo agotaba recursos de la pila de red (errores `tcp_write error, errno=No more processes` y desconexiones).
   - Condición de carrera en el handshake: el mensaje "READY" quedaba detrás de paquetes de audio del micrófono; se solucionó con `vTaskDelay()` para forzar vaciado del búfer de red.

4) Inestabilidad de hardware (problema actual):
   - Conexión Bluetooth inestable: ciclo constante de intento de conexión (Estado 1) y desconexión inmediata (Estado 0), impidiendo un estado estable para iniciar streaming.
   - Hipótesis principal: fuente de alimentación insuficiente para operar WiFi y Bluetooth simultáneamente.

## Hardware físico: Vibe Node (componentes actuales)

- Microcontrolador: IDEABoard (basada en ESP32-WROOM-32).
- Micrófono: Módulo analógico con amplificador MAX9814.
- Pantalla: TFT de 2.4" con controlador táctil ILI9341 + XPT2046.
- Amplificador de audio: PAM8403.
- Alimentación: Batería LiPo de 3.7V con módulo de carga TP4056.

---

Notas rápidas de foco actual:
- Priorizar estabilidad del flujo de salida (A2DP) antes del flujo de entrada.
- La hipótesis de alimentación insuficiente es la principal a validar y resolver para estabilizar Bluetooth.

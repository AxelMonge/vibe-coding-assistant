import asyncio
import websockets
import sys
import numpy as np
import soundfile as sf
import time
from datetime import datetime

# --- CONFIGURACIÓN ---
SERVER_IP = "0.0.0.0"
SERVER_PORT = 81
TARGET_SAMPLE_RATE = 44100
TARGET_CHANNELS = 2
TARGET_DTYPE = 'int16'

# Protocolo de envío:
#  - Modo créditos: el ESP32 envía "CREDIT N" y el servidor responde con N
#    frames binarios de tamaño CHUNK_SIZE (último menor si quedan menos bytes).
#  - Modo solicitud directa: (legacy) el ESP32 pide tamaños concretos con
#    "REQUEST_DATA_<bytes>" y se responde con ese bloque.
# Activamos modo créditos por defecto porque el firmware actual usa CREDIT.
USE_CREDIT_PROTOCOL = True
CHUNK_SIZE = 1024  # Debe coincidir con chunk_bytes del cliente (firmware)

# --- FUNCIÓN DE LOGGING MEJORADA ---
def log_info(message, client_addr="SERVER"):
    timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
    print(f"[{timestamp}] [{client_addr}] {message}")

# --- LÓGICA DEL SERVIDOR ---
async def audio_handler(websocket, path):
    client_addr = f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
    log_info("Cliente conectado.", client_addr)
    
    audio_prepared = False
    raw_audio_data: bytes | None = None
    total_size = 0
    bytes_sent = 0  # offset global de envío

    try:
        # Bucle principal para manejar todos los mensajes de este cliente
        async for message in websocket:
            if isinstance(message, str):
                log_info(f"Recibido mensaje de texto: '{message}'", client_addr)

                if message == "READY":
                    if len(sys.argv) < 2:
                        log_info("ERROR: No se proporcionó un archivo de audio.", client_addr)
                        await websocket.close(1011, "Server error: No audio file")
                        return
                    
                    audio_file_path = sys.argv[1]
                    log_info(f"Señal 'READY' recibida. Cargando y convirtiendo: '{audio_file_path}'", client_addr)
                    
                    data, _ = sf.read(audio_file_path, dtype=TARGET_DTYPE, always_2d=True)
                    if data.shape[1] == 1:
                        data = np.concatenate([data, data], axis=1)
                    
                    raw_audio_data = data.tobytes()
                    total_size = len(raw_audio_data)
                    audio_prepared = True
                    log_info(f"Audio preparado. Tamaño total: {total_size / 1024:.2f} KB.", client_addr)

                    # --- LA SOLUCIÓN DEFINITIVA ---
                    # Inmediatamente después de preparar el audio, le damos la orden al ESP32
                    # para que cambie a su estado de recepción de streaming.
                    log_info("Enviando comando 'START_STREAM' al cliente...", client_addr)
                    await websocket.send("START_STREAM")
                    continue  # esperamos CREDIT o REQUEST_DATA_*

                # ---------------- PROTOCOLO CRÉDITOS -----------------
                if USE_CREDIT_PROTOCOL and message.startswith("CREDIT "):
                    if not audio_prepared:
                        log_info("Advertencia: CREDIT recibido antes de preparar audio. Ignorando.", client_addr)
                        continue
                    # Extraer número de créditos
                    try:
                        parts = message.split()
                        if len(parts) != 2:
                            raise ValueError
                        credits = int(parts[1])
                    except ValueError:
                        log_info(f"ERROR: Formato CREDIT inválido: '{message}'", client_addr)
                        continue

                    if credits <= 0:
                        log_info(f"Advertencia: CREDIT <=0 ({credits}). Ignorando.", client_addr)
                        continue

                    frames_sent = 0
                    bytes_before = bytes_sent
                    for _ in range(credits):
                        if bytes_sent >= total_size:
                            break
                        end = min(bytes_sent + CHUNK_SIZE, total_size)
                        chunk = raw_audio_data[bytes_sent:end]
                        if not chunk:
                            break
                        await websocket.send(chunk)
                        bytes_sent = end
                        frames_sent += 1

                        # ceder control al loop para no monopolizar (stream suave)
                        await asyncio.sleep(0)

                    if frames_sent:
                        pct = (bytes_sent / total_size) * 100 if total_size else 0
                        log_info(
                            f"CRÉDITOS consumidos={frames_sent}/{credits} | Enviado ahora={bytes_sent-bytes_before}B | Total={bytes_sent}/{total_size}B ({pct:.1f}%)",
                            client_addr
                        )

                    if bytes_sent >= total_size:
                        log_info("Transmisión completada (modo créditos). Enviando 'END_STREAM'.", client_addr)
                        await websocket.send("END_STREAM")
                        audio_prepared = False  # permitir un nuevo READY
                        bytes_sent = 0
                    continue

                # --------------- PROTOCOLO LEGACY REQUEST -------------
                elif message.startswith("REQUEST_DATA_"):
                    if not audio_prepared:
                        log_info("Advertencia: Se pidieron datos antes de que el audio estuviera listo. Ignorando.", client_addr)
                        continue

                    try:
                        bytes_to_send = int(message.split('_')[2])
                        chunk = raw_audio_data[bytes_sent : bytes_sent + bytes_to_send]
                        
                        if chunk:
                            # Ya no necesitamos loguear cada chunk para no saturar la terminal
                            # log_info(f"Enviando chunk de {len(chunk)} bytes...", client_addr)
                            await websocket.send(chunk)
                            bytes_sent += len(chunk)
                        else:
                            log_info("Transmisión completada. Enviando 'STREAM_END'.", client_addr)
                            await websocket.send("END_STREAM")
                            # Reiniciamos para poder recibir otro 'READY' si fuera necesario
                            audio_prepared = False
                            bytes_sent = 0

                    except (IndexError, ValueError):
                        log_info(f"ERROR: Petición de datos mal formada: '{message}'", client_addr)

            elif isinstance(message, bytes):
                # Esto es el audio del micrófono del ESP32
                log_info(f"Recibido chunk de audio binario de {len(message)} bytes (micrófono).", client_addr)
                # Aquí es donde en el futuro procesarías este audio con Gemini
                pass

    except websockets.exceptions.ConnectionClosed as e:
        log_info(f"Conexión cerrada. Razón: {e.reason} (Código: {e.code})", client_addr)
    except Exception as e:
        log_info(f"ERROR INESPERADO: {e}", client_addr)
    finally:
        log_info("Handler finalizado.", client_addr)

async def main():
    # Configurar parámetros de keepalive y tamaños para tolerar latencias / buffers
    log_info(f"Servidor Jarvis (v8 - Final Flow) escuchando en ws://{SERVER_IP}:{SERVER_PORT}")
    server = await websockets.serve(
        audio_handler,
        SERVER_IP,
        SERVER_PORT,
        ping_interval=20,
        ping_timeout=20,
        max_queue=64,
        write_limit=2**16,
        read_limit=2**16,
    )
    await server.wait_closed()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python test_speaker.py ruta/a/tu/audio.wav")
        sys.exit(1)
    try:
        # Política recomendada para Windows para evitar WinError 121 en desconexiones abruptas
        if sys.platform.startswith("win"):
            asyncio.set_event_loop_policy(asyncio.WindowsProactorEventLoopPolicy())
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 Servidor detenido por el usuario.")
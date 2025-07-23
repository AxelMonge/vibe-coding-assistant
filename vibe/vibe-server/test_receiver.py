import asyncio
import wave
import logging
from fastapi import FastAPI, WebSocket, WebSocketDisconnect

# --- Configuración del Servidor y Audio (DEBE COINCIDIR CON EL ESP32) ---
HOST = "0.0.0.0"  # Escucha en todas las interfaces de red
PORT = 8000
WEBSOCKET_PATH = "/ws_test"
OUTPUT_FILENAME = "test_output.wav"

# Parámetros del WAV
SAMPLE_RATE = 16000
NUM_CHANNELS = 1
SAMPLE_WIDTH_BYTES = 2  # uint16_t son 2 bytes

# Configuración del Logger para ver los eventos del servidor
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

app = FastAPI(title="Vibe Test Receiver")

@app.websocket(WEBSOCKET_PATH)
async def websocket_endpoint(websocket: WebSocket):
    """
    Endpoint de WebSocket que recibe datos de audio y los guarda en un archivo .wav.
    """
    await websocket.accept()
    logging.info(f"Cliente conectado desde {websocket.client.host}")
    try:
        while True:
            # Espera a recibir los datos binarios del ESP32
            audio_data = await websocket.receive_bytes()
            num_bytes = len(audio_data)
            
            if num_bytes == 0:
                logging.warning("Se recibieron 0 bytes. Ignorando.")
                continue

            logging.info(f"Recibidos {num_bytes} bytes de datos de audio.")
            
            # --- Ensamblaje del archivo WAV ---
            try:
                with wave.open(OUTPUT_FILENAME, 'wb') as wav_file:
                    wav_file.setnchannels(NUM_CHANNELS)
                    wav_file.setsampwidth(SAMPLE_WIDTH_BYTES)
                    wav_file.setframerate(SAMPLE_RATE)
                    # Escribe el payload de bytes directamente. ¡No se necesita procesar nada!
                    wav_file.writeframes(audio_data)
                
                num_samples = num_bytes // SAMPLE_WIDTH_BYTES
                duration = num_samples / SAMPLE_RATE
                logging.info(f"ÉXITO: Archivo '{OUTPUT_FILENAME}' guardado correctamente ({duration:.2f} segundos de audio).")

            except Exception as e:
                logging.error(f"Fallo crítico al crear el archivo WAV: {e}")

    except WebSocketDisconnect:
        logging.info("Cliente desconectado.")
    except Exception as e:
        logging.error(f"Ocurrió un error inesperado en el WebSocket: {e}")

if __name__ == "__main__":
    print("--- Servidor de Prueba de Audio Vibe ---")
    print(f"Escuchando en ws://{HOST}:{PORT}{WEBSOCKET_PATH}")
    print(f"Asegúrate de que la IP en el ESP32 es la correcta.")
    
    # Importar e iniciar uvicorn programáticamente
    import uvicorn
    uvicorn.run(app, host=HOST, port=PORT)
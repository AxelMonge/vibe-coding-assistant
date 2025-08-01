import asyncio
import websockets
import wave
import time

SERVER_IP = "0.0.0.0"
SERVER_PORT = 8000
WAV_FILE = "test_audio.wav"
CHUNK_SIZE = 960  # Coincide con CHUNK_BUFFER_BYTES del firmware original

async def audio_sender(websocket, path):
    print(f"Cliente conectado desde {websocket.remote_address}")
    try:
        with wave.open(WAV_FILE, 'rb') as wf:
            print(f"Enviando archivo de audio: {WAV_FILE}")
            print(f"  - Canales: {wf.getnchannels()}")
            print(f"  - Tasa de muestreo: {wf.getframerate()} Hz")
            print(f"  - Profundidad de bits: {wf.getsampwidth() * 8} bits")
            
            # Calcular el delay entre chunks para simular streaming en tiempo real
            bytes_per_second = wf.getframerate() * wf.getnchannels() * wf.getsampwidth()
            chunk_duration_sec = CHUNK_SIZE / bytes_per_second
            
            while True:
                data = wf.readframes(CHUNK_SIZE // wf.getparams().nchannels // wf.getparams().sampwidth)
                if not data:
                    print("Fin del archivo. Reiniciando en 3 segundos...")
                    time.sleep(3)
                    wf.rewind()
                    continue
                
                await websocket.send(data)
                await asyncio.sleep(chunk_duration_sec) # Esperar para simular tiempo real

    except websockets.exceptions.ConnectionClosed:
        print(f"Cliente desconectado: {websocket.remote_address}")
    except FileNotFoundError:
        print(f"ERROR: No se encontró el archivo '{WAV_FILE}'.")
    except Exception as e:
        print(f"Ocurrió un error: {e}")

async def main():
    print(f"Servidor WebSocket de audio iniciado en ws://{SERVER_IP}:{SERVER_PORT}")
    async with websockets.serve(audio_sender, SERVER_IP, SERVER_PORT):
        await asyncio.Future()  # Correr indefinidamente

if __name__ == "__main__":
    asyncio.run(main())
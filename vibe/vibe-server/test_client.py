import asyncio
import websockets

async def test_natural_language_command():
    uri = "ws://localhost:8000/ws"
    try:
        async with websockets.connect(uri) as websocket:
            print("INFO:    Inspector conectado al servidor.")

            # Leer archivo de audio
            audio_file_path = "test_comando.mp3"
            try:
                with open(audio_file_path, "rb") as f:
                    audio_data = f.read()
                print(f"INFO:    Archivo de audio '{audio_file_path}' leído correctamente, {len(audio_data)} bytes")
            except FileNotFoundError:
                print(f"ERROR:   Archivo '{audio_file_path}' no encontrado")
                return
            except Exception as e:
                print(f"ERROR:   Fallo al leer el archivo de audio: {e}")
                return

            await websocket.send(audio_data)
            print(f"INFO:    Enviando datos de audio desde '{audio_file_path}'")

            # Await server response
            response_json = await websocket.recv()
            print(f"INFO:    Respuesta JSON recibida <- {response_json}")

    except ConnectionRefusedError:
        print("ERROR:   La conexión fue rechazada. ¿Está el Vibe Server en funcionamiento?")
    except Exception as e:
        print(f"ERROR:   Error en el cliente: {e}")

if __name__ == "__main__":
    asyncio.run(test_natural_language_command())
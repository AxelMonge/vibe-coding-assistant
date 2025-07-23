import wave
import asyncio
from fastapi import FastAPI, WebSocket
from starlette.websockets import WebSocketDisconnect

app = FastAPI(title="Audio RX")

@app.websocket("/audio")
async def audio_ws(ws: WebSocket):
    await ws.accept()
    print("[WS] Cliente conectado")

    # Abrimos WAV de salida
    wav = wave.open("grabacion.wav", "wb")
    wav.setnchannels(1)        # mono
    wav.setsampwidth(2)        # 16-bit
    wav.setframerate(16000)    # 16kHz

    try:
        while True:
            data = await ws.receive_bytes()  # bloque binario PCM16
            if not data:
                break
            wav.writeframes(data)
    except WebSocketDisconnect:
        print("[WS] Cliente desconectado")
    finally:
        wav.close()
        print("[WAV] grabacion.wav guardado")

    # (Opcional) reproducir localmente, convertir, etc.
    # from pydub import AudioSegment
    # audio = AudioSegment.from_wav("grabacion.wav")
    # audio.export("grabacion.mp3", format="mp3", bitrate="128k")
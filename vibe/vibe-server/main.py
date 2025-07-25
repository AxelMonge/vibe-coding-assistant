import logging
import time  # Para logs de timing
from dotenv import load_dotenv
load_dotenv()

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import asyncio
from enum import Enum, auto

from services.vad_service import VoiceActivityProcessor
from services.stt_service import initialize_stt_service, transcribe_audio
from services.gemini_service import process_user_intent  # Importamos la nueva función unificada

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)-20s - %(levelname)-8s - %(message)s')
logger = logging.getLogger(__name__)

app = FastAPI(title="Vibe Server", version="4.1.0")  # Versión de Inferencia de un Solo Paso

@app.on_event("startup")
def startup_event():
    initialize_stt_service()

NORMALIZATION_RULES = {
    "punto pay": ".py", "punto pie": ".py", "docker file": "dockerfile",
    "doctor file": "dockerfile", "java script": "javascript", "type script": "typescript",
    "lista los archivos": "ls", "instala": "npm install", "corre": "npm run"
}

def normalize_transcript(text: str) -> str:
    start_time = time.perf_counter()  # Inicio timing
    normalized_text = text.lower()
    for error, correction in NORMALIZATION_RULES.items():
        normalized_text = normalized_text.replace(error, correction)
    end_time = time.perf_counter()  # Fin timing
    logger.info(f"Proceso Normalización tomó {(end_time - start_time) * 1000:.2f} ms")
    return normalized_text

class VibeState(Enum):
    AWAITING_AUDIO = auto()
    LISTENING = auto()
    PROCESSING_INTENT = auto()  # Un único estado de procesamiento

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_id = f"{websocket.client.host}:{websocket.client.port}"
    logger.info(f"Cliente conectado: {client_id}.")
    
    vad_processor = VoiceActivityProcessor()
    current_state = VibeState.AWAITING_AUDIO
    
    try:
        while True:
            if current_state == VibeState.AWAITING_AUDIO:
                audio_chunk = await websocket.receive_bytes()
                vad_processor.process_chunk(audio_chunk)
                current_state = VibeState.LISTENING

            elif current_state == VibeState.LISTENING:
                audio_chunk = await websocket.receive_bytes()
                vad_start = time.perf_counter()  # Inicio timing VAD
                if vad_processor.process_chunk(audio_chunk):
                    vad_end = time.perf_counter()  # Fin timing VAD
                    logger.info(f"Proceso VAD tomó {(vad_end - vad_start) * 1000:.2f} ms")
                    stt_start = time.perf_counter()  # Inicio timing STT
                    raw_transcript = await transcribe_audio(vad_processor.get_audio_buffer())
                    stt_end = time.perf_counter()  # Fin timing STT
                    logger.info(f"Proceso STT tomó {(stt_end - stt_start) * 1000:.2f} ms")
                    if raw_transcript:
                        websocket.state.raw_transcript = raw_transcript
                        current_state = VibeState.PROCESSING_INTENT
                    else:
                        vad_processor.reset()
                        current_state = VibeState.AWAITING_AUDIO
            
            elif current_state == VibeState.PROCESSING_INTENT:
                raw_transcript = websocket.state.raw_transcript
                normalized_transcript = normalize_transcript(raw_transcript)
                logger.info(f"[{client_id}] Crudo: '{raw_transcript}' -> Normalizado: '{normalized_transcript}'")
                
                intent_start = time.perf_counter()  # Inicio timing Intent
                intent_data = await process_user_intent(normalized_transcript)
                intent_end = time.perf_counter()  # Fin timing Intent
                logger.info(f"Proceso Intent (Gemini) tomó {(intent_end - intent_start) * 1000:.2f} ms")

                if intent_data and "command" in intent_data and "response" in intent_data:
                    command = intent_data["command"]
                    response = intent_data["response"]

                    logger.info(f"[{client_id}] >>> RESPUESTA GENERADA: '{response}' <<<")
                    await websocket.send_text(f"Vibe dice: {response}")

                    if command:
                        logger.info(f"[{client_id}] >>> EJECUTANDO: {command} <<< (Simulado)")
                        # Aquí irá el enrutamiento al Connection Manager
                    
                else:
                    logger.error(f"[{client_id}] La respuesta de Gemini fue inválida.")
                    await websocket.send_text("Error: Hubo un problema de comunicación con la IA.")

                vad_processor.reset()
                current_state = VibeState.AWAITING_AUDIO
                logger.info("Ciclo conversacional completado.")

    except WebSocketDisconnect:
        logger.info(f"Cliente {client_id} desconectado.")
    except Exception as e:
        logger.error(f"Error crítico en FSM: {e}", exc_info=True)
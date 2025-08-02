import logging
import time
from dotenv import load_dotenv
load_dotenv()

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import asyncio
from enum import Enum, auto

# Asegúrate de que has actualizado vad_service.py con la versión 2.0 que te proporcioné.
from services.vad_service import VoiceActivityProcessor
from services.stt_service import initialize_stt_service, transcribe_audio
from services.gemini_service import process_user_intent

# --- Configuración del Logging ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)-20s - %(levelname)-8s - %(message)s')
logger = logging.getLogger(__name__)

# --- Inicialización de la App FastAPI ---
app = FastAPI(title="Vibe Server", version="5.0.0") # Versión con Búfer de Re-ensamblaje

@app.on_event("startup")
def startup_event():
    """Inicializa los servicios necesarios cuando el servidor arranca."""
    initialize_stt_service()

# --- Reglas de Normalización de Texto ---
NORMALIZATION_RULES = {
    "punto pay": ".py", "punto pie": ".py", "docker file": "dockerfile",
    "doctor file": "dockerfile", "java script": "javascript", "type script": "typescript",
    "lista los archivos": "ls", "instala": "npm install", "corre": "npm run"
}

def normalize_transcript(text: str) -> str:
    """Aplica un conjunto de reglas para corregir errores comunes de transcripción."""
    start_time = time.perf_counter()
    normalized_text = text.lower()
    for error, correction in NORMALIZATION_RULES.items():
        normalized_text = normalized_text.replace(error, correction)
    end_time = time.perf_counter()
    logger.info(f"Proceso Normalización tomó {(end_time - start_time) * 1000:.2f} ms")
    return normalized_text

# --- Máquina de Estados Finitos (FSM) Simplificada ---
# La nueva lógica del VAD nos permite simplificar la FSM.
class VibeState(Enum):
    LISTENING = auto()          # Estado principal: recibiendo audio y esperando fin de elocución.
    PROCESSING = auto()         # Estado de transición: STT y Gemini están trabajando.

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    Endpoint principal de WebSocket que maneja el ciclo de conversación completo.
    """
    await websocket.accept()
    client_id = f"{websocket.client.host}:{websocket.client.port}"
    logger.info(f"Cliente conectado: {client_id}. Iniciando ciclo de escucha.")
    
    vad_processor = VoiceActivityProcessor()
    current_state = VibeState.LISTENING
    
    try:
        while True:
            # El bucle ahora es más simple. Siempre esperamos datos.
            audio_chunk = await websocket.receive_bytes()

            if current_state == VibeState.LISTENING:
                # 1. Añadir el chunk de audio (tamaño arbitrario) al búfer interno del VAD.
                vad_processor.add_chunk(audio_chunk)
                
                # 2. Pedir al VAD que procese su búfer. Devolverá True solo al final del habla.
                if vad_processor.process_stream():
                    logger.info("VAD detectó fin de elocución. Cambiando a estado PROCESSING.")
                    current_state = VibeState.PROCESSING
                    
                    # --- Inicia el procesamiento asíncrono ---
                    # Obtenemos el buffer de audio completo de la elocución.
                    full_audio_buffer = vad_processor.get_audio_buffer()
                    
                    # Transcribir el audio.
                    stt_start = time.perf_counter()
                    raw_transcript = await transcribe_audio(full_audio_buffer)
                    stt_end = time.perf_counter()
                    logger.info(f"Proceso STT tomó {(stt_end - stt_start) * 1000:.2f} ms")

                    if not raw_transcript:
                        logger.warning("STT no devolvió transcripción. Volviendo a escuchar.")
                        vad_processor.reset()
                        current_state = VibeState.LISTENING
                        continue # Salta al siguiente ciclo del bucle

                    # Normalizar y procesar con Gemini.
                    normalized_transcript = normalize_transcript(raw_transcript)
                    logger.info(f"[{client_id}] Crudo: '{raw_transcript}' -> Normalizado: '{normalized_transcript}'")
                    
                    intent_start = time.perf_counter()
                    intent_data = await process_user_intent(normalized_transcript)
                    intent_end = time.perf_counter()
                    logger.info(f"Proceso Intent (Gemini) tomó {(intent_end - intent_start) * 1000:.2f} ms")

                    # Enviar respuesta al cliente.
                    if intent_data and "response" in intent_data:
                        response = intent_data["response"]
                        logger.info(f"[{client_id}] >>> RESPUESTA GENERADA: '{response}' <<<")
                        await websocket.send_text(f"Vibe dice: {response}")
                        # Aquí se podría añadir la lógica para enviar el audio TTS.
                    else:
                        logger.error(f"[{client_id}] La respuesta de Gemini fue inválida.")
                        await websocket.send_text("Error: Hubo un problema de comunicación con la IA.")

                    # --- Ciclo completado ---
                    logger.info("Ciclo conversacional completado. Volviendo a estado LISTENING.")
                    vad_processor.reset()
                    current_state = VibeState.LISTENING

    except WebSocketDisconnect:
        logger.info(f"Cliente {client_id} desconectado.")
    except Exception as e:
        logger.error(f"Error crítico en el WebSocket: {e}", exc_info=True)
        # En caso de un error inesperado, nos aseguramos de que el cliente se desconecte.
        await websocket.close(code=1011)
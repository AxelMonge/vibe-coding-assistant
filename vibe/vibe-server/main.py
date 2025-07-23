import asyncio
import logging
from enum import Enum, auto

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from services.vad_service import VoiceActivityProcessor
# Los siguientes servicios se usarán en los próximos pasos, los importamos ahora.
# from services.stt_service import transcribe_audio
# from services.gemini_service import translate_command_to_json

# --- Configuración del Logger ---
# Se configura un logger raíz para capturar logs de todos los módulos.
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)-15s - %(levelname)-8s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# --- Definición de la Máquina de Estados Finitos (FSM) ---
class VibeState(Enum):
    """Define los estados operativos discretos del asistente por conexión."""
    AWAITING_AUDIO = auto()
    LISTENING_AND_PROCESSING_VAD = auto()
    TRANSCRIBING = auto()
    PROCESSING_LLM = auto()
    EXECUTING_COMMAND = auto()
    RESPONDING_TTS = auto()

# --- Inicialización de la Aplicación FastAPI ---
app = FastAPI(
    title="Vibe Server",
    description="El cerebro orquestador de Vibe, basado en una Máquina de Estados Finitos.",
    version="2.0.0"
)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    Gestiona el ciclo de vida completo de una conexión de cliente Vibe.
    Cada cliente conectado opera su propia instancia de la FSM.
    """
    await websocket.accept()
    client_id = f"{websocket.client.host}:{websocket.client.port}"
    logger.info(f"Cliente conectado: {client_id}. Creando instancia de FSM.")
    
    # Cada conexión tiene su propio procesador de voz y estado.
    vad_processor = VoiceActivityProcessor()
    current_state = VibeState.AWAITING_AUDIO
    
    try:
        while True:
            # --- Motor de la Máquina de Estados Finitos ---

            if current_state == VibeState.AWAITING_AUDIO:
                # En este estado, el servidor está pasivo, esperando el primer
                # fragmento de audio para iniciar el proceso de escucha activa.
                audio_chunk = await websocket.receive_bytes()
                vad_processor.process_chunk(audio_chunk)
                current_state = VibeState.LISTENING_AND_PROCESSING_VAD
                logger.info(f"[{client_id}] Transición: AWAITING_AUDIO -> LISTENING_AND_PROCESSING_VAD")

            elif current_state == VibeState.LISTENING_AND_PROCESSING_VAD:
                # El servidor ahora escucha activamente, pasando cada fragmento
                # de audio al servicio VAD para determinar el fin del habla.
                audio_chunk = await websocket.receive_bytes()
                user_finished_speaking = vad_processor.process_chunk(audio_chunk)
                
                if user_finished_speaking:
                    current_state = VibeState.TRANSCRIBING
                    logger.info(f"[{client_id}] Transición: LISTENING_AND_PROCESSING_VAD -> TRANSCRIBING")
            
            elif current_state == VibeState.TRANSCRIBING:
                # El VAD ha determinado el fin del habla. Ahora se procesa el audio acumulado.
                full_audio = vad_processor.get_audio_buffer()
                duration = len(full_audio) / (16000 * 2) if len(full_audio) > 0 else 0
                logger.info(f"[{client_id}] Audio completo recibido ({len(full_audio)} bytes, ~{duration:.2f}s).")
                
                # --- PUNTO DE PRUEBA FINAL DE ESTA FASE ---
                # En esta fase, solo confirmamos que el flujo VAD->FSM funciona.
                # En el siguiente paso, aquí se llamará al servicio STT.
                logger.info(f"[{client_id}] >>> TEST DE FSM PASADO: Flujo VAD completado. <<<")
                await websocket.send_text("Vibe ha detectado el final de tu voz. Listo para transcribir.")
                
                # Reseteamos el estado para la próxima interacción.
                vad_processor.reset()
                current_state = VibeState.AWAITING_AUDIO
                logger.info(f"[{client_id}] FSM reseteada. Transición -> AWAITING_AUDIO")

    except WebSocketDisconnect:
        logger.info(f"Cliente {client_id} desconectado.")
    except Exception as e:
        logger.error(f"[{client_id}] Error crítico en la FSM del WebSocket: {e}", exc_info=True)
    finally:
        # Asegurarse de que los recursos se limpien si es necesario.
        logger.info(f"Cerrando la sesión para el cliente {client_id}.")
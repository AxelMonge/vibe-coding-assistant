import os
import logging
import asyncio
# --- ACTUALIZACIÓN CIENTÍFICA ---
# Importamos la versión v1p1beta1 de la librería, que nos da acceso a speechContexts.
from google.cloud import speech_v1p1beta1 as speech
from google.api_core.exceptions import GoogleAPICallError
from google.oauth2 import service_account

logger = logging.getLogger(__name__)

# --- Variables de Estado del Módulo ---
speech_client: speech.SpeechClient | None = None
PROJECT_ID: str | None = None

# --- Vocabulario de Dominio Específico ---
# Esta lista se convertirá en el "cerebro" de nuestro STT.
# La poblamos con todos los términos técnicos que Vibe debe entender.
DOMAIN_VOCABULARY = [
    # Programación General
    "python", "javascript", "react", "vue", "angular", "git", "docker", "kubernetes",
    "async", "await", "useState", "useEffect", "API", "JSON", "HTML", "CSS", "SQL",
    "NoSQL", "función", "variable", "clase", "método", "objeto", "array", "diccionario",
    "puntero", "terminal", "consola", "servidor", "commit", "push", "pull", "branch",
    "merge", "rebase", "VS Code", "Visual Studio", "FastAPI", "websocket",
    
    # --- NUEVOS TÉRMINOS DE IA ---
    "Gemini", "ChatGPT", "Grok", "agente de IA", "agentes de IA", "LLM", "modelo de lenguaje", "Siri", "Alexa", "Langchain", "n8n",

    # Electrónica
    "ESP32", "GPIO", "PWM", "ADC", "I2C", "SPI", "resistor", "capacitor", "ohm",
    "farad", "voltaje", "corriente", "Arduino", "PlatformIO", "sketch", "circuito",
    
    # Fitness (basado en tu pregunta sobre "hipertrofia")
    "hipertrofia", "VO2 max", "variabilidad de la frecuencia cardíaca", "MET",
    "calorías", "proteína", "carbohidratos", "lípidos", "entrenamiento de fuerza",
    
    # Fonética y Comandos
    "punto py", "punto js", "punto ts", "punto html", "punto css"
]

def initialize_stt_service():
    """Inicializa el cliente STT v1p1beta1."""
    global speech_client, PROJECT_ID
    try:
        credentials_path = os.getenv("GOOGLE_APPLICATION_CREDENTIALS")
        if not credentials_path or not os.path.exists(credentials_path):
            logger.fatal("La variable de entorno GOOGLE_APPLICATION_CREDENTIALS no es válida.")
            return
        credentials = service_account.Credentials.from_service_account_file(credentials_path)
        PROJECT_ID = credentials.project_id
        logger.info(f"Inicializando SpeechClient (v1p1beta1) para el proyecto: {PROJECT_ID}")
        speech_client = speech.SpeechClient(credentials=credentials)
    except Exception as e:
        logger.fatal(f"Fallo crítico al inicializar SpeechClient: {e}", exc_info=True)

async def transcribe_audio(audio_bytes: bytes) -> str | None:
    """
    Transcribe audio usando el modelo command_and_search y adaptación con speechContexts.
    """
    if not speech_client or not PROJECT_ID:
        logger.error("El servicio STT no está inicializado. Abortando.")
        return None
    if not audio_bytes:
        return None

    # --- LA IMPLEMENTACIÓN CIENTÍFICA ---
    # 1. Crear el contexto de habla con nuestro vocabulario y un alto boost.
    #    Esto le dice al modelo que estas frases son extremadamente probables.
    speech_contexts = [speech.SpeechContext(
        phrases=DOMAIN_VOCABULARY,
        boost=15.0  # Un valor de boost alto para priorizar nuestro vocabulario.
    )]

    # 2. Configurar el reconocimiento.
    config = speech.RecognitionConfig(
        encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
        sample_rate_hertz=16000,
        language_code="es-ES",
        # Usamos el modelo optimizado para comandos, como sugiere la investigación.
        model="command_and_search",
        # Habilitamos el modelo mejorado para mayor precisión.
        use_enhanced=True,
        # Adjuntamos nuestro contexto de habla.
        speech_contexts=speech_contexts,
    )

    audio = speech.RecognitionAudio(content=audio_bytes)

    try:
        logger.info(f"Enviando solicitud de transcripción con speechContexts (modelo command_and_search)...")
        # La llamada a recognize en v1p1beta1 también es síncrona.
        response = await asyncio.to_thread(speech_client.recognize, config=config, audio=audio)
    except GoogleAPICallError as e:
        logger.error(f"Error en la llamada a la API de Google Speech: {e}", exc_info=True)
        return None

    if not response or not response.results:
        logger.warning("La respuesta de la API de STT no contiene resultados.")
        return None

    # La estructura de la respuesta es ligeramente diferente en v1.
    most_likely_transcript = response.results[0].alternatives[0].transcript
    logger.info(f"Texto transcrito (con adaptación v1) recibido: '{most_likely_transcript}'")
    
    return most_likely_transcript
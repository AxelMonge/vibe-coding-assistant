"""
Servicio STT: Transcripción con Google Cloud Speech-to-Text V2.

Actualizaciones (2025): Ubicación 'global'; AdaptationPhraseSet en lugar de PhraseSetReference; Manejo de operaciones long-running; Recuperación de recursos existentes; Adaptación aplicada directamente en config para streaming; Corregido interim_results en streaming_features [ref: cloud.google.com/speech-to-text/v2/docs/reference/rpc/google.cloud.speech.v2#streamingrecognitionconfig, GitHub generative-ai notebooks].
"""
import os
import queue
from google.cloud.speech_v2 import SpeechClient
from google.cloud.speech_v2.types import cloud_speech as speech  # Alias para types.
from google.api_core.exceptions import AlreadyExists, NotFound
from dotenv import load_dotenv

load_dotenv()

client = None

PROJECT_ID = os.getenv('GOOGLE_PROJECT_ID', 'tactile-oxygen-465119-b1')
LOCATION = 'global'  # Requerido para V2; regiones como 'us-central1' causan error 400.
PHRASE_SET_ID = 'vibe-phrase-set'
LANGUAGE_CODE = 'es-ES'
MODEL_NAME = 'latest_short'
SAMPLE_RATE = 16000
CHANNELS = 1

PHRASES = [
    {"value": "crea un archivo", "boost": 20.0},
    {"value": "nuevo archivo", "boost": 20.0},
    {"value": "escribe en", "boost": 20.0},
    {"value": "agrega código", "boost": 20.0},
    {"value": "reemplaza bloque", "boost": 20.0},
    {"value": "borra archivo", "boost": 20.0},
    {"value": "elimina", "boost": 20.0},
    {"value": "abre terminal", "boost": 20.0},
    {"value": "ejecuta comando", "boost": 20.0},
    {"value": "refactoriza", "boost": 20.0},
    {"value": "debug", "boost": 20.0},
    {"value": "prueba unitaria", "boost": 20.0},
    {"value": "commit", "boost": 20.0},
    {"value": "push", "boost": 20.0},
    {"value": "pull", "boost": 20.0}
]

def initialize_service():
    """
    Inicializa STT V2: Crea o recupera PhraseSet (con boosts por frase). No crea recognizer custom; usa default '_' con adaptación en config.
    """
    global client
    try:
        client = SpeechClient()

        parent = f"projects/{PROJECT_ID}/locations/{LOCATION}"
        phrase_set_path = f"{parent}/phraseSets/{PHRASE_SET_ID}"

        # Crear o recuperar PhraseSet.
        try:
            phrase_set_obj = speech.PhraseSet(
                display_name="Vibe Commands Phrase Set",
                phrases=[speech.PhraseSet.Phrase(**phrase) for phrase in PHRASES]  # Boosts por frase.
            )
            create_phrase_set_request = speech.CreatePhraseSetRequest(
                parent=parent,
                phrase_set_id=PHRASE_SET_ID,
                phrase_set=phrase_set_obj
            )
            operation = client.create_phrase_set(request=create_phrase_set_request)
            operation.result()  # Espera operación long-running.
            print("INFO: Nuevo PhraseSet creado.")
        except AlreadyExists:
            # Recupera existente para confirmar path.
            phrase_set_obj = client.get_phrase_set(name=phrase_set_path)
            print("INFO: PhraseSet ya existe; recuperado.")
        except NotFound:
            print("WARNING: PhraseSet no encontrado después de intento de creación.")
            return False, None

        # No crea recognizer; usa default con adaptación en config por request.
        print("INFO: STT V2 inicializado (usando recognizer default con adaptación).")
        return True, phrase_set_path  # Retorna True y path para usar en transcripción.

    except Exception as e:
        print(f"FATAL: Error inicialización STT: {e}")
        return False, None

def transcribe_streaming(audio_queue: queue.Queue) -> str | None:
    """
    Transcribe raw PCM en streaming con V2, aplicando adaptación directamente.
    """
    if not client:
        print("ERROR: STT no inicializado.")
        return None

    success, phrase_set_path = initialize_service()  # Asegura inicialización si no hecha.
    if not success:
        return None

    try:
        recognizer_path = f"projects/{PROJECT_ID}/locations/{LOCATION}/recognizers/_"  # Default recognizer.

        explicit_decoding = speech.ExplicitDecodingConfig(
            encoding=speech.ExplicitDecodingConfig.AudioEncoding.LINEAR16,
            sample_rate_hertz=SAMPLE_RATE,
            audio_channel_count=CHANNELS
        )

        adaptation = speech.SpeechAdaptation(
            phrase_sets=[
                speech.SpeechAdaptation.AdaptationPhraseSet(phrase_set=phrase_set_path)
            ]
        )

        config = speech.RecognitionConfig(
            explicit_decoding_config=explicit_decoding,
            language_codes=[LANGUAGE_CODE],
            model=MODEL_NAME,
            adaptation=adaptation,
            features=speech.RecognitionFeatures(enable_automatic_punctuation=True)
        )
        
        streaming_features = speech.StreamingRecognitionFeatures(interim_results=True)
        
        streaming_config = speech.StreamingRecognitionConfig(
            config=config,
            streaming_features=streaming_features
        )

        def request_generator():
            yield speech.StreamingRecognizeRequest(
                recognizer=recognizer_path,
                streaming_config=streaming_config
            )
            while True:
                chunk = audio_queue.get()
                if chunk is None:
                    return
                yield speech.StreamingRecognizeRequest(audio=chunk)

        responses = client.streaming_recognize(requests=request_generator())

        transcript = ""
        for response in responses:
            if not response.results:
                continue
            result = response.results[0]
            if not result.alternatives:
                continue
            transcript = result.alternatives[0].transcript
            if result.is_final:
                print(f"Transcript final: {transcript}")
                return transcript

        print("WARNING: Sin texto reconocido.")
        return None

    except Exception as e:
        print(f"ERROR: Fallo transcripción: {e}")
        return None

# Inicializa automáticamente al importar.
initialize_service()
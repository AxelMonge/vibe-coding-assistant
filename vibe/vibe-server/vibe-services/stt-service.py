# vibe-server/services/stt_service.py
# Este módulo se encarga de la transcripción de audio a texto.
# Su única responsabilidad (SRP) es interactuar con la API de Google Speech-to-Text.
from google.cloud import speech_v2 as speech

def transcribe_audio_bytes(audio_data: bytes) -> str | None:
    """
    Toma datos de audio binarios, los envía a la API de Google STT y devuelve el texto transcrito.
    Utiliza adaptación de modelo para mejorar la precisión en terminología de programación.
    """
    try:
        # Crea un cliente para la API.
        client = speech.SpeechClient()

        # Nuestro vocabulario personalizado para ayudar al modelo.
        phrase_set = speech.PhraseSet(
            phrases=[
                speech.Phrase(value="Vibe"),
                speech.Phrase(value="Vibe Architect"),
                speech.Phrase(value="FastAPI"),
                speech.Phrase(value="refactorizar"),
                speech.Phrase(value="constante"),
                speech.Phrase(value="asíncrono", boost_value=15), # Le damos más peso
                speech.Phrase(value="await", boost_value=15)
            ]
        )
        
        # Adaptación para aplicar nuestro set de frases.
        adaptation = speech.SpeechAdaptation(phrase_sets=[phrase_set])

        # Configuración del reconocimiento.
        config = speech.RecognitionConfig(
            language_codes=["es-ES"],
            model="long", # Modelo para comandos y dictado.
            adaptation=adaptation,
            # No es necesario especificar encoding o sample rate si el formato es común (WAV).
            # La API puede autodetectarlo.
        )
        
        # Preparamos la petición.
        request = speech.RecognizeRequest(
            recognizer=f"projects/{client.project_name}/locations/global/recognizers/_",
            config=config,
            content=audio_data,
        )

        # Enviamos la petición y obtenemos la respuesta.
        response = client.recognize(request=request)

        if response.results:
            return response.results.alternatives.transcript
        else:
            print("INFO:     El audio no contenía texto reconocible.")
            return ""
            
    except Exception as e:
        print(f"ERROR:    Fallo en la transcripción con Google STT: {e}")
        return None
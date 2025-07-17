from google.cloud import speech_v2 as speech
import os
from dotenv import load_dotenv

# Cargar variables de entorno desde .env
load_dotenv()

# Obtener PROJECT_ID desde las variables de entorno
PROJECT_ID = os.environ.get("PROJECT_ID")

def transcribe_audio_bytes(audio_data: bytes) -> str | None:
    try:
        client = speech.SpeechClient()
        config = speech.RecognitionConfig(
            auto_decoding_config=speech.AutoDetectDecodingConfig(),
            language_codes=["es-ES"],
            model="latest_long",
        )
        request = speech.RecognizeRequest(
            recognizer=f"projects/{PROJECT_ID}/locations/global/recognizers/_",
            config=config,
            content=audio_data,
        )
        response = client.recognize(request=request)
        if response.results:
            for result in response.results:
                for alt in result.alternatives:
                    return alt.transcript
        else:
            print("No se reconoció texto.")
            return ""
    except Exception as e:
        print(f"Error: {e}")
        return None

if __name__ == "__main__":
    audio_file_path = "vibe/vibe-server/services/test_comando.mp3"
    with open(audio_file_path, "rb") as f:
        audio_data = f.read()
    print("Transcripción:", transcribe_audio_bytes(audio_data))
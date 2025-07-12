from google.cloud import speech_v2 as speech

# ⚡ Pon tu Project ID aquí:
PROJECT_ID = "tactile-oxygen-465119-b1"

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
    audio_file_path = "vibe/vibe-server/vibe-services/test_command.wav"

    with open(audio_file_path, "rb") as f:
        audio_data = f.read()

    print("Transcripción:", transcribe_audio_bytes(audio_data))

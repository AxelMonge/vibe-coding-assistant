import os
import logging
from dotenv import load_dotenv
from google.cloud.speech_v2 import SpeechClient
from google.cloud.speech_v2.types import cloud_speech
from google.api_core.exceptions import AlreadyExists
from google.oauth2 import service_account

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)-20s - %(levelname)-8s - %(message)s')
logger = logging.getLogger("GCP_Setup")

load_dotenv()

CUSTOM_CLASS_ID = "vibe-programming-class"
PHRASE_SET_ID = "vibe-simple-phrases" # Renombrado para mayor claridad

def setup_resources():
    try:
        credentials_path = os.getenv("GOOGLE_APPLICATION_CREDENTIALS")
        if not credentials_path or not os.path.exists(credentials_path):
            logger.fatal("La variable de entorno GOOGLE_APPLICATION_CREDENTIALS no es válida.")
            return

        credentials = service_account.Credentials.from_service_account_file(credentials_path)
        project_id = credentials.project_id
        client = SpeechClient(credentials=credentials)
        parent = f"projects/{project_id}/locations/global"

    except Exception as e:
        logger.fatal(f"Fallo al inicializar el cliente de GCP: {e}", exc_info=True)
        return

    # 1. Crear la CustomClass (esta parte era correcta)
    try:
        logger.info(f"Intentando crear/verificar CustomClass '{CUSTOM_CLASS_ID}'...")
        custom_class_request = cloud_speech.CreateCustomClassRequest(
            parent=parent,
            custom_class_id=CUSTOM_CLASS_ID,
            custom_class=cloud_speech.CustomClass(
                display_name="Vibe Programming Concepts",
                items=[cloud_speech.CustomClass.ClassItem(value=term) for term in [
                    "python", "javascript", "react", "vue", "angular", "git", "docker",
                    "kubernetes", "async", "await", "useState", "useEffect", "cout",
                    "cin", "API", "JSON", "HTML", "CSS", "SQL", "NoSQL", "función",
                    "variable", "clase", "método", "objeto", "array", "diccionario",
                    "puntero", "terminal", "consola", "servidor"
                ]]
            )
        )
        client.create_custom_class(request=custom_class_request)
        logger.info(f"CustomClass '{CUSTOM_CLASS_ID}' creada con éxito.")
    except AlreadyExists:
        logger.info(f"CustomClass '{CUSTOM_CLASS_ID}' ya existe.")
    except Exception as e:
        logger.fatal(f"Fallo al crear la CustomClass: {e}", exc_info=True)
        return

    # 2. Crear el PhraseSet con frases literales y simples.
    try:
        logger.info(f"Intentando crear/verificar PhraseSet '{PHRASE_SET_ID}'...")
        phrase_set_request = cloud_speech.CreatePhraseSetRequest(
            parent=parent,
            phrase_set_id=PHRASE_SET_ID,
            phrase_set=cloud_speech.PhraseSet(
                display_name="Vibe Simple Command Phrases",
                phrases=[
                    # --- LA CORRECCIÓN CIENTÍFICA ---
                    # Frases literales, sin referencias a clases.
                    cloud_speech.PhraseSet.Phrase(value="punto py", boost=20),
                    cloud_speech.PhraseSet.Phrase(value="punto js", boost=20),
                    cloud_speech.PhraseSet.Phrase(value="crea un archivo", boost=15),
                    cloud_speech.PhraseSet.Phrase(value="borra el archivo", boost=15),
                    cloud_speech.PhraseSet.Phrase(value="escribe en el archivo", boost=15),
                    cloud_speech.PhraseSet.Phrase(value="ejecuta en la terminal", boost=15),
                ]
            )
        )
        client.create_phrase_set(request=phrase_set_request)
        logger.info(f"PhraseSet '{PHRASE_SET_ID}' creado con éxito.")
    except AlreadyExists:
        logger.info(f"PhraseSet '{PHRASE_SET_ID}' ya existe.")
    except Exception as e:
        logger.fatal(f"Fallo al crear el PhraseSet: {e}", exc_info=True)
        return
        
    logger.info("\n>>> Configuración de recursos de GCP completada con éxito. <<<")

if __name__ == "__main__":
    setup_resources()
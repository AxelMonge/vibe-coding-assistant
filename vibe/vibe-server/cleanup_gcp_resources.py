import os
import logging
from dotenv import load_dotenv
from google.cloud.speech_v2 import SpeechClient
from google.api_core.exceptions import NotFound
from google.oauth2 import service_account

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)-20s - %(levelname)-8s - %(message)s')
logger = logging.getLogger("GCP_Cleanup")

load_dotenv()

CUSTOM_CLASS_ID = "vibe-programming-class"
PHRASE_SET_ID = "vibe-class-based-phrases"

def cleanup_resources():
    """
    Elimina los recursos de adaptación (CustomClass y PhraseSet) de Google Cloud
    de forma idempotente (si no existen, no falla).
    """
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

    # 1. Eliminar el PhraseSet. Es dependiente, por lo que se elimina primero.
    try:
        phrase_set_path = client.phrase_set_path(project_id, "global", PHRASE_SET_ID)
        logger.info(f"Intentando eliminar PhraseSet '{phrase_set_path}'...")
        client.delete_phrase_set(name=phrase_set_path)
        logger.info(f"PhraseSet '{PHRASE_SET_ID}' eliminado con éxito.")
    except NotFound:
        logger.info(f"PhraseSet '{PHRASE_SET_ID}' no encontrado. No se requiere ninguna acción.")
    except Exception as e:
        logger.error(f"Fallo al eliminar el PhraseSet: {e}", exc_info=True)

    # 2. Eliminar la CustomClass.
    try:
        custom_class_path = client.custom_class_path(project_id, "global", CUSTOM_CLASS_ID)
        logger.info(f"Intentando eliminar CustomClass '{custom_class_path}'...")
        client.delete_custom_class(name=custom_class_path)
        logger.info(f"CustomClass '{CUSTOM_CLASS_ID}' eliminada con éxito.")
    except NotFound:
        logger.info(f"CustomClass '{CUSTOM_CLASS_ID}' no encontrada. No se requiere ninguna acción.")
    except Exception as e:
        logger.error(f"Fallo al eliminar la CustomClass: {e}", exc_info=True)
        
    logger.info("\n>>> Limpieza de recursos de GCP completada. <<<")

if __name__ == "__main__":
    cleanup_resources()
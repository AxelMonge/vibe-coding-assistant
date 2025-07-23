"""
Módulo de Configuración Central para Vibe Server.

Este archivo actúa como una única fuente de verdad (Single Source of Truth) para
todas las configuraciones críticas del proyecto. Centralizar la configuración
aquí facilita la gestión, reduce la duplicación de código y mejora la seguridad
al mantener todas las rutas y claves sensibles en un solo lugar.
"""
import os

# --- Configuración de GCP (Google Cloud Platform) ---

# Ruta al archivo de credenciales de la cuenta de servicio.
# Este archivo es nuestra "llave" para acceder a los servicios de Google Cloud.
# Debe estar en .gitignore para no ser subido al repositorio.
GCP_CREDENTIALS_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config', 'gcp_credentials.json')

# El ID de nuestro proyecto en Google Cloud.
PROJECT_ID = "tactile-oxygen-465119-b1" 

# La ubicación geográfica de los recursos de la API. 'global' es la estándar.
GCP_LOCATION = "global"

# --- Configuración del Servicio de Transcripción (STT) ---

# ID único para nuestro recurso "Recognizer" en Google Cloud.
# Crear un recognizer persistente mejora el rendimiento.
STT_RECOGNIZER_ID = "vibe-recognizer-es-v1"

# ID único para nuestro recurso "PhraseSet".
# Le damos un nombre para poder actualizarlo en el futuro.
STT_PHRASESET_ID = "vibe-technical-jargon-v1"
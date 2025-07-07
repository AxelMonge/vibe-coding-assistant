"""
Gemini Service: Handles interaction with Google Gemini API for command translation.

This module is responsible for converting natural language commands into structured JSON
format for controlling a code editor, adhering to the Single Responsibility Principle (SRP).

Dependencies:
- os: For accessing environment variables
- json: For parsing JSON responses
- google.generativeai: For interacting with the Gemini API
- dotenv: For loading environment variables from .env file

Global Variables:
- PROMPT_INSTRUCTIONS: String containing instructions for the Gemini model
- model: Configured Gemini model instance
"""

import os
import json
import google.generativeai as genai
from dotenv import load_dotenv

# Load environment variables (API key) from .env file
load_dotenv()

def configure_ai():
    """
    Configures and returns the Gemini AI model.

    Behavior:
        1. Configures the Gemini API with the provided API key
        2. Initializes the 'gemini-1.5-pro-latest' model
        3. Returns the model or None if configuration fails

    Exceptions:
        KeyError: Raised if GOOGLE_API_KEY is not found in environment variables
    """
    try:
        genai.configure(api_key=os.environ["GOOGLE_API_KEY"])
        model = genai.GenerativeModel('gemini-1.5-pro')
        return model
    except KeyError:
        print("ERROR: GOOGLE_API_KEY no encontrada. Asegúrate de que tu archivo .env está configurado.")
        return None

# Instructions for Gemini to convert natural language to structured JSON
PROMPT_INSTRUCTIONS = """
Eres Vibe-Translator, un asistente experto que convierte comandos de lenguaje natural a un formato JSON estricto para controlar un editor de código. Tu única respuesta DEBE ser un objeto JSON válido y nada más. No añadas explicaciones ni texto adicional.

El formato JSON debe tener dos claves: "action" y "params".
Las acciones posibles son: "create_file", "delete_file", "write_to_file", "replace_code_block".
Los "params" dependen de la acción.

Aquí tienes ejemplos:

Usuario: "crea un archivo llamado app.js"
Tu respuesta:
{
  "action": "create_file",
  "params": {
    "filename": "app.js",
    "content": ""
  }
}

Usuario: "haz una nueva hoja de estilos que se llame styles.css"
Tu respuesta:
{
  "action": "create_file",
  "params": {
    "filename": "styles.css",
    "content": ""
  }
}

Usuario: "en el archivo config.py, escribe port = 5000"
Tu respuesta:
{
  "action": "write_to_file",
  "params": {
    "filename": "config.py",
    "content": "port = 5000"
  }
}
"""

# Configure the model once when the module is loaded
model = configure_ai()

async def translate_command_to_json(user_command: str) -> dict | None:
    """
    Sends user command to Gemini and returns the JSON response as a Python dictionary.

    Args:
        user_command (str): The natural language command from the user

    Behavior:
        1. Combines prompt instructions with the user command
        2. Sends the prompt to the Gemini model
        3. Parses the response into a Python dictionary
        4. Returns None if any error occurs during processing

    Returns:
        dict: Parsed JSON response from Gemini, or None if an error occurs

    Exceptions:
        Exception: Caught for any errors during Gemini interaction or JSON parsing
    """
    if not model:
        return None

    # Combine instructions with the user command
    full_prompt = f"{PROMPT_INSTRUCTIONS}\nUsuario: \"{user_command}\"\nTu respuesta:"

    try:
        response = await model.generate_content_async(full_prompt)
        json_response_text = response.text.strip()

        # Clean up JSON if wrapped in markdown code blocks
        if json_response_text.startswith("```json"):
            json_response_text = json_response_text[7:-3].strip()

        # Parse JSON text to Python dictionary
        return json.loads(json_response_text)
    except Exception as e:
        print(f"ERROR: Fallo al interactuar con Gemini o parsear su respuesta: {e}")
        print(f"Respuesta cruda recibida: {response.text if 'response' in locals() else 'N/A'}")
        return None
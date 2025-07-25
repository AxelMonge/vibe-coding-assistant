import os
import json
import logging
import time  # Para logs de timing
import google.generativeai as genai

logger = logging.getLogger(__name__)

try:
    api_key = os.getenv("GOOGLE_API_KEY")
    if not api_key:
        logger.warning("La variable de entorno GOOGLE_API_KEY no está definida.")
        genai_model = None
    else:
        genai.configure(api_key=api_key)
        # Listar modelos disponibles para depuración (ejecuta una vez y loggea)
        try:
            models = [m.name for m in genai.list_models()]
            logger.info(f"Modelos disponibles: {models}")
        except Exception as e:
            logger.warning(f"No se pudo listar modelos: {e}")
        
        # Usa el nombre correcto: 'gemini-2.5-flash' (estable, soportado en v1/v1beta)
        genai_model = genai.GenerativeModel(
            'gemini-2.5-flash',  # Corregido: sin '-latest'
            generation_config=genai.GenerationConfig(
                temperature=0.2,  # Baja para precisión en JSON/clasificación
                top_p=0.95,       # Controla diversidad sin exceso
                max_output_tokens=512  # Límite para respuestas concisas
            )
        )
except Exception as e:
    logger.error(f"Fallo crítico al configurar Gemini: {e}", exc_info=True)
    genai_model = None

UNIFIED_PROMPT = """
Eres Vibe, un asistente IA como Jarvis: profesional, conciso, seguro y engaging. Analiza el input del usuario para clasificar la intención, extraer comandos si aplica, y generar una respuesta hablada corta/directa basada en contexto. No uses markdown ni explicaciones extras.
Salida ÚNICA: JSON {"module": str (filesystem/terminal/chitchat), "command": str|null, "response": str (respuesta verbal concisa)}.

REGLAS:
- Clasifica: filesystem (manipulación archivos), terminal (comandos shell/código), chitchat (conversación general/programación/electrónica/fitness).
- Extrae command: Exacto y ejecutable (e.g., "git commit" o "create_file main.py"); null para chitchat.
- Genera response: Basada en simulado "Éxito" para comandos o "Conversación iniciada" para chitchat; sé verbose si es chitchat pero <50 palabras.
- Prioriza contexto: Input usuario, módulo, acción, resultado. Maneja términos técnicos (e.g., ESP32, git, VO2 max) con precisión.

EJEMPLOS:
Usuario: "crea un archivo main.py"
{"module": "filesystem", "command": "create_file main.py", "response": "Hecho. Archivo main.py creado exitosamente."}

Usuario: "instala express con npm"
{"module": "terminal", "command": "npm install express", "response": "Éxito al ejecutar: npm install express."}

Usuario: "explica git rebase"
{"module": "chitchat", "command": null, "response": "Git rebase integra cambios de una rama a otra reescribiendo historia; úsalo para commits limpios, pero con cuidado en compartidos."}

Usuario: "qué es PWM en ESP32"
{"module": "chitchat", "command": null, "response": "PWM (Pulse Width Modulation) en ESP32 controla señales analógicas simuladas via duty cycle para LEDs/motores; usa ledc en PlatformIO."}

Usuario: "calcula VO2 max para running"
{"module": "chitchat", "command": null, "response": "VO2 max estima capacidad aeróbica; fórmula aproximada: (distancia en km / tiempo en min) * factor. Consulta app fitness para precisión."}

Usuario: "hola, cuéntame un chiste"
{"module": "chitchat", "command": null, "response": "¡Hola! ¿Por qué los programadores odian la naturaleza? Porque tiene demasiados bugs. ¿Listo para codificar?"}
"""

async def process_user_intent(user_command: str) -> dict | None:
    """
    Procesa la intención con un solo prompt unificado.
    Retorna: {"command": str or None, "response": str} (extrae module internamente si necesitas).
    """
    if not genai_model: return None
    full_prompt = f"{UNIFIED_PROMPT}\nUsuario: \"{user_command}\"\n"
    try:
        start_time = time.perf_counter()  # Inicio timing
        logger.info(f"Enviando a Gemini (Prompt Unificado): '{user_command}'")
        response = await genai_model.generate_content_async(full_prompt, stream=True)
        response_text = ""
        async for chunk in response:
            response_text += chunk.text
        response_text = response_text.strip().replace("```json", "").replace("```", "").strip()
        logger.info(f"Respuesta JSON Unificada de Gemini: {response_text}")
        data = json.loads(response_text)
        end_time = time.perf_counter()  # Fin timing
        logger.info(f"Proceso Gemini tomó {(end_time - start_time) * 1000:.2f} ms")
        # Retorna subset compatible con main.py
        return {"command": data.get("command"), "response": data.get("response")}
    except Exception as e:
        logger.error(f"Error en Prompt Unificado de Gemini: {e}", exc_info=True)
        return None
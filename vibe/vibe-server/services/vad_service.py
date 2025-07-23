# vibe-server/services/vad_service.py

import webrtcvad
from collections import deque
import logging

# Configuración del Logger
logger = logging.getLogger("VADService")

class VoiceActivityProcessor:
    """
    Procesa un stream de audio para detectar actividad de voz (VAD) y
    determina cuándo un usuario ha terminado de hablar.
    """
    def __init__(self, sample_rate=16000, vad_aggressiveness=3, silence_duration_ms=1500):
        """
        Inicializa el procesador VAD.

        Args:
            sample_rate (int): Frecuencia de muestreo (8000, 16000, 32000, o 48000).
            vad_aggressiveness (int): Nivel de agresividad del VAD (0-3). 3 es el más agresivo.
            silence_duration_ms (int): Milisegundos de silencio para considerar que el usuario terminó.
        """
        self.sample_rate = sample_rate
        self.vad = webrtcvad.Vad(vad_aggressiveness)
        
        # webrtcvad solo funciona con chunks de 10, 20, o 30 ms.
        # Usaremos 30ms, que es el recomendado para mejor precisión.
        self.chunk_duration_ms = 30
        self.chunk_samples = int(sample_rate * self.chunk_duration_ms / 1000)
        self.chunk_bytes = self.chunk_samples * 2  # 2 bytes por muestra (16-bit)

        # Calculamos cuántos chunks de silencio se necesitan para activar el fin de habla.
        self.silence_chunks_needed = silence_duration_ms // self.chunk_duration_ms
        
        # Usamos un deque (cola de doble extremo) para mantener un historial de los últimos N chunks.
        self.speech_history = deque(maxlen=self.silence_chunks_needed)
        
        self.audio_buffer = bytearray()
        self.is_speaking = False

    def process_chunk(self, audio_chunk: bytes):
        """
        Procesa un chunk de audio y actualiza el estado de voz.

        Returns:
            bool: True si el usuario ha terminado de hablar, False en caso contrario.
        """
        if len(audio_chunk) != self.chunk_bytes:
            logger.warning(f"Chunk de audio de tamaño incorrecto. Se esperaban {self.chunk_bytes} bytes, se recibieron {len(audio_chunk)}.")
            return False

        try:
            is_speech = self.vad.is_speech(audio_chunk, self.sample_rate)
            self.speech_history.append(is_speech)

            if is_speech:
                self.is_speaking = True  # Marcamos que el usuario ha empezado a hablar en algún momento.
                self.audio_buffer.extend(audio_chunk)
            elif self.is_speaking:
                # Si ya había hablado, también guardamos los chunks de silencio intermedios.
                self.audio_buffer.extend(audio_chunk)

            # Verificamos si el usuario ha terminado de hablar.
            # Condición: Debe haber hablado en algún momento (is_speaking == True) Y
            # el buffer de historial debe estar lleno (hemos procesado suficientes chunks) Y
            # todos los chunks en el historial deben ser de silencio.
            if self.is_speaking and len(self.speech_history) == self.silence_chunks_needed and not any(self.speech_history):
                logger.info("FIN DE LA VOZ DETECTADO.")
                return True # ¡El usuario ha terminado!
                
        except Exception as e:
            logger.error(f"Error en el procesamiento VAD: {e}")

        return False # El usuario sigue hablando o está en silencio.

    def get_audio_buffer(self) -> bytes:
        """Devuelve el buffer de audio acumulado."""
        return bytes(self.audio_buffer)

    def reset(self):
        """Resetea el estado para la próxima interacción."""
        self.speech_history.clear()
        self.audio_buffer.clear()
        self.is_speaking = False
        logger.info("Procesador VAD reseteado.")
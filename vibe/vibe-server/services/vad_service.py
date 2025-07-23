import webrtcvad
from collections import deque
import logging

# Configuración del Logger para este servicio específico.
logger = logging.getLogger(__name__)

class VoiceActivityProcessor:
    """
    Procesa un stream de audio para detectar actividad de voz (VAD) y
    determina cuándo un usuario ha terminado de hablar, encapsulando la lógica
    del algoritmo VAD de WebRTC.
    """
    def __init__(self, sample_rate: int = 16000, vad_aggressiveness: int = 3, silence_duration_ms: int = 1500):
        """
        Inicializa el procesador VAD con parámetros validados.

        Args:
            sample_rate (int): Frecuencia de muestreo. Debe ser 8000, 16000, 32000, o 48000.
            vad_aggressiveness (int): Nivel de agresividad del VAD (0-3). 3 es el más agresivo
                                     y menos propenso a clasificar ruido como voz.
            silence_duration_ms (int): Milisegundos de silencio para considerar el fin del habla.
        """
        if sample_rate not in [8000, 16000, 32000, 48000]:
            raise ValueError("Frecuencia de muestreo no válida para VAD. Usar 8000, 16000, 32000, o 48000.")
        
        self.sample_rate = sample_rate
        self.vad = webrtcvad.Vad(vad_aggressiveness)
        
        # webrtcvad solo funciona con chunks de 10, 20, o 30 ms.
        # 30ms es el recomendado para la mejor precisión.
        self.chunk_duration_ms = 30
        self.chunk_samples = int(sample_rate * self.chunk_duration_ms / 1000)
        self.chunk_bytes = self.chunk_samples * 2  # 2 bytes por muestra (PCM 16-bit)

        # Calcula cuántos chunks de silencio consecutivos se necesitan para activar el fin de habla.
        self.silence_chunks_needed = silence_duration_ms // self.chunk_duration_ms
        
        # Usamos un deque como una ventana deslizante para mantener un historial de los últimos N chunks.
        self.speech_history = deque(maxlen=self.silence_chunks_needed)
        
        self.audio_buffer = bytearray()
        self.is_speaking = False

    def process_chunk(self, audio_chunk: bytes) -> bool:
        """
        Procesa un chunk de audio, actualiza el estado de voz y determina si el usuario ha terminado.

        Args:
            audio_chunk (bytes): Un bloque de audio que debe tener el tamaño exacto de `self.chunk_bytes`.

        Returns:
            bool: True si se ha detectado el final del habla, False en caso contrario.
        """
        if len(audio_chunk) != self.chunk_bytes:
            logger.warning(f"Chunk de audio de tamaño incorrecto. Se esperaban {self.chunk_bytes} bytes, se recibieron {len(audio_chunk)}.")
            return False

        try:
            is_speech = self.vad.is_speech(audio_chunk, self.sample_rate)
        except Exception as e:
            logger.error(f"Error en la validación del chunk por webrtcvad: {e}")
            return False

        self.speech_history.append(is_speech)

        # Si el chunk contiene voz, o si el usuario ya había empezado a hablar,
        # acumulamos el chunk en nuestro buffer de audio.
        if is_speech or self.is_speaking:
            self.audio_buffer.extend(audio_chunk)

        if is_speech:
            self.is_speaking = True

        # La condición para determinar el fin del habla es científicamente rigurosa:
        # 1. El usuario debe haber hablado en algún momento (`self.is_speaking` es True).
        # 2. Debemos haber procesado suficientes chunks para llenar nuestra ventana de silencio (`len == maxlen`).
        # 3. La ventana de silencio debe estar compuesta enteramente de chunks de no-voz (`not any(history)`).
        if self.is_speaking and len(self.speech_history) == self.silence_chunks_needed and not any(self.speech_history):
            logger.info("VAD ha detectado una secuencia de silencio calificada. Fin del habla.")
            return True
            
        return False

    def get_audio_buffer(self) -> bytes:
        """Devuelve el buffer de audio acumulado que contiene la elocución completa."""
        return bytes(self.audio_buffer)

    def reset(self):
        """Resetea el estado del procesador para la próxima interacción."""
        self.speech_history.clear()
        self.audio_buffer.clear()
        self.is_speaking = False
        logger.info("Procesador VAD reseteado a su estado inicial.")
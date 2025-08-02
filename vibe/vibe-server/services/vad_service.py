import webrtcvad
from collections import deque
import logging

logger = logging.getLogger(__name__)

class VoiceActivityProcessor:
    """
    Procesa un stream de audio para detectar actividad de voz (VAD).
    VERSIÓN 2.0: Incluye un búfer de re-ensamblaje para manejar chunks de
    tamaño arbitrario, haciéndolo robusto y agnóstico al cliente.
    """
    def __init__(self, sample_rate: int = 16000, vad_aggressiveness: int = 3, silence_duration_ms: int = 1500):
        if sample_rate not in [8000, 16000, 32000, 48000]:
            raise ValueError("Frecuencia de muestreo no válida para VAD.")
        
        self.sample_rate = sample_rate
        self.vad = webrtcvad.Vad(vad_aggressiveness)
        
        # --- PARÁMETROS DEL CONTRATO INTERNO DEL VAD ---
        self.chunk_duration_ms = 30
        self.chunk_samples = int(sample_rate * self.chunk_duration_ms / 1000) # 480 muestras
        self.chunk_bytes = self.chunk_samples * 2  # 960 bytes (PCM 16-bit)

        self.silence_chunks_needed = silence_duration_ms // self.chunk_duration_ms
        self.speech_history = deque(maxlen=self.silence_chunks_needed)
        
        # --- LA NUEVA ARQUITECTURA: BÚFER DE RE-ENSAMBLAJE ---
        # Este búfer acumulará los datos que llegan del WebSocket.
        self.internal_buffer = bytearray()
        
        self.audio_buffer = bytearray()
        self.is_speaking = False

    def add_chunk(self, incoming_chunk: bytes):
        """
        Añade un chunk de audio de cualquier tamaño al búfer interno.
        Esta es la nueva función de entrada pública.
        """
        self.internal_buffer.extend(incoming_chunk)

    def process_stream(self) -> bool:
        """
        Procesa los datos en el búfer interno, extrayendo y analizando
        chunks del tamaño correcto para el VAD.

        Returns:
            bool: True si se ha detectado el final del habla, False en caso contrario.
        """
        # Mientras haya suficientes bytes en el búfer para formar un chunk válido para el VAD...
        while len(self.internal_buffer) >= self.chunk_bytes:
            # Extraer un chunk de 960 bytes del principio del búfer.
            frame_to_process = self.internal_buffer[:self.chunk_bytes]
            
            # Eliminar el chunk procesado del búfer.
            self.internal_buffer = self.internal_buffer[self.chunk_bytes:]

            # --- La lógica original ahora opera sobre el frame extraído ---
            try:
                is_speech = self.vad.is_speech(frame_to_process, self.sample_rate)
            except Exception as e:
                logger.error(f"Error en la validación del chunk por webrtcvad: {e}")
                continue # Salta al siguiente chunk si hay un error

            self.speech_history.append(is_speech)

            if is_speech or self.is_speaking:
                self.audio_buffer.extend(frame_to_process)

            if is_speech:
                self.is_speaking = True

            if self.is_speaking and len(self.speech_history) == self.silence_chunks_needed and not any(self.speech_history):
                logger.info("VAD ha detectado una secuencia de silencio calificada. Fin del habla.")
                return True
        
        # Si salimos del bucle, significa que no hay suficientes datos para procesar
        # o no se ha detectado el fin del habla.
        return False

    def get_audio_buffer(self) -> bytes:
        return bytes(self.audio_buffer)

    def reset(self):
        self.speech_history.clear()
        self.audio_buffer.clear()
        # IMPORTANTE: No reseteamos el internal_buffer, porque podría contener
        # un fragmento de un paquete que pertenece a la siguiente elocución.
        # Lo resetearemos explícitamente desde main.py si es necesario.
        self.is_speaking = False
        logger.info("Procesador VAD reseteado a su estado inicial.")
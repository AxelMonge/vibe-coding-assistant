# vibe/vibe-server/services/audio_decode.py
from __future__ import annotations
from dataclasses import dataclass
from typing import Tuple
import subprocess
from shutil import which

@dataclass(frozen=True)
class PCMParams:
    sample_rate: int       # 44100
    channels: int          # 2
    sample_width: int      # 2 (bytes) => s16le
    bytes_per_second: int  # sr * ch * sw
    duration_sec: float    # segundos (estimada por tamaño/bytes_per_second)

def _get_ffmpeg_exe() -> str:
    exe = which("ffmpeg") or which("ffmpeg.exe")
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception as e:
        raise RuntimeError("No se encontró ffmpeg (ni en PATH ni via imageio-ffmpeg).") from e

def decode_mp3_to_pcm441_stereo(mp3_path: str) -> Tuple[bytes, PCMParams]:
    """
    Decodifica MP3 -> PCM s16le, 44.1 kHz, estéreo usando ffmpeg (sin pydub/audioop).
    Devuelve (raw_bytes, params). duration_sec se estima por tamaño/bytes_per_second.
    """
    ffmpeg = _get_ffmpeg_exe()
    cmd = [
        ffmpeg,
        "-v", "error",      # solo errores
        "-nostdin",
        "-i", mp3_path,     # entrada
        "-f", "s16le",      # salida: signed 16-bit little endian
        "-acodec", "pcm_s16le",
        "-ar", "44100",     # sample rate
        "-ac", "2",         # estéreo
        "pipe:1"            # stdout
    ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if proc.returncode != 0:
        raise RuntimeError(f"ffmpeg falló: {proc.stderr.decode(errors='ignore')}")
    raw = proc.stdout
    sample_rate, channels, sample_width = 44100, 2, 2
    bps = sample_rate * channels * sample_width
    duration_sec = len(raw) / float(bps) if bps else 0.0
    params = PCMParams(
        sample_rate=sample_rate,
        channels=channels,
        sample_width=sample_width,
        bytes_per_second=bps,
        duration_sec=duration_sec,
    )
    return raw, params

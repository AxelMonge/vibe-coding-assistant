# test_speaker.py
from __future__ import annotations
import asyncio
import contextlib
import time
import subprocess
from dataclasses import dataclass
from shutil import which
from typing import Tuple

from fastapi import FastAPI, WebSocket, WebSocketDisconnect

app = FastAPI(title="Vibe Speaker Test (WS downlink)")

# ---------- Decode (MP3 -> PCM s16le 44.1kHz estéreo) ----------

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
    ffmpeg = _get_ffmpeg_exe()
    cmd = [
        ffmpeg,
        "-v", "error",
        "-nostdin",
        "-i", mp3_path,
        "-f", "s16le",
        "-acodec", "pcm_s16le",
        "-ar", "44100",
        "-ac", "2",
        "pipe:1",
    ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if proc.returncode != 0:
        raise RuntimeError(f"ffmpeg falló: {proc.stderr.decode(errors='ignore')}")
    raw = proc.stdout
    sample_rate, channels, sample_width = 44100, 2, 2
    bps = sample_rate * channels * sample_width
    duration_sec = len(raw) / float(bps) if bps else 0.0
    return raw, PCMParams(sample_rate, channels, sample_width, bps, duration_sec)

# ---------- WS helper ----------

def _slice_frames(raw: bytes, bytes_per_frame: int) -> list[memoryview]:
    total_full = len(raw) // bytes_per_frame
    return [memoryview(raw[i*bytes_per_frame:(i+1)*bytes_per_frame]) for i in range(total_full)]

@app.websocket("/ws/speaker-test")
async def speaker_test_ws(ws: WebSocket):
    await ws.accept()

    # Query params (seguros para WebSocket)
    qp = ws.query_params
    mp3_file = qp.get("file", "audio_test.mp3")
    try:
        frame_ms = int(qp.get("frame_ms", "30"))
    except ValueError:
        frame_ms = 30
    if frame_ms <= 0 or frame_ms > 1000:
        frame_ms = 30
    loop_flag = qp.get("loop", "1")  # "1"=loop, "0"=una sola vez
    loop_enabled = loop_flag not in ("0", "false", "False", "no", "No")

    # Decode una vez
    try:
        raw, p = decode_mp3_to_pcm441_stereo(mp3_file)
    except Exception as e:
        with contextlib.suppress(Exception):
            await ws.send_json({"type":"error","msg":str(e)})
            await ws.close(code=1011)
        return

    if not (p.sample_rate == 44100 and p.channels == 2 and p.sample_width == 2):
        with contextlib.suppress(Exception):
            await ws.send_json({"type":"error","msg":"Formato inesperado"})
            await ws.close(code=1011)
        return

    bytes_per_frame = int(p.bytes_per_second * (frame_ms / 1000.0))
    if bytes_per_frame <= 0:
        with contextlib.suppress(Exception):
            await ws.close(code=1011)
        return

    frames = _slice_frames(raw, bytes_per_frame)
    if not frames:
        with contextlib.suppress(Exception):
            await ws.close(code=1011)
        return

    start_msg = {"type":"audio.start","sr":p.sample_rate,"ch":p.channels,"fmt":"s16le","frame_ms":frame_ms}
    stop_msg = {"type":"audio.stop"}

    try:
        while True:
            await ws.send_json(start_msg)

            t0 = time.perf_counter()
            period = frame_ms / 1000.0

            for i, frame in enumerate(frames, start=1):
                await ws.send_bytes(frame)
                next_t = t0 + i * period
                now = time.perf_counter()
                delay = next_t - now
                if delay > 0:
                    await asyncio.sleep(delay)

            # pequeño colchón de silencio
            await ws.send_bytes(b"\x00" * bytes_per_frame)
            await ws.send_json(stop_msg)

            if not loop_enabled:
                break

            await asyncio.sleep(0.05)

    except WebSocketDisconnect:
        return
    except Exception:
        with contextlib.suppress(Exception):
            await ws.close(code=1011)

# ---------- Arranque directo ----------

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("test_speaker:app", host="0.0.0.0", port=8010, reload=False)

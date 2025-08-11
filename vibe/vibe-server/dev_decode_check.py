# vibe/vibe-server/dev_decode_check.py
from services.audio_decode import decode_mp3_to_pcm441_stereo

if __name__ == "__main__":
    pcm, p = decode_mp3_to_pcm441_stereo("test_audio.mp3")
    print("OK decode")
    print(f"sr={p.sample_rate}, ch={p.channels}, sw={p.sample_width}B, dur={p.duration_sec:.3f}s")
    print(f"bytes={len(pcm)}, bytes/s={p.bytes_per_second}")
    expected = int(p.bytes_per_second * p.duration_sec)
    print(f"expected≈{expected}, diff={abs(len(pcm)-expected)}")

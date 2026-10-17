
import wave
from pathlib import Path
from faster_whisper import WhisperModel

PROJECT_DIR = Path(__file__).resolve().parent.parent
AUDIO_DIR = PROJECT_DIR / "test_audio"
AUDIO_DIR.mkdir(exist_ok=True)

SAMPLE_RATE = 16000

print("Loading speech recognition model...")

model = WhisperModel(
    "base",
    device="cpu",
    compute_type="int8"
)

print("Speech recognition ready.")


def transcribe_pcm(pcm_data: bytes) -> str:
    if not pcm_data:
        return ""

    if len(pcm_data) % 2 != 0:
        raise ValueError("Invalid 16-bit PCM audio.")

    wav_path = AUDIO_DIR / "inmp441_recording.wav"

    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm_data)

    segments, _ = model.transcribe(
        str(wav_path),
        language="en",
        vad_filter=True
    )

    text = " ".join(
        segment.text.strip() for segment in segments
    ).strip()

    print("Recognized:", text if text else "(no speech)")
    return text

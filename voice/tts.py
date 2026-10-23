import audioop
import tempfile
import wave
from pathlib import Path

import pyttsx3

SAMPLE_RATE = 16000


def generate_pcm(text: str) -> bytes:
    """Generate 16 kHz, mono, signed 16-bit PCM without local playback."""

    with tempfile.TemporaryDirectory() as temp_dir:
        wav_path = Path(temp_dir) / "tts.wav"

        engine = pyttsx3.init()

        # List available Windows voices and select a female voice.
        voices = engine.getProperty("voices")
        selected_voice = None

        for voice in voices:
            print(f"Available voice: {voice.name} ({voice.id})")

            if "zira" in voice.name.lower():
                selected_voice = voice
                break

        if selected_voice is None:
            for voice in voices:
                if "female" in voice.name.lower():
                    selected_voice = voice
                    break

        if selected_voice is not None:
            engine.setProperty("voice", selected_voice.id)
            print(f"Selected voice: {selected_voice.name}")
        else:
            print("No known female voice found; using the Windows default.")

        engine.setProperty("rate", 175)
        engine.save_to_file(text, str(wav_path))
        engine.runAndWait()
        engine.stop()

        if not wav_path.exists() or wav_path.stat().st_size <= 44:
            raise RuntimeError("TTS did not generate a valid WAV file.")

        with wave.open(str(wav_path), "rb") as wav:
            channels = wav.getnchannels()
            sample_width = wav.getsampwidth()
            source_rate = wav.getframerate()
            pcm = wav.readframes(wav.getnframes())

        if sample_width != 2:
            raise RuntimeError(
                f"Expected 16-bit audio, got {sample_width * 8}-bit."
            )

        if channels == 2:
            pcm = audioop.tomono(pcm, 2, 0.5, 0.5)
        elif channels != 1:
            raise RuntimeError(f"Unsupported channel count: {channels}")

        if source_rate != SAMPLE_RATE:
            pcm, _ = audioop.ratecv(
                pcm, 2, 1, source_rate, SAMPLE_RATE, None
            )

        return pcm


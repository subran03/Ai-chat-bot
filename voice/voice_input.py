import sounddevice as sd

sample_rate = 16000
duration = 6
channels = 1


def record_voice():
    print("Start speaking...")

    recording = sd.rec(
        int(duration * sample_rate),
        samplerate=sample_rate,
        channels=channels,
        dtype="int16"
    )

    sd.wait()

    print("Recording finished.")
    return recording


if __name__ == "__main__":
    audio = record_voice()

    print("Voice input captured.")
    print("Audio samples:", len(audio))

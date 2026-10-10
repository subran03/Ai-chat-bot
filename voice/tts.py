import time
import pyttsx3
def speak(text):
    """Speak text through the laptop's default audio output."""
    if not isinstance(text, str) or not text.strip():
        print("TTS error: text is empty or invalid.")
        return None
    engine = None
    try:
        start_time = time.perf_counter()
        # Create a fresh engine for each response
        engine = pyttsx3.init()
        engine.setProperty("rate", 175)
        engine.say(text)
        engine.runAndWait()
        latency = time.perf_counter() - start_time
        print(f"TTS latency: {latency:.3f} seconds")
        return latency
    except Exception as error:
        print("TTS playback failed:", error)
        return None
    finally:
        if engine is not None:
            try:
                engine.stop()
            except Exception:
                pass
if __name__ == "__main__":
    while True:
        text = input("Enter text to speak (or exit): ")
        if text.strip().lower() == "exit":
            break
        speak(text)
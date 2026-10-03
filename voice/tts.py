import time
import pyttsx3
try:
    engine = pyttsx3.init()
except Exception as e:
    engine = None
    print("TTS engine could not be initialized.")
    print("Error:", e)
def speak(text):
    if engine is None:
        print("TTS is unavailable.")
        return None
    if not isinstance(text, str) or not text.strip():
        print("TTS error: text is empty or invalid.")
        return None
    try:
        start_time = time.perf_counter()
        engine.say(text)
        engine.runAndWait()
        end_time = time.perf_counter()
        latency = end_time - start_time
        print(f"TTS latency: {latency:.3f} seconds")
        return latency
    except Exception as e:
        print("TTS playback failed.")
        print("Error:", e)
        return None
if __name__ == "__main__":
    text = input("Enter text to speak: ")
    latency = speak(text)
    if latency is not None:
        print("Speech completed successfully.")
        print(f"Returned latency: {latency:.3f} seconds")
    else:
        print("Speech could not be completed.")
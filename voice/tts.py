import pyttsx3
import threading
import time
from pynput import keyboard as pynput_keyboard

engine = pyttsx3.init()
engine.setProperty("rate", 175)
engine.setProperty(
    "voice",
    r"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_EN-US_ZIRA_11.0"
)

stop_requested = threading.Event()
speaking = threading.Event()


def on_press(key):
    try:
        if key.char and key.char.lower() == "s" and speaking.is_set():
            stop_requested.set()
    except AttributeError:
        pass


listener = pynput_keyboard.Listener(on_press=on_press)
listener.start()


def speak(text):
    stop_requested.clear()
    speaking.set()
    start = time.time()
    loop_started = False

    try:
        engine.startLoop(False)
        loop_started = True
        engine.say(text)

        while engine.isBusy():
            if stop_requested.is_set():
                engine.stop()
                break

            engine.iterate()
            time.sleep(0.01)

    finally:
        if loop_started:
            try:
                engine.stop()
                engine.endLoop()
            except Exception as exc:
                print(f"TTS cleanup error: {exc}")

        speaking.clear()
        stop_requested.clear()

    print(f"TTS latency: {time.time() - start:.2f}s")

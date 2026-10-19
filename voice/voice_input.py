import sys
import wave
import threading
import traceback
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import requests
from faster_whisper import WhisperModel
PROJECT_DIR = Path(__file__).resolve().parent.parent
AUDIO_DIR = PROJECT_DIR / "test_audio"
AUDIO_DIR.mkdir(exist_ok=True)
SAMPLE_RATE = 16000
RECORD_SECONDS = 8
RECEIVER_PORT = 8001
AI_SERVER_URL = "http://127.0.0.1:8000"
sys.path.insert(0, str(PROJECT_DIR))
print("Loading speech recognition model...")
model = WhisperModel("base", device="cpu", compute_type="int8")
print("Speech recognition ready.")
processing_lock = threading.Lock()
def process_audio(pcm_data):
    wav_path = AUDIO_DIR / "inmp441_recording.wav"

    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm_data)

    print("\nAudio saved:", wav_path)
    print("Transcribing audio...")

    segments, _ = model.transcribe(
        str(wav_path),
        language="en",
        vad_filter=True
    )

    message = " ".join(
        segment.text.strip() for segment in segments
    ).strip()

    if not message:
        print("No speech recognized. Please try again.")
        return "No speech recognized."

    print("\nYou said:", message)
    print("Waiting for AI response...")

    try:
        response = requests.post(
            AI_SERVER_URL,
            json={"message": message},
            timeout=(5, 300)
        )
        response.raise_for_status()

        result = response.json()
        answer = result.get("response")

        if not isinstance(answer, str) or not answer.strip():
            raise RuntimeError(f"Invalid AI response: {result}")

        print("\nAI:", answer)

        try:
            from voice.tts import speak
            speak(answer)
        except Exception:
            print("Text-to-speech failed:")
            traceback.print_exc()

        return "Processing complete."

    except Exception as exc:
        print("AI request failed:", exc)
        traceback.print_exc()
        return "AI request failed."


class AudioHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/audio":
            self.send_error(404, "Use /audio")
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            expected_length = SAMPLE_RATE * RECORD_SECONDS * 2

            if length != expected_length:
                self.send_error(
                    400,
                    f"Expected {expected_length} bytes; received {length}"
                )
                return

            pcm_data = self.rfile.read(length)

            if len(pcm_data) != length:
                self.send_error(400, "Incomplete audio data")
                return

            # Wait for the previous answer to finish instead of returning 503.
            with processing_lock:
                result = process_audio(pcm_data)

            body = result.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)

            print("\nReady. Press r in ESP32 Serial Monitor to record again.")

        except (BrokenPipeError, ConnectionResetError):
            print("ESP32 disconnected while waiting for processing.")
        except Exception:
            print("Audio receiver error:")
            traceback.print_exc()
            try:
                self.send_error(500, "Audio processing failed")
            except (BrokenPipeError, ConnectionResetError):
                pass

    def log_message(self, format, *args):
        print("HTTP:", format % args)


if __name__ == "__main__":
    server = ThreadingHTTPServer(
        ("0.0.0.0", RECEIVER_PORT),
        AudioHandler
    )

    print(f"Audio receiver listening on port {RECEIVER_PORT}")
    print("Start the AI server separately.")
    print("Press r in ESP32 Serial Monitor to record.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping audio receiver...")
    finally:
        server.server_close()

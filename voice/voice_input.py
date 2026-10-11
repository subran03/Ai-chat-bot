import sys
import wave
from pathlib import Path
from http.server import BaseHTTPRequestHandler, HTTPServer
import requests
from faster_whisper import WhisperModel
PROJECT_DIR = Path(__file__).resolve().parent.parent
AUDIO_DIR = PROJECT_DIR / "test_audio"
AUDIO_DIR.mkdir(exist_ok=True)
SAMPLE_RATE = 16000
RECEIVER_PORT = 8001
AI_SERVER_URL = "http://127.0.0.1:8000"
sys.path.insert(0, str(PROJECT_DIR))
print("Loading speech recognition model...")
model = WhisperModel("base", device="cpu", compute_type="int8")
print("Speech recognition ready.")
def process_audio(pcm_data):
    wav_path = AUDIO_DIR / "inmp441_recording.wav"
    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm_data)
    print("Audio saved:", wav_path)
    segments, info = model.transcribe(
        str(wav_path),
        language="en",
        vad_filter=True
    )
    message = " ".join(segment.text.strip() for segment in segments).strip()
    if not message:
        print("No speech recognized. Please try again.")
        return
    print("\nYou said:", message)
    try:
        response = requests.post(
            AI_SERVER_URL,
            json={"message": message},
            timeout=120
        )
        response.raise_for_status()
        result = response.json()
        answer = result.get("response")
        if not isinstance(answer, str) or not answer.strip():
            print("The AI server did not return a valid answer:", result)
            return
        print("\nAI:", answer)
        try:
            from voice.tts import speak
            speak(answer)
        except Exception as exc:
            print("Could not use voice/tts.py:", exc)
            print("The AI response is shown above.")
    except requests.RequestException as exc:
        print("Could not contact the AI server:", exc)
class AudioHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/audio":
            self.send_error(404, "Use /audio")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            expected_length = SAMPLE_RATE * 5 * 2
            if length != expected_length:
                self.send_error(
                    400,
                    f"Expected {expected_length} audio bytes; received {length}"
                )
                return
            pcm_data = self.rfile.read(length)
            if len(pcm_data) != length:
                self.send_error(400, "Incomplete audio data")
                return
            process_audio(pcm_data)
            body = b"Audio processed. Check the Python terminal."
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except Exception as exc:
            print("Audio processing error:", exc)
            self.send_error(500, "Audio processing failed")
    def log_message(self, format, *args):
        print("HTTP:", format % args)
if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", RECEIVER_PORT), AudioHandler)
    print(f"Audio receiver listening on port {RECEIVER_PORT}")
    print("Start your AI server separately, then press r in ESP32 Serial Monitor.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping audio receiver.")
    finally:
        server.server_close()
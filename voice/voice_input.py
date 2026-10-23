import sys
import time
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

from voice.tts import generate_pcm

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

    whisper_start = time.perf_counter()
    segments, _ = model.transcribe(
        str(wav_path),
        language="en",
        vad_filter=True
    )
    message = " ".join(
        segment.text.strip() for segment in segments
    ).strip()

    print(f"Whisper transcription time: {time.perf_counter() - whisper_start:.2f}s")

    if not message:
        return "text/plain; charset=utf-8", b"No speech recognized."

    print("\nYou said:", message)
    print("Waiting for AI response...")

    try:
        gemma_start = time.perf_counter()
        response = requests.post(
            AI_SERVER_URL,
            json={"message": message},
            timeout=(5, 300)
        )
        response.raise_for_status()
        print(f"Gemma HTTP response time: {time.perf_counter() - gemma_start:.2f}s")

        result = response.json()
        answer = result.get("response")

        if not isinstance(answer, str) or not answer.strip():
            raise RuntimeError(f"Invalid AI response: {result}")

        print("\nAI:", answer)
        print("Generating speech audio...")

        tts_start = time.perf_counter()
        output_pcm = generate_pcm(answer)
        print(f"TTS generation time: {time.perf_counter() - tts_start:.2f}s")
        print(f"Audio generated: {len(output_pcm)} bytes, {len(output_pcm) / (SAMPLE_RATE * 2):.2f}s")

        if not output_pcm:
            raise RuntimeError("TTS generated empty audio.")

        # Mono, signed 16-bit little-endian PCM at 16 kHz.
        return "audio/pcm", output_pcm

    except Exception as exc:
        print("AI/TTS processing failed:", exc)
        traceback.print_exc()
        return "text/plain; charset=utf-8", f"AI/TTS failed: {exc}".encode("utf-8", errors="replace")


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

            with processing_lock:
                content_type, body = process_audio(pcm_data)

            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("X-Sample-Rate", str(SAMPLE_RATE))
            self.send_header("Connection", "close")
            self.end_headers()

            self.wfile.write(body)
            self.wfile.flush()

            if content_type == "audio/pcm":
                print("Speech audio sent to ESP32.")
            else:
                print("Text response sent to ESP32.")

        except (BrokenPipeError, ConnectionResetError):
            print("ESP32 disconnected during processing or audio transfer.")

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
    server = ThreadingHTTPServer(("0.0.0.0", RECEIVER_PORT), AudioHandler)
    print(f"Audio receiver listening on port {RECEIVER_PORT}")
    print("Start the AI server separately.")
    print("Press r in ESP32 Serial Monitor to record.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping audio receiver...")
    finally:
        server.server_close()

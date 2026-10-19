import json
import sys
from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
sys.path.append(str(Path(__file__).resolve().parents[1]))
from ai.assistant import get_response
class RequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"AI Chatbot server is running."
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
            if content_length <= 0 or content_length > 1_000_000:
                self.send_json(400, {"error": "Invalid request body"})
                return

            body = self.rfile.read(content_length)
            data = json.loads(body)
            message = data.get("message", "")

            if not isinstance(message, str) or not message.strip():
                self.send_json(400, {"error": "Message cannot be empty"})
                return

            print(f"Received message: {message}")
            ai_response = get_response(message)
            self.send_json(200, {"response": ai_response})

        except json.JSONDecodeError:
            self.send_json(400, {"error": "Invalid JSON"})
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            print("Client disconnected before the response was sent.")
        except Exception as exc:
            print(f"AI server error: {exc}")
            try:
                self.send_json(500, {"error": "Internal server error"})
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass

    def send_json(self, status, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        print("HTTP:", format % args)


class ChatServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


if __name__ == "__main__":
    server = ChatServer(("0.0.0.0", 8000), RequestHandler)
    print("AI server started on port 8000...")
    print("Waiting for connections...")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping AI server...")
    finally:
        server.server_close()

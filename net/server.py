import json
import sys
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
sys.path.append(str(Path(__file__).resolve().parents[1]))
from ai.assistant import get_response
class RequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.end_headers()
        self.wfile.write(b"AI Chatbot server is running. Send a POST request to chat.")
    def do_POST(self):
        try:
            content_length = int(self.headers.get("content-length", 0))
            body = self.rfile.read(content_length)
            data = json.loads(body)
            message = data.get("message", "")
            if not isinstance(message, str) or not message.strip():
                self.send_response(400)
                response = {"error": "Message cannot be empty"}
            else:
                ai_response = get_response(message)
                response = {"response": ai_response}
                self.send_response(200)
        except json.JSONDecodeError:
            self.send_response(500)
            response = {"error": "Invalid JSON."}
        except Exception as e:
            self.send_response(500)
            response = {"error": "Internal server error."}
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(response).encode())
server = HTTPServer(("0.0.0.0", 8000), RequestHandler)
print("Server started on port 8000...")
print("Waiting for connection...")
server.serve_forever()
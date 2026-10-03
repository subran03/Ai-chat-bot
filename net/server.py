from http.server import BaseHTTPRequestHandler, HTTPServer
import json
class RequestHandler(BaseHTTPRequestHandler):

    def do_POST(self):
        try:
            content_length =int(self.headers.get("content-length",0))
            body =self.rfile.read(content_length)
            data =json.loads(body)
            message =data.get("message", "")
            if not isinstance(message,str) or not message.strip():
                self.send_response(400)
                response ={"error": "Message cannot be empty"}
            else:
                response ={"response":f"received: {message}"}
                self.send_response(200)
        except json.JSONDecodeError:
                self.send_response(500)
                response ={"error": "Invalid JSON."}
        except Exception as e:
                self.send_response(500)
                response ={"error": "Internal server error."}

        self.send_header("content-type", "application/json")
        self.end_headers()

        self.wfile.write(json.dumps(response).encode())    
 

server =HTTPServer(("0.0.0.0", 8000), RequestHandler)

print("Server started on port 8000...")
print("Waiting for connection...")

server.serve_forever()

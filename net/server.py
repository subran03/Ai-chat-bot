from http.server import BaseHTTPRequestHandler, HTTPServer


class RequestHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        message = "Hello from AI Chatbot Server!"

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()

        self.wfile.write(message.encode())


server = HTTPServer(("0.0.0.0", 8000), RequestHandler)

print("Server started on port 8000...")
print("Waiting for connection...")

server.serve_forever()

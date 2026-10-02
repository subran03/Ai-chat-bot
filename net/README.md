# Networking Module
This module handles the communication between the client and the server for the AI chatbot.
## Files
- `server.py` - Runs the Python server and listens for client requests.
- `test/client.py` - A simple client used to test communication with the server.

## Current Progress
- Basic Python HTTP server created.
- Server tested successfully on localhost.
- Client program created for testing.
- Client successfully connects to the server and receives a response.

## Communication Flow
Client → HTTP Request → Server → HTTP Response → Client

## Developer
Subran Yadav

## Run and test locally
From the repository root, 
run `python net/server.py`, 
run `python net/test/client.py`
then open http://localhost:8000. 
Server response: `Hello from AI Chatbot Server!`
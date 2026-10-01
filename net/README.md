# Networking Module

## Responsibility
This module handles the network communication of the AI chatbot.

## Main Tasks
- Connect the ESP32 to Wi-Fi
- Send data from the ESP32 to the backend
- Receive responses from the backend
- Handle connection failures and timeouts
- Reconnect when the network connection is lost
- Measure network communication latency

## Communication Flow
ESP32
↓
Wi-Fi
↓
Backend
↓
AI Service
↓
Backend
↓
ESP32

## Developer
Subran Yadav

## Run and test locally
From the repository root, run `python .\net\server.py`, then open http://localhost:8000. The expected response is: `Hello from AI Chatbot Server!`
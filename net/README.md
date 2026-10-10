# Networking Module — AI Chatbot

## 1. Overview
The networking module connects the ESP32 to a Python HTTP server that integrates with the AI assistant. It allows users to enter questions through the Serial Monitor, send them to the server, and receive responses from the AI which was integrated.

### Main components
- **Microcontroller:** WEMOS LOLIN D32 (ESP32)
- **Firmware framework:** Arduino
- **Development environment:** PlatformIO
- **Communication protocol:** HTTP
- **Server:** Python HTTP server
- **Server port:** 8000
- **Request format:** JSON
- **Response format:** JSON

## 2. System Architecture
The communication flow is:
1. The ESP32 connects to a Wi-Fi network.
2. The user enters a question through the Serial Monitor.
3. The ESP32 sends an HTTP POST request containing the question to the Python server.
4. The server processes the request using the AI assistant integration.
5. The server returns a JSON response.
6. The ESP32 extracts and displays the AI response in the Serial Monitor.

## 3. Project Structure
net/
├── server.py
├── README.md
├── Test/
│   └── client.py
└── esp32/
    ├── platformio.ini
    ├── src/
    │   └── main.cpp
    ├── include/
    ├── lib/
    └── test/

## 4. Network Configuration
The ESP32 and Python server must be reachable over the same local network.
During testing, a Windows laptop Mobile Hotspot was used.
- **Wi-Fi band:** 2.4 GHz
- **Laptop hotspot IP during testing:** `192.168.137.1`
- **ESP32 IP during testing:** `192.168.137.15`
- **Server port:** `8000`
These IP addresses are examples from the test setup. They may change depending on the network configuration.
The server URL configured in the ESP32 firmware is:
const char* serverUrl = "http://192.168.137.1:8000";
Replace the IP address with the laptop's current hotspot-side IPv4 address if it changes.

## 5. Requirements
- Python installed on the laptop
- Required Python packages installed
- A valid AI API configuration for the assistant
- Visual Studio Code with PlatformIO IDE
- WEMOS LOLIN D32 ESP32 board
- USB data cable
- A Wi-Fi network accessible to both the ESP32 and laptop

## 6. Running the Python Server
Open a terminal at the repository root and run:
powershell:
python .\net\server.py
The server listens on port `8000`.
Expected startup output:
text:
Server started on port 8000...
Waiting for connection...
Keep the server running while testing the ESP32.
The AI API key must be configured through an environment variable and must not be committed to the repository.

## 7. Configuring and Uploading the ESP32
Open the PlatformIO project located at:
text
net/esp32/

Configure the Wi-Fi credentials in `src/main.cpp`:
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

Configure the server address:
const char* serverUrl = "http://YOUR_LAPTOP_IP:8000";
Replace the placeholders with the appropriate local values. Do not publish real Wi-Fi passwords or API keys.
Build and upload the firmware using PlatformIO.
Open the Serial Monitor and set the baud rate to `115200`.

## 8. HTTP Communication
The ESP32 sends an HTTP POST request with a JSON body similar to:

```json
{
  "message": "What is PlatformIO?"
}
```
The ESP32 uses ArduinoJson to parse the response and display the value of the `response` field.
An HTTP status code of `200` indicates that the request was processed successfully.
The firmware uses an HTTP timeout of 60 seconds to allow time for AI responses.

## 9. Testing and Results
The following functionality was verified during testing:
- ESP32 successfully connects to the laptop's 2.4 GHz hotspot.
- ESP32 obtains a local IP address.
- ESP32 sends HTTP POST requests to the Python server.
- The server returns HTTP `200` responses for successful requests.
- AI responses are received and displayed in readable form.
- Multiple consecutive questions can be submitted without restarting the ESP32.
The networking and server communication path was successfully tested using the Serial Monitor of the Platformio.

## 10. Troubleshooting

### ESP32 cannot connect to Wi-Fi
- Verify the Wi-Fi name and password.
- Ensure the hotspot is enabled.
- Use a 2.4 GHz Wi-Fi band.
- Check the connection messages in the Serial Monitor.

### HTTP request fails or times out
- Verify that the Python server is running.
- Check the server IP address and port.
- Ensure the laptop and ESP32 are connected to the same local network.
- Check Windows Firewall rules if inbound connections are blocked.
- Allow additional time for AI responses when necessary.

### ESP32 cannot reach the server
Do not use `localhost` as the server address in the ESP32 firmware. Use the laptop's reachable local IPv4 address instead.

## 11. Security Notes
- Never commit real Wi-Fi passwords or API keys.
- Keep API credentials in environment variables.
- Avoid exposing credentials in screenshots, logs, or public repositories.
- Use only trusted networks and rotate any credentials that have been exposed.

## 12. Conclusion
The networking module provides communication between the ESP32 and the Python-based AI server using Wi-Fi and HTTP. It supports sending user questions, receiving AI responses, and handling multiple consecutive requests through the Serial Monitor.

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "subran";
const char* password = "12345678";
const char* serverUrl = "http://192.168.137.1:8000";

void sendMessage(String message) 
{
    if (WiFi.status() != WL_CONNECTED) 
    {
        Serial.println("Wi-Fi disconnected. Reconnecting...");
        WiFi.reconnect();
        return;
    }

    HTTPClient http;
    http.setTimeout(60000);
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["message"] = message;

    String json;
    serializeJson(doc, json);

    Serial.println("Sending message to AI...");
    int httpCode = http.POST(json);

    Serial.print("HTTP response code: ");
    Serial.println(httpCode);


    if (httpCode > 0) 
    {
    String response = http.getString();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    Serial.print("AI:");
    String answer = doc["response"].as<String>();
    answer.replace("\\n", "\n");
    Serial.println(answer);

    if (!error && doc["response"].is<const char*>()) 
    {
        Serial.println("AI:");
        String answer = doc["response"].as<String>();
        answer.replace("\\n", "\n");
        Serial.println(answer);
    } 
    else 
    {
        Serial.println("Could not parse the server response.");
        Serial.println(response);
    }
    }
    else 
    {
        Serial.print("Request failed: ");
        Serial.println(http.errorToString(httpCode));
    }

    http.end();
}
void setup() 
{
    Serial.begin(115200);
    Serial.setTimeout(30000);
    delay(1000);

    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Ask your question to the AI:");
}
void loop() 
{
    static String message = "";
    while (Serial.available()) 
    {
        char c = Serial.read();
        if (c == '\n' || c == '\r') 
        {
            if (message.length() > 0) 
            {
                Serial.println();
                sendMessage(message);
                message = "";
                Serial.println();
                Serial.print("Ask your question to the AI:");
            }
        }
        else if (c == '\b' || c == 127)
        {
            if (message.length() > 0) 
            {
                message.remove(message.length() - 1);
                Serial.print("\b \b");
            }
        }
        else 
        {
            message += c;
            Serial.print(c);
        }
    }
}

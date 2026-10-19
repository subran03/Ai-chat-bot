#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <driver/i2s.h>

const char* ssid = "subran";
const char* password = "12345678";
const char* serverIP = "192.168.137.1";
const uint16_t serverPort = 8001;

#define I2S_BCLK 26
#define I2S_WS   25
#define I2S_SD   33

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 8
#define SAMPLE_COUNT (SAMPLE_RATE * RECORD_SECONDS)
#define CHUNK_SAMPLES 1024

const unsigned long RESPONSE_TIMEOUT_MS = 300000;

bool setupMicrophone() {
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK;
    pins.ws_io_num = I2S_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = I2S_SD;

    esp_err_t result =
        i2s_driver_install(I2S_NUM_0, &config, 0, nullptr);

    if (result != ESP_OK) {
        Serial.printf("I2S driver install failed: %d\n", result);
        return false;
    }

    result = i2s_set_pin(I2S_NUM_0, &pins);

    if (result != ESP_OK) {
        Serial.printf("I2S pin setup failed: %d\n", result);
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("Microphone interface ready.");
    return true;
}

bool writeAll(WiFiClient& client, const uint8_t* data, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        size_t written = client.write(data + sent, length - sent);

        if (written == 0) {
            return false;
        }

        sent += written;
    }

    return true;
}

// Reads one HTTP line, allowing for CRLF line endings.
bool readHttpLine(WiFiClient& client, String& line,
                  unsigned long& lastProgress) {
    line = "";

    while (millis() - lastProgress < RESPONSE_TIMEOUT_MS) {
        while (client.available()) {
            char c = (char)client.read();
            lastProgress = millis();

            if (c == '\n') {
                line.trim();
                return true;
            }

            if (c != '\r') {
                line += c;

                // Avoid allocating unlimited memory for a malformed header.
                if (line.length() > 1024) {
                    return false;
                }
            }
        }

        if (!client.connected() && !client.available()) {
            return false;
        }

        delay(5);
    }

    return false;
}

bool readHttpResponse(WiFiClient& client) {
    client.setTimeout(10000);

    unsigned long lastProgress = millis();
    String line;

    // Read HTTP status line.
    if (!readHttpLine(client, line, lastProgress)) {
        Serial.println("Timed out waiting for HTTP status.");
        return false;
    }

    String statusLine = line;
    Serial.print("Server response: ");
    Serial.println(statusLine);

    // Read headers and find Content-Length.
    int contentLength = -1;

    while (true) {
        if (!readHttpLine(client, line, lastProgress)) {
            Serial.println("Failed while reading HTTP headers.");
            return false;
        }

        if (line.length() == 0) {
            break;
        }

        String lowerLine = line;
        lowerLine.toLowerCase();

        if (lowerLine.startsWith("content-length:")) {
            contentLength =
                line.substring(line.indexOf(':') + 1).toInt();
        }
    }

    // This receiver should return Content-Length for its response body.
    if (contentLength < 0) {
        Serial.println("Missing Content-Length header.");
        return false;
    }

    Serial.println("HTTP headers received. Reading response body...");

    // Read exactly Content-Length bytes. Do not wait for connection close.
    int received = 0;

    while (received < contentLength) {
        while (client.available() && received < contentLength) {
            char c = (char)client.read();
            Serial.write(c);
            received++;
            lastProgress = millis();
        }

        if (received >= contentLength) {
            break;
        }

        if (!client.connected() && !client.available()) {
            Serial.println("\nConnection closed before body was complete.");
            return false;
        }

        if (millis() - lastProgress >= RESPONSE_TIMEOUT_MS) {
            Serial.println("\nTimed out reading HTTP response body.");
            return false;
        }

        delay(5);
    }

    Serial.println();
    return statusLine.indexOf("200") >= 0;
}

void sendAudio() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected.");
        return;
    }

    WiFiClient client;
    client.setTimeout(10000);

    Serial.println("Connecting to laptop audio receiver...");

    if (!client.connect(serverIP, serverPort)) {
        Serial.println("Connection failed. Check laptop IP and port.");
        return;
    }

    client.print("POST /audio HTTP/1.1\r\n");
    client.printf("Host: %s:%u\r\n", serverIP, serverPort);
    client.print("Content-Type: application/octet-stream\r\n");
    client.printf("Content-Length: %u\r\n",
                  (unsigned int)(SAMPLE_COUNT * 2));
    client.print("Connection: close\r\n\r\n");

    int32_t rawSamples[CHUNK_SAMPLES];
    int16_t pcmSamples[CHUNK_SAMPLES];

    size_t totalSamples = 0;
    int32_t peakLevel = 0;

    Serial.println("Recording... Speak now!");

    while (totalSamples < SAMPLE_COUNT) {
        size_t bytesRead = 0;

        esp_err_t result = i2s_read(
            I2S_NUM_0,
            rawSamples,
            sizeof(rawSamples),
            &bytesRead,
            portMAX_DELAY
        );

        if (result != ESP_OK || bytesRead == 0) {
            Serial.println("Microphone read failed.");
            client.stop();
            return;
        }

        size_t samplesRead = bytesRead / sizeof(int32_t);
        size_t remaining = SAMPLE_COUNT - totalSamples;

        if (samplesRead > remaining) {
            samplesRead = remaining;
        }

        for (size_t i = 0; i < samplesRead; i++) {
            int32_t sample = rawSamples[i] >> 14;

            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;

            pcmSamples[i] = (int16_t)sample;

            int32_t magnitude = abs((int)pcmSamples[i]);
            if (magnitude > peakLevel) {
                peakLevel = magnitude;
            }
        }

        if (!writeAll(
                client,
                (const uint8_t*)pcmSamples,
                samplesRead * sizeof(int16_t))) {
            Serial.println("Audio upload failed.");
            client.stop();
            return;
        }

        totalSamples += samplesRead;
    }

    Serial.println("Recording complete.");
    Serial.printf("Audio peak level: %ld\n", (long)peakLevel);
    Serial.println("Waiting for AI response and speech to finish...");

    bool success = readHttpResponse(client);

    client.stop();

    if (success) {
        Serial.println("Request completed successfully.");
    } else {
        Serial.println("Request finished with an error or timeout.");
    }

    Serial.println("\nReady. Press r then Enter to record again.");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\nConnecting to Wi-Fi...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    if (!setupMicrophone()) {
        Serial.println("Microphone initialization failed.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Press r then Enter to record 8 seconds of audio.");
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();

        if (c == 'r' || c == 'R') {
            while (Serial.available()) {
                Serial.read();
            }

            sendAudio();
        }
    }

    delay(5);
}

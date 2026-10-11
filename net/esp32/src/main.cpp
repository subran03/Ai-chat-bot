
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <driver/i2s.h>

// Wi-Fi hotspot credentials
const char* ssid = "subran";
const char* password = "12345678";

// Laptop running voice_input.py
const char* serverIP = "192.168.137.67";
const uint16_t serverPort = 8001;

// INMP441 microphone pins
#define I2S_BCLK 26
#define I2S_WS   25
#define I2S_SD   33

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 5
#define SAMPLE_COUNT (SAMPLE_RATE * RECORD_SECONDS)
#define CHUNK_SAMPLES 1024

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

void sendAudio() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected. Please reconnect.");
        return;
    }

    WiFiClient client;
    client.setTimeout(120000);

    Serial.println("Connecting to laptop audio receiver...");

    if (!client.connect(serverIP, serverPort)) {
        Serial.println("Connection failed. Check laptop IP and Wi-Fi.");
        return;
    }

    // Send HTTP headers for 5 seconds of mono, 16-bit PCM audio.
    client.print("POST /audio HTTP/1.1\r\n");
    client.printf("Host: %s:%u\r\n", serverIP, serverPort);
    client.print("Content-Type: application/octet-stream\r\n");
    client.printf("Content-Length: %u\r\n", SAMPLE_COUNT * 2);
    client.print("Connection: close\r\n\r\n");

    int32_t rawSamples[CHUNK_SAMPLES];
    int16_t pcmSamples[CHUNK_SAMPLES];

    size_t totalSamples = 0;
    int32_t peakLevel = 0;

    Serial.println("Recording for 5 seconds. Speak now!");

    while (totalSamples < SAMPLE_COUNT) {
        size_t remaining = SAMPLE_COUNT - totalSamples;
        size_t samplesToRead =
            remaining < CHUNK_SAMPLES ? remaining : CHUNK_SAMPLES;

        size_t bytesRead = 0;

        esp_err_t result = i2s_read(
            I2S_NUM_0,
            rawSamples,
            samplesToRead * sizeof(int32_t),
            &bytesRead,
            portMAX_DELAY
        );

        if (result != ESP_OK || bytesRead == 0) {
            Serial.println("Microphone read failed.");
            client.stop();
            return;
        }

        size_t count = bytesRead / sizeof(int32_t);

    for (size_t i = 0; i < count; i++) {
        if (totalSamples == 0 && i < 10) {
            Serial.printf("Raw I2S sample %u: %ld\n",
                            (unsigned)i, (long)rawSamples[i]);
        }

        int32_t sample = rawSamples[i] >> 16;

        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;

        pcmSamples[i] = (int16_t)sample;

        int32_t level = abs((int)pcmSamples[i]);
        if (level > peakLevel) {
            peakLevel = level;
        }
    }

        if (!writeAll(
                client,
                (const uint8_t*)pcmSamples,
                count * sizeof(int16_t))) {
            Serial.println("Audio upload failed.");
            client.stop();
            return;
        }

        totalSamples += count;
    }

    Serial.printf("Microphone peak level: %ld\n", (long)peakLevel);

    if (peakLevel < 100) {
        Serial.println("WARNING: Audio level is extremely low.");
    } else {
        Serial.println("Audio signal detected.");
    }

    Serial.println("Audio sent. Waiting for Python server...");

    unsigned long start = millis();

    while (millis() - start < 120000) {
        while (client.available()) {
            Serial.write(client.read());
        }

        if (!client.connected() && !client.available()) {
            break;
        }

        delay(10);
    }

    client.stop();
    Serial.println("\nFinished. Type r to record again.");
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
        return;
    }

    Serial.println("Type r and press Enter to record.");
}

void loop() {
    if (Serial.available()) {
        char command = Serial.read();

        if (command == 'r' || command == 'R') {
            sendAudio();
        }
    }
}

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <driver/i2s.h>

const char* ssid = "subran";
const char* password = "12345678";
const char* serverIP = "192.168.137.1";
const uint16_t serverPort = 8001;

#define MIC_BCLK 26
#define MIC_WS   25
#define MIC_SD   33

#define SPK_BCLK 27
#define SPK_LRC  14
#define SPK_DIN  22

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 8
#define SAMPLE_COUNT (SAMPLE_RATE * RECORD_SECONDS)
#define CHUNK_SAMPLES 256

const unsigned long RESPONSE_TIMEOUT_MS = 300000;

// Static buffers reduce stack use and help prevent stack overflow.
static int32_t micRawSamples[CHUNK_SAMPLES];
static int16_t micPcmSamples[CHUNK_SAMPLES];

static int16_t speakerMonoSamples[CHUNK_SAMPLES];
static int16_t speakerStereoSamples[CHUNK_SAMPLES * 2];
static uint8_t textBuffer[128];

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
    pins.bck_io_num = MIC_BCLK;
    pins.ws_io_num = MIC_WS;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = MIC_SD;

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &config, 0, nullptr);

    if (err != ESP_OK) {
        Serial.printf("Mic I2S install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_NUM_0, &pins);

    if (err != ESP_OK) {
        Serial.printf("Mic I2S pin setup failed: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("Microphone ready.");
    return true;
}

bool setupSpeaker() {
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = SPK_BCLK;
    pins.ws_io_num = SPK_LRC;
    pins.data_out_num = SPK_DIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(I2S_NUM_1, &config, 0, nullptr);

    if (err != ESP_OK) {
        Serial.printf("Speaker I2S install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_NUM_1, &pins);

    if (err != ESP_OK) {
        Serial.printf("Speaker I2S pin setup failed: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_1);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_1);
    Serial.println("Speaker ready.");
    return true;
}

bool writeAll(WiFiClient& client, const uint8_t* data, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        size_t n = client.write(data + sent, length - sent);

        if (n == 0) {
            return false;
        }

        sent += n;
    }

    return true;
}

bool readHttpLine(
    WiFiClient& client,
    String& line,
    unsigned long& lastProgress
) {
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

                if (line.length() > 1024) {
                    return false;
                }
            }
        }

        if (!client.connected() && !client.available()) {
            return false;
        }

        delay(2);
    }

    return false;
}

bool readExact(
    WiFiClient& client,
    uint8_t* dst,
    size_t length,
    unsigned long& lastProgress
) {
    size_t received = 0;

    while (received < length) {
        int availableBytes = client.available();

        if (availableBytes > 0) {
            size_t wanted = length - received;

            if (wanted > (size_t)availableBytes) {
                wanted = (size_t)availableBytes;
            }

            int n = client.read(dst + received, wanted);

            if (n > 0) {
                received += (size_t)n;
                lastProgress = millis();
            }
        } else {
            if (!client.connected()) {
                return false;
            }

            if (millis() - lastProgress >= RESPONSE_TIMEOUT_MS) {
                return false;
            }

            delay(2);
        }
    }

    return true;
}

bool playPcmResponse(WiFiClient& client, int contentLength) {
    if (contentLength <= 0 || (contentLength % 2) != 0) {
        Serial.println("Invalid PCM payload length.");
        return false;
    }

    size_t remaining = (size_t)contentLength;
    unsigned long lastProgress = millis();

    Serial.printf(
        "Playing %.2f seconds of speech...\n",
        remaining / (float)(SAMPLE_RATE * 2)
    );

    while (remaining > 0) {
        size_t bytesToRead = remaining;

        if (bytesToRead > sizeof(speakerMonoSamples)) {
            bytesToRead = sizeof(speakerMonoSamples);
        }

        // PCM samples are 16-bit, so keep reads sample-aligned.
        bytesToRead &= ~((size_t)1);

        if (!readExact(
                client,
                (uint8_t*)speakerMonoSamples,
                bytesToRead,
                lastProgress
            )) {
            Serial.println("Audio download incomplete.");
            return false;
        }

        size_t sampleCount = bytesToRead / sizeof(int16_t);

        // MAX98357A receives stereo I2S frames; duplicate mono audio.
        for (size_t i = 0; i < sampleCount; i++) {
            speakerStereoSamples[2 * i] = speakerMonoSamples[i];
            speakerStereoSamples[2 * i + 1] = speakerMonoSamples[i];
        }

        const size_t bytesExpected =
            sampleCount * 2 * sizeof(int16_t);

        size_t bytesWritten = 0;

        esp_err_t err = i2s_write(
            I2S_NUM_1,
            speakerStereoSamples,
            bytesExpected,
            &bytesWritten,
            pdMS_TO_TICKS(1000)
        );

        if (err != ESP_OK) {
            Serial.printf("Speaker I2S write failed: %d\n", err);
            return false;
        }

        if (bytesWritten != bytesExpected) {
            Serial.printf(
                "Incomplete I2S write: %u of %u bytes\n",
                (unsigned int)bytesWritten,
                (unsigned int)bytesExpected
            );
            return false;
        }

        remaining -= bytesToRead;
    }

    // Allow the final buffered samples to finish.
    // This legacy driver version does not expose i2s_wait_tx_done().
    delay(150);

    Serial.println("Playback finished.");
    return true;
}

bool readHttpResponse(WiFiClient& client) {
    client.setTimeout(10000);

    unsigned long lastProgress = millis();
    String line;

    if (!readHttpLine(client, line, lastProgress)) {
        Serial.println("Timed out reading HTTP status.");
        return false;
    }

    String statusLine = line;

    Serial.print("Server response: ");
    Serial.println(statusLine);

    int contentLength = -1;
    String contentType = "";

    while (true) {
        if (!readHttpLine(client, line, lastProgress)) {
            Serial.println("Failed reading HTTP headers.");
            return false;
        }

        if (line.length() == 0) {
            break;
        }

        String lower = line;
        lower.toLowerCase();

        if (lower.startsWith("content-length:")) {
            contentLength =
                line.substring(line.indexOf(':') + 1).toInt();
        } else if (lower.startsWith("content-type:")) {
            contentType = line.substring(line.indexOf(':') + 1);
            contentType.trim();
            contentType.toLowerCase();
        }
    }

    if (contentLength < 0) {
        Serial.println("Missing Content-Length.");
        return false;
    }

    if (statusLine.indexOf("200") < 0) {
        Serial.println("HTTP request failed.");
        return false;
    }

    if (contentType.startsWith("audio/pcm")) {
        return playPcmResponse(client, contentLength);
    }

    Serial.print("Server message: ");

    size_t remaining = (size_t)contentLength;
    unsigned long lastProgressText = millis();

    while (remaining > 0) {
        size_t n = remaining > sizeof(textBuffer)
            ? sizeof(textBuffer)
            : remaining;

        if (!readExact(
                client,
                textBuffer,
                n,
                lastProgressText
            )) {
            Serial.println("\nIncomplete text response.");
            return false;
        }

        Serial.write(textBuffer, n);
        remaining -= n;
    }

    Serial.println();
    return false;
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
    client.printf(
        "Content-Length: %u\r\n",
        (unsigned int)(SAMPLE_COUNT * sizeof(int16_t))
    );
    client.print("Connection: close\r\n\r\n");

    size_t totalSamples = 0;
    int32_t peakLevel = 0;

    Serial.println("Recording... Speak now!");

    while (totalSamples < SAMPLE_COUNT) {
        size_t bytesRead = 0;

        esp_err_t err = i2s_read(
            I2S_NUM_0,
            micRawSamples,
            sizeof(micRawSamples),
            &bytesRead,
            portMAX_DELAY
        );

        if (err != ESP_OK || bytesRead == 0) {
            Serial.println("Microphone read failed.");
            client.stop();
            return;
        }

        size_t samplesRead = bytesRead / sizeof(int32_t);
        size_t remainingSamples = SAMPLE_COUNT - totalSamples;

        if (samplesRead > remainingSamples) {
            samplesRead = remainingSamples;
        }

        for (size_t i = 0; i < samplesRead; i++) {
            int32_t sample = micRawSamples[i] >> 14;

            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;

            micPcmSamples[i] = (int16_t)sample;

            int32_t magnitude = abs((int)micPcmSamples[i]);

            if (magnitude > peakLevel) {
                peakLevel = magnitude;
            }
        }

        if (!writeAll(
                client,
                (const uint8_t*)micPcmSamples,
                samplesRead * sizeof(int16_t)
            )) {
            Serial.println("Audio upload failed.");
            client.stop();
            return;
        }

        totalSamples += samplesRead;
    }

    Serial.println("Recording complete.");
    Serial.printf("Audio peak level: %ld\n", (long)peakLevel);
    Serial.println("Waiting for AI response and speaker playback...");

    bool success = readHttpResponse(client);
    client.stop();

    Serial.println(
        success
            ? "Request completed successfully."
            : "Request finished with an error."
    );

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

    if (!setupSpeaker()) {
        Serial.println("Speaker initialization failed.");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Press r to record 8 seconds of audio.");
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

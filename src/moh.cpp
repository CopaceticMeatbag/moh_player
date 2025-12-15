/**
 * Music On Hold Device - Final PSRAM Version (Optimized 1KB Chunks)
 * - Chunk Size reduced to 1024 to match modem firmware limit
 * - Parsing logic hardened to handle A7670 response quirks
 */

#define TINY_GSM_RX_BUFFER 1024
// #define DUMP_AT_COMMANDS

#include "utilities.h"
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>
#include "Audio.h"

// --- PSRAM Config ---
#ifndef BOARD_HAS_PSRAM
    #error "Please enable PSRAM in Arduino IDE: Tools > PSRAM > Enabled"
#endif
#define LARGE_BUFFER_SIZE (2048 * 1024) 

// UPDATED: Set to 1024 to match modem firmware behavior
#define MODEM_READ_SIZE 1024

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// I2S Pins
#define I2S_BCLK      21
#define I2S_LRC       22
#define I2S_DOUT      23

// Audio Settings
#define AUDIO_FILE_URL "https://messagesonhold.com.au/uploads/client-audio-wavs/ritz.mp3" 
#define AUDIO_FILE_PATH "/holdfdfad_mus.mp3"
#define DOWNLOAD_CHECK_INTERVAL_MS (30 * 60 * 1000) 

Audio audio;
bool fileReady = false;
unsigned long lastDownloadCheck = 0;
uint8_t* psramBuf = nullptr;

// Forward declarations
void shutdownModem();
bool powerOnModem();
bool connectNetwork();
void disconnectNetwork();
bool downloadAudioFile();
void checkForNewAudio();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Music On Hold Device (1KB Chunk Version) ===\n");

    if (psramInit()) {
        Serial.print("PSRAM Ready. Free: ");
        Serial.print(ESP.getFreePsram() / 1024);
        Serial.println(" KB");
    } else {
        Serial.println("PSRAM Init Failed! Halt.");
        while(1) delay(1000);
    }

    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    delay(100);

    Serial.println("Initializing SD card...");
    SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN);
    if (!SD.begin(BOARD_SD_CS_PIN)) {
        Serial.println("SD card init FAILED");
        while(1) delay(1000);
    }
    Serial.println("SD card OK");

    Serial.println("Initializing I2S audio...");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(15); 
    Serial.println("I2S OK");

    if (SD.exists(AUDIO_FILE_PATH)) {
        Serial.println("Audio file found on SD card");
        fileReady = true;
    } else {
        Serial.println("No audio file found, downloading...");
        checkForNewAudio(); 
    }

    if (fileReady) {
        Serial.println("Starting audio playback loop...");
        audio.connecttoFS(SD, AUDIO_FILE_PATH);
    }

    lastDownloadCheck = millis();
}

void loop() {
    if (fileReady) {
        audio.loop();
    }

    if (millis() - lastDownloadCheck >= DOWNLOAD_CHECK_INTERVAL_MS) {
        Serial.println("\n--- 30-minute check triggered ---");
        checkForNewAudio();
        lastDownloadCheck = millis();
    }
}

void checkForNewAudio() {
    bool wasPlaying = fileReady;
    
    if (wasPlaying) {
        audio.stopSong();
    }

    if (!powerOnModem()) {
        Serial.println("Modem init failed");
        shutdownModem();
        if (wasPlaying) audio.connecttoFS(SD, AUDIO_FILE_PATH);
        return;
    }

    if (!connectNetwork()) {
        Serial.println("Network connection failed");
        shutdownModem();
        if (wasPlaying) audio.connecttoFS(SD, AUDIO_FILE_PATH);
        return;
    }

    Serial.print("Allocating 512KB PSRAM buffer... ");
    psramBuf = (uint8_t*)ps_malloc(LARGE_BUFFER_SIZE);
    if (!psramBuf) {
        Serial.println("FAILED! Not enough PSRAM.");
        disconnectNetwork();
        shutdownModem();
        if (wasPlaying) audio.connecttoFS(SD, AUDIO_FILE_PATH);
        return;
    }
    Serial.println("OK");

    bool downloadSuccess = downloadAudioFile();

    free(psramBuf);
    psramBuf = nullptr;

    disconnectNetwork();
    shutdownModem();

    if (downloadSuccess) {
        fileReady = true;
        Serial.println("Restarting playback with new file...");
        audio.connecttoFS(SD, AUDIO_FILE_PATH);
    } else {
        Serial.println("Download failed, reverting to old file");
        if (wasPlaying) audio.connecttoFS(SD, AUDIO_FILE_PATH);
    }
}

// --- CORE DOWNLOAD LOGIC ---
bool downloadAudioFile() {
    Serial.println("\n--- Downloading Audio File (Optimized 1KB) ---");
    Serial.print("URL: ");
    Serial.println(AUDIO_FILE_URL);

    modem.sendAT("+HTTPINIT");
    if (modem.waitResponse() != 1) return false;

    modem.sendAT("+HTTPPARA=\"URL\",\"", AUDIO_FILE_URL, "\"");
    if (modem.waitResponse() != 1) return false;

    Serial.println("Setting User-Agent...");
    modem.sendAT("+HTTPPARA=\"USERDATA\",\"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.110 Safari/537.36\"");
    if (modem.waitResponse() != 1) Serial.println("Warning: Failed to set USERDATA headers");

    Serial.println("Sending GET Request...");
    modem.sendAT("+HTTPACTION=0");
    if (modem.waitResponse() != 1) { // Wait for initial OK
        Serial.println("CMD Failed");
        modem.sendAT("+HTTPTERM");
        return false;
    }

    Serial.println("Waiting for server response...");
    if (modem.waitResponse(60000UL, GF("+HTTPACTION:")) != 1) {
        Serial.println("Timeout waiting for HTTP Status");
        modem.sendAT("+HTTPTERM");
        return false;
    }

    // Parse Status
    SerialAT.readStringUntil(','); // Method
    int status = SerialAT.parseInt(); // Status Code
    SerialAT.readStringUntil(','); // Comma
    long contentLength = SerialAT.parseInt(); // Length
    SerialAT.readStringUntil('\n'); 

    Serial.print("Status: "); Serial.println(status);
    Serial.print("Size: "); Serial.println(contentLength);

    if (status != 200 || contentLength <= 0) {
        Serial.println("HTTP Error or Invalid Size");
        modem.sendAT("+HTTPTERM");
        return false;
    }

    if (SD.exists(AUDIO_FILE_PATH)) SD.remove(AUDIO_FILE_PATH);
    File file = SD.open(AUDIO_FILE_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("SD Create Failed");
        modem.sendAT("+HTTPTERM");
        return false;
    }

    long totalDownloaded = 0;
    size_t bufferOffset = 0; 

    // 6. Download Loop
    while (totalDownloaded < contentLength) {
        int requestSize = min((long)MODEM_READ_SIZE, contentLength - totalDownloaded);
        
        modem.sendAT("+HTTPREAD=", requestSize);
        
        // The modem immediately sends "OK", then possibly empty "+HTTPREAD:", then real header
        // We assume "OK" is consumed by waitResponse searching for "+HTTPREAD:"
        
        int len = 0;
        unsigned long loopStart = millis();
        
        // Loop to find the ACTUAL data header (skipping "OK" and phantom empty headers)
        while (len == 0 && millis() - loopStart < 5000) {
            if (modem.waitResponse(1000, GF("+HTTPREAD:")) == 1) {
                len = SerialAT.parseInt();
                SerialAT.readStringUntil('\n'); 
            } else {
                // Keep waiting or break if needed
            }
        }

        if (len > 0) {
            int bytesRecv = SerialAT.readBytes(psramBuf + bufferOffset, len);
            if (bytesRecv != len) {
                Serial.println("Stream mismatch!");
                break;
            }
            bufferOffset += len;
            totalDownloaded += len;
        } else {
            Serial.println("Timeout waiting for data header");
            break;
        }

        // Consume the trailing "+HTTPREAD: 0" that A7670 sends
        // We wait briefly for it; if it doesn't appear, we continue anyway
        modem.waitResponse(200, GF("+HTTPREAD: 0"));

        // Flush PSRAM to SD if full or finished
        if (bufferOffset >= LARGE_BUFFER_SIZE || totalDownloaded == contentLength) {
            Serial.print("Flushing "); Serial.print(bufferOffset); Serial.print(" bytes... ");
            if (file.write(psramBuf, bufferOffset) != bufferOffset) {
                Serial.println("SD Fail");
                break;
            }
            Serial.println("OK");
            bufferOffset = 0;
            Serial.print("Progress: "); Serial.print(totalDownloaded);
            Serial.print("/"); Serial.println(contentLength);
        }
    }

    file.close();
    modem.sendAT("+HTTPTERM");
    modem.waitResponse();

    Serial.println("\nDownload finished");
    return (totalDownloaded == contentLength);
}

// --- Helper Functions ---
bool powerOnModem() {
    Serial.println("\n--- Powering On Modem ---");
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    pinMode(MODEM_DTR_PIN, OUTPUT); digitalWrite(MODEM_DTR_PIN, LOW);
    pinMode(BOARD_PWRKEY_PIN, OUTPUT); digitalWrite(BOARD_PWRKEY_PIN, LOW);

    bool alreadyOn = false;
    for (int i = 0; i < 10; i++) {
        if (modem.testAT(1000)) { alreadyOn = true; break; }
        delay(500);
    }
    
    if (!alreadyOn) {
        digitalWrite(BOARD_PWRKEY_PIN, HIGH); delay(100); digitalWrite(BOARD_PWRKEY_PIN, LOW);
        delay(3000);
        int retry = 0;
        while (!modem.testAT(1000)) {
            if (retry++ > 20) return false;
            delay(500);
        }
    } else {
        modem.sendAT("+CFUN=1,1");
        modem.waitResponse(10000);
        delay(5000);
        int retry = 0;
        while (!modem.testAT(1000)) {
            if (retry++ > 20) return false;
            delay(500);
        }
    }

    modem.sendAT("+IPR=921600");
    if (modem.waitResponse(3000) != 1) return false;
    delay(100);
    SerialAT.updateBaudRate(921600);
    delay(100);
    return true;
}

bool connectNetwork() {
    Serial.println("\n--- Connecting Network ---");
    if (!modem.waitForNetwork(60000L)) return false;
    modem.sendAT("+NETCLOSE"); modem.waitResponse(5000);
    if (!modem.gprsConnect("telstra.wap")) return false;
    return true;
}

void disconnectNetwork() {
    modem.sendAT("+HTTPTERM"); modem.waitResponse(2000);
    modem.sendAT("+NETCLOSE"); modem.waitResponse(5000);
}

void shutdownModem() {
    digitalWrite(BOARD_PWRKEY_PIN, LOW); delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH); delay(3000);
    digitalWrite(BOARD_PWRKEY_PIN, LOW); delay(1000);
}

void audio_eof_mp3(const char *info) { audio.connecttoFS(SD, AUDIO_FILE_PATH); }
void audio_eof_speech(const char *info) { audio.connecttoFS(SD, AUDIO_FILE_PATH); }
void audio_info(const char *info) { Serial.print("Audio: "); Serial.println(info); }
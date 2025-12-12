/**
 * Music On Hold Device - Robust Download Implementation
 * - Downloads audio file via HTTPS with proper modem shutdown
 * - Checks for updates every 30 minutes
 * - Continuous audio playback via I2S
 * - Raw AT command handling to avoid library bugs
 */

#define TINY_GSM_RX_BUFFER 1024
//#define DUMP_AT_COMMANDS

#include "utilities.h"
#include <TinyGsmClient.h>
#include <SD.h>
#include <SPI.h>
#include "Audio.h"

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// I2S Pins (reassigned to avoid modem pin conflicts)
#define I2S_BCLK      21
#define I2S_LRC       22
#define I2S_DOUT      23

// Audio file settings
#define AUDIO_FILE_URL "https://messagesonhold.com.au/uploads/client-audio-wavs/ritz.mp3" 
#define AUDIO_FILE_PATH "/hold_mus.mp3"
#define DOWNLOAD_CHECK_INTERVAL_MS (30 * 60 * 1000) // 30 minutes

Audio audio;
bool fileReady = false;
unsigned long lastDownloadCheck = 0;

String apn = "telstra.wap";

// Forward declarations
void shutdownModem();
bool powerOnModem();
bool connectNetwork();
void disconnectNetwork();
bool downloadAudioFile();
void checkForNewAudio();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Music On Hold Device ===\n");

    // Power on peripheral (SD + Modem)
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
    delay(100);

    // Initialize SD card
    Serial.println("Initializing SD card...");
    SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN);
    if (!SD.begin(BOARD_SD_CS_PIN)) {
        Serial.println("SD card init FAILED");
        while(1) delay(1000);
    }
    Serial.println("SD card OK");

    // Initialize I2S audio
    Serial.println("Initializing I2S audio...");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(15); // 0...21
    Serial.println("I2S OK");

    // Check if file already exists
    if (SD.exists(AUDIO_FILE_PATH)) {
        Serial.println("Audio file found on SD card");
        fileReady = true;
    } else {
        Serial.println("No audio file found, downloading...");
        checkForNewAudio(); // Initial download attempt
    }

    // Start playback if file available
    if (fileReady) {
        Serial.println("Starting audio playback loop...");
        audio.connecttoFS(SD, AUDIO_FILE_PATH);
    } else {
        Serial.println("WARNING: No audio file available for playback");
    }

    lastDownloadCheck = millis();
}

void loop() {
    // Handle audio playback
    if (fileReady) {
        audio.loop();
    }

    // Check for new audio file every 30 minutes
    if (millis() - lastDownloadCheck >= DOWNLOAD_CHECK_INTERVAL_MS) {
        Serial.println("\n--- 30-minute check triggered ---");
        checkForNewAudio();
        lastDownloadCheck = millis();
    }
}

void checkForNewAudio() {
    bool wasPlaying = fileReady;
    
    // Power on and connect
    if (!powerOnModem()) {
        Serial.println("Modem init failed");
        shutdownModem();
        return;
    }

    if (!connectNetwork()) {
        Serial.println("Network connection failed");
        shutdownModem();
        return;
    }

    // Attempt download
    bool downloadSuccess = downloadAudioFile();

    // Always clean up
    disconnectNetwork();
    shutdownModem();

    if (downloadSuccess) {
        fileReady = true;
        
        // If this is a new/updated file and audio was playing, restart playback
        if (wasPlaying) {
            Serial.println("Restarting playback with new file...");
            audio.stopSong();
            delay(100);
        }
        audio.connecttoFS(SD, AUDIO_FILE_PATH);
    }
}

bool powerOnModem() {
    Serial.println("\n--- Powering On Modem ---");
    
    // Start at default modem baud rate
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    // Pull down DTR (disable sleep) - do this first
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
    
    // Setup power key pin
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    // Check if modem is already responsive
    Serial.print("Checking if modem already on");
    bool alreadyOn = false;
    for (int i = 0; i < 10; i++) {
        if (modem.testAT(1000)) {
            Serial.println(" YES");
            alreadyOn = true;
            break;
        }
        Serial.print(".");
        delay(500);
    }
    
    if (!alreadyOn) {
        Serial.println(" NO");
        
        // Modem not responding - try power on sequence
        Serial.print("Power on sequence");
        digitalWrite(BOARD_PWRKEY_PIN, HIGH);
        delay(MODEM_POWERON_PULSE_WIDTH_MS);
        digitalWrite(BOARD_PWRKEY_PIN, LOW);
        
        // Wait for modem to boot
        delay(3000);
        
        // Check if power-on worked
        int retry = 0;
        while (!modem.testAT(1000)) {
            Serial.print(".");
            if (retry++ > 3) {
                // Power-on didn't work, try hard reset
                Serial.println(" FAILED");
                Serial.print("Attempting hard reset");
                
                pinMode(MODEM_RESET_PIN, OUTPUT);
                digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); 
                delay(100);
                digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); 
                delay(2600);
                digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
                
                // Wait longer after hard reset
                delay(5000);
                
                // Final attempt to reach modem
                retry = 0;
                while (!modem.testAT(1000)) {
                    Serial.print(".");
                    if (retry++ > 10) {
                        Serial.println(" TIMEOUT");
                        return false;
                    }
                    delay(500);
                }
                break;
            }
            delay(500);
        }
        Serial.println(" OK");
    } else {
        // Modem was already on - do soft reset to ensure clean state
        Serial.print("Soft reset via AT command");
        modem.sendAT("+CFUN=1,1");  // Reset modem functionality
        modem.waitResponse(10000);
        
        // Wait for modem to restart
        delay(5000);
        
        // Wait for modem to be ready again
        int retry = 0;
        while (!modem.testAT(1000)) {
            Serial.print(".");
            if (retry++ > 20) {
                Serial.println(" TIMEOUT after soft reset");
                return false;
            }
            delay(500);
        }
        Serial.println(" OK");
    }

    // Switch modem to high speed
    Serial.print("Switching to 921600 baud");
    modem.sendAT("+IPR=921600");
    if (modem.waitResponse(3000) != 1) {
        Serial.println(" FAILED");
        return false;
    }
    delay(100);
    
    // Switch ESP32 serial to match
    SerialAT.updateBaudRate(921600);
    delay(100);
    
    // Verify communication at new baud rate
    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 5) {
            Serial.println(" FAILED");
            // Fall back to 115200
            SerialAT.updateBaudRate(115200);
            modem.sendAT("+IPR=115200");
            modem.waitResponse();
            Serial.println("Falling back to 115200 baud");
            return true; // Continue at lower speed rather than fail
        }
    }
    Serial.println(" OK");

    return true;
}

bool connectNetwork() {
    Serial.println("\n--- Connecting Network ---");

    // Wait for SIM card
    Serial.print("Checking SIM");
    int timeout = 30;
    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY && timeout-- > 0) {
        sim = modem.getSimStatus();
        if (sim == SIM_LOCKED) {
            Serial.println(" LOCKED");
            return false;
        }
        if (sim != SIM_READY) {
            Serial.print(".");
            delay(1000);
        }
    }
    if (sim != SIM_READY) {
        Serial.println(" TIMEOUT");
        return false;
    }
    Serial.println(" OK");

    // Register with network
    Serial.print("Registering");
    timeout = 60;
    RegStatus status = REG_NO_RESULT;
    while (timeout-- > 0) {
        status = modem.getRegistrationStatus();
        if (status == REG_OK_HOME || status == REG_OK_ROAMING) {
            break;
        }
        if (status == REG_DENIED) {
            Serial.println(" DENIED");
            return false;
        }
        Serial.print(".");
        delay(1000);
    }
    if (status != REG_OK_HOME && status != REG_OK_ROAMING) {
        Serial.println(" TIMEOUT");
        return false;
    }
    Serial.println(" OK");

    // Close any existing network connection first
    modem.sendAT("+NETCLOSE");
    modem.waitResponse(5000);

    // Activate network
    Serial.print("Opening network");
    int retry = 3;
    bool netOpen = false;
    while (retry-- > 0) {
        if (modem.setNetworkActive()) {
            netOpen = true;
            break;
        }
        Serial.print(".");
        delay(2000);
    }
    if (!netOpen) {
        Serial.println(" FAILED");
        return false;
    }
    Serial.println(" OK");

    // Verify IP address
    String ip = modem.getLocalIP();
    Serial.print("IP: ");
    Serial.println(ip);
    
    if (ip.length() == 0 || ip == "0.0.0.0") {
        Serial.println("Invalid IP address");
        return false;
    }

    return true;
}

void disconnectNetwork() {
    Serial.println("\n--- Disconnecting Network ---");
    
    // Terminate any HTTP session
    modem.sendAT("+HTTPTERM");
    modem.waitResponse(2000);
    
    // Close network
    modem.sendAT("+NETCLOSE");
    if (modem.waitResponse(5000) == 1) {
        Serial.println("Network closed");
    } else {
        Serial.println("Network close: no response (may already be closed)");
    }
}

void shutdownModem() {
    Serial.println("\n--- Shutting Down Modem ---");
    
    // Power off sequence
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(MODEM_POWEROFF_PULSE_WIDTH_MS);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    
    Serial.println("Modem powered off");
    delay(1000);
}

bool downloadAudioFile() {
    Serial.println("\n--- Downloading Audio File ---");
    Serial.print("URL: ");
    Serial.println(AUDIO_FILE_URL);

    // Initialize HTTP session
    if (!modem.https_begin()) {
        Serial.println("HTTP init failed");
        return false;
    }

    // Set URL with auto SSL version
    if (!modem.https_set_url(AUDIO_FILE_URL)) {
        Serial.println("Failed to set URL");
        modem.https_end();
        return false;
    }

    // Set realistic browser headers to avoid bot detection
    modem.https_set_user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    modem.https_add_header("Accept", "*/*");
    modem.https_add_header("Accept-Encoding", "identity");
    modem.https_add_header("Connection", "close");

    // Execute GET request
    size_t totalSize = 0;
    int httpCode = modem.https_get(&totalSize);
    if (httpCode != 200) {
        Serial.print("HTTP GET failed, code: ");
        Serial.println(httpCode);
        modem.https_end();
        return false;
    }

    Serial.print("Download successful, size: ");
    Serial.print(totalSize);
    Serial.println(" bytes");

    // Delete old file if exists
    if (SD.exists(AUDIO_FILE_PATH)) {
        SD.remove(AUDIO_FILE_PATH);
    }
    
    // Open file for writing
    File file = SD.open(AUDIO_FILE_PATH, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        modem.https_end();
        return false;
    }

    // Download in chunks using library function with safeguards
    const size_t CHUNK_SIZE = 1024;
    uint8_t buffer[CHUNK_SIZE];
    size_t totalRead = 0;
    
    Serial.print("Writing to SD: ");
    unsigned long startTime = millis();
    
    while (totalRead < totalSize) {
        // Read chunk from modem
        int bytesRead = modem.https_body(buffer, CHUNK_SIZE);
        
        if (bytesRead <= 0) {
            Serial.print("\nRead failed at offset ");
            Serial.println(totalRead);
            file.close();
            modem.https_end();
            return false;
        }
        
        // SAFEGUARD: Scan for AT response contamination
        // Look for patterns like "\r\n+HTTPREAD:" or "\nOK\r\n" in binary data
        int cleanStart = 0;
        int cleanEnd = bytesRead;
        
        // Check beginning of buffer for leftover AT responses
        for (int i = 0; i < bytesRead - 10 && i < 100; i++) {
            if ((buffer[i] == '\r' || buffer[i] == '\n') && buffer[i+1] == '+') {
                // Found potential AT response at start
                // Scan forward to find where actual data starts
                for (int j = i; j < bytesRead - 1; j++) {
                    if (buffer[j] == '\n') {
                        cleanStart = j + 1;
                        Serial.print("\n[Stripped ");
                        Serial.print(cleanStart);
                        Serial.print(" bytes of AT response from start]");
                        break;
                    }
                }
                break;
            }
        }
        
        // Check end of buffer for partial AT responses
        for (int i = bytesRead - 1; i > bytesRead - 50 && i > 0; i--) {
            if (buffer[i] == '+' && i > 0 && (buffer[i-1] == '\r' || buffer[i-1] == '\n')) {
                // Found potential AT response at end
                cleanEnd = i - 1;
                // Skip back over the newline
                while (cleanEnd > 0 && (buffer[cleanEnd] == '\r' || buffer[cleanEnd] == '\n')) {
                    cleanEnd--;
                }
                cleanEnd++; // Include last valid byte
                Serial.print("\n[Stripped ");
                Serial.print(bytesRead - cleanEnd);
                Serial.print(" bytes of AT response from end]");
                break;
            }
        }
        
        // Check middle of buffer for contamination (shouldn't happen, but be paranoid)
        for (int i = cleanStart + 10; i < cleanEnd - 10; i++) {
            if ((buffer[i] == '\r' || buffer[i] == '\n') && 
                buffer[i+1] == '+' && 
                buffer[i+2] == 'H' &&
                buffer[i+3] == 'T' &&
                buffer[i+4] == 'T' &&
                buffer[i+5] == 'P') {
                Serial.print("\nCRITICAL: AT contamination in middle at offset ");
                Serial.println(totalRead + i);
                file.close();
                modem.https_end();
                return false;
            }
        }
        
        // Write clean data to file
        int cleanLength = cleanEnd - cleanStart;
        if (cleanLength > 0) {
            if (file.write(buffer + cleanStart, cleanLength) != cleanLength) {
                Serial.println("\nSD write error");
                file.close();
                modem.https_end();
                return false;
            }
            totalRead += cleanLength;
        } else {
            // Entire buffer was contaminated?
            Serial.println("\nEntire chunk contaminated!");
            file.close();
            modem.https_end();
            return false;
        }
        
        // Progress indicator
        if (totalRead % 4096 == 0 || totalRead >= totalSize) {
            Serial.print(".");
        }
        
        // If we got less than requested, we're probably at end
        if (bytesRead < CHUNK_SIZE) {
            break;
        }
    }

    file.close();
    modem.https_end();

    unsigned long elapsed = (millis() - startTime) / 1000;
    if (elapsed == 0) elapsed = 1;
    float speedKBps = (float)totalRead / 1024.0 / (float)elapsed;

    Serial.println();
    Serial.print("File saved: ");
    Serial.print(AUDIO_FILE_PATH);
    Serial.print(" (");
    Serial.print(totalRead);
    Serial.print(" bytes in ");
    Serial.print(elapsed);
    Serial.print("s = ");
    Serial.print(speedKBps, 1);
    Serial.println(" KB/s)");
    
    // Verify we got all data
    if (totalRead < totalSize) {
        Serial.print("WARNING: Expected ");
        Serial.print(totalSize);
        Serial.print(" bytes but only wrote ");
        Serial.println(totalRead);
    }
    
    return true;
}

// Audio library callbacks
void audio_eof_mp3(const char *info) {
    audio.connecttoFS(SD, AUDIO_FILE_PATH);
}

void audio_eof_speech(const char *info) {
    audio.connecttoFS(SD, AUDIO_FILE_PATH);
}

void audio_info(const char *info) {
    Serial.print("Audio: ");
    Serial.println(info);
}
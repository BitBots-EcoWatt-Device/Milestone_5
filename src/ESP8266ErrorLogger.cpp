#include "ESP8266ErrorLogger.h"

const char* ESP8266ErrorLogger::LOG_FILE = "/error_log.txt";

ESP8266ErrorLogger errorLogger;

ESP8266ErrorLogger::ESP8266ErrorLogger() 
    : modbusExceptionCount_(0)
    , crcErrorCount_(0)
    , corruptResponseCount_(0)
    , packetDropCount_(0)
    , timeoutCount_(0)
{
}

bool ESP8266ErrorLogger::begin() {
    if (!LittleFS.begin()) {
        Serial.println("[ERROR_LOG] Failed to mount LittleFS");
        return false;
    }
    
    Serial.println("[ERROR_LOG] Error logging system initialized");
    return true;
}

String ESP8266ErrorLogger::getTimestamp() {
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu.%03lu]", 
             hours % 24, minutes % 60, seconds % 60, ms % 1000);
    return String(timestamp);
}

String ESP8266ErrorLogger::errorTypeToString(ErrorType type) {
    switch (type) {
        case ErrorType::MODBUS_EXCEPTION: return "MODBUS_EXCEPTION";
        case ErrorType::CRC_ERROR: return "CRC_ERROR";
        case ErrorType::CORRUPT_RESPONSE: return "CORRUPT_RESPONSE";
        case ErrorType::PACKET_DROP: return "PACKET_DROP";
        case ErrorType::TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

void ESP8266ErrorLogger::writeToFile(const String& logEntry) {
    File logFile = LittleFS.open(LOG_FILE, "a");
    if (!logFile) {
        Serial.println("[ERROR_LOG] Failed to open log file for writing");
        return;
    }
    
    logFile.println(logEntry);
    logFile.close();
    
    // Check if file is getting too large
    trimLogFile();
}

void ESP8266ErrorLogger::trimLogFile() {
    File logFile = LittleFS.open(LOG_FILE, "r");
    if (!logFile) {
        return;
    }
    
    size_t fileSize = logFile.size();
    logFile.close();
    
    if (fileSize > MAX_LOG_SIZE) {
        // Read last half of the file
        logFile = LittleFS.open(LOG_FILE, "r");
        if (!logFile) return;
        
        size_t keepSize = MAX_LOG_SIZE / 2;
        size_t skipSize = fileSize - keepSize;
        
        // Skip to midpoint
        logFile.seek(skipSize, SeekSet);
        
        // Read remaining data
        String tempData = logFile.readString();
        logFile.close();
        
        // Rewrite file with trimmed data
        logFile = LittleFS.open(LOG_FILE, "w");
        if (logFile) {
            logFile.print("[TRIMMED] Log file trimmed at ");
            logFile.println(getTimestamp());
            logFile.print(tempData);
            logFile.close();
        }
    }
}

void ESP8266ErrorLogger::logError(ErrorType type, const String& details) {
    String logEntry = getTimestamp() + " " + errorTypeToString(type) + ": " + details;
    writeToFile(logEntry);
    
    // Update counters
    switch (type) {
        case ErrorType::MODBUS_EXCEPTION: modbusExceptionCount_++; break;
        case ErrorType::CRC_ERROR: crcErrorCount_++; break;
        case ErrorType::CORRUPT_RESPONSE: corruptResponseCount_++; break;
        case ErrorType::PACKET_DROP: packetDropCount_++; break;
        case ErrorType::TIMEOUT: timeoutCount_++; break;
        default: break;
    }
}

void ESP8266ErrorLogger::logModbusException(uint8_t exceptionCode, const String& message) {
    char details[128];
    snprintf(details, sizeof(details), "Code 0x%02X - %s", exceptionCode, message.c_str());
    logError(ErrorType::MODBUS_EXCEPTION, String(details));
}

void ESP8266ErrorLogger::logCRCError(const String& context) {
    logError(ErrorType::CRC_ERROR, context);
}

void ESP8266ErrorLogger::logCorruptResponse(const String& details) {
    logError(ErrorType::CORRUPT_RESPONSE, details);
}

void ESP8266ErrorLogger::logPacketDrop(const String& details) {
    logError(ErrorType::PACKET_DROP, details);
}

void ESP8266ErrorLogger::logTimeout(const String& operation) {
    logError(ErrorType::TIMEOUT, "Timeout during: " + operation);
}

uint32_t ESP8266ErrorLogger::getErrorCount(ErrorType type) {
    switch (type) {
        case ErrorType::MODBUS_EXCEPTION: return modbusExceptionCount_;
        case ErrorType::CRC_ERROR: return crcErrorCount_;
        case ErrorType::CORRUPT_RESPONSE: return corruptResponseCount_;
        case ErrorType::PACKET_DROP: return packetDropCount_;
        case ErrorType::TIMEOUT: return timeoutCount_;
        default: return 0;
    }
}

uint32_t ESP8266ErrorLogger::getTotalErrorCount() {
    return modbusExceptionCount_ + crcErrorCount_ + corruptResponseCount_ + 
           packetDropCount_ + timeoutCount_;
}

void ESP8266ErrorLogger::printLog() {
    File logFile = LittleFS.open(LOG_FILE, "r");
    if (!logFile) {
        Serial.println("[ERROR_LOG] No error log file found");
        return;
    }
    
    Serial.println("\n========== ERROR LOG ==========");
    while (logFile.available()) {
        Serial.println(logFile.readStringUntil('\n'));
    }
    Serial.println("==============================\n");
    
    logFile.close();
}

void ESP8266ErrorLogger::printStats() {
    Serial.println("\n========== ERROR STATISTICS ==========");
    Serial.print("Modbus Exceptions:  "); Serial.println(modbusExceptionCount_);
    Serial.print("CRC Errors:         "); Serial.println(crcErrorCount_);
    Serial.print("Corrupt Responses:  "); Serial.println(corruptResponseCount_);
    Serial.print("Packet Drops:       "); Serial.println(packetDropCount_);
    Serial.print("Timeouts:           "); Serial.println(timeoutCount_);
    Serial.println("--------------------------------------");
    Serial.print("TOTAL ERRORS:       "); Serial.println(getTotalErrorCount());
    Serial.println("======================================\n");
}

void ESP8266ErrorLogger::clearOldLogs() {
    if (LittleFS.remove(LOG_FILE)) {
        Serial.println("[ERROR_LOG] Error log cleared");
        
        // Reset counters
        modbusExceptionCount_ = 0;
        crcErrorCount_ = 0;
        corruptResponseCount_ = 0;
        packetDropCount_ = 0;
        timeoutCount_ = 0;
    } else {
        Serial.println("[ERROR_LOG] Failed to clear error log");
    }
}

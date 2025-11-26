#ifndef ESP8266_ERROR_LOGGER_H
#define ESP8266_ERROR_LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>

// Error types for categorization
enum class ErrorType {
    MODBUS_EXCEPTION,
    CRC_ERROR,
    CORRUPT_RESPONSE,
    PACKET_DROP,
    TIMEOUT,
    UNKNOWN
};

class ESP8266ErrorLogger {
public:
    ESP8266ErrorLogger();
    
    bool begin();
    
    // Log an error with timestamp and details
    void logError(ErrorType type, const String& details);
    
    // Specific logging methods for each error type
    void logModbusException(uint8_t exceptionCode, const String& message);
    void logCRCError(const String& context);
    void logCorruptResponse(const String& details);
    void logPacketDrop(const String& details);
    void logTimeout(const String& operation);
    
    // Get error statistics
    uint32_t getErrorCount(ErrorType type);
    uint32_t getTotalErrorCount();
    
    // Print error log to Serial
    void printLog();
    void printStats();
    
    // Clear old logs (keep last N entries)
    void clearOldLogs();
    
private:
    static const char* LOG_FILE;
    static const size_t MAX_LOG_SIZE = 8192; // 8KB max log file
    
    // Error counters
    uint32_t modbusExceptionCount_;
    uint32_t crcErrorCount_;
    uint32_t corruptResponseCount_;
    uint32_t packetDropCount_;
    uint32_t timeoutCount_;
    
    String getTimestamp();
    String errorTypeToString(ErrorType type);
    void writeToFile(const String& logEntry);
    void trimLogFile();
};

// Global error logger instance
extern ESP8266ErrorLogger errorLogger;

#endif // ESP8266_ERROR_LOGGER_H

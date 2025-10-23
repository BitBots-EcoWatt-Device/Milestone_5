#ifndef ESP8266_POWER_MONITOR_H
#define ESP8266_POWER_MONITOR_H

#include <Arduino.h>

/**
 * Power monitoring and estimation utility for ESP8266
 * Tracks operational states and estimates power consumption
 */
class ESP8266PowerMonitor
{
public:
    ESP8266PowerMonitor();

    // State tracking
    void startOperation(const char* operationName);
    void endOperation();
    void recordSleep(unsigned long durationMs);
    void recordWiFiActivity(unsigned long durationMs);
    void recordClockScaling(uint8_t frequencyMhz, unsigned long durationMs);

    // Reporting
    void printReport();
    void printDetailedReport();
    float getAverageCurrent();
    float getEstimatedBatteryLife(float batteryCapacityMah);

    // Control
    void reset();
    void enable(bool enabled);
    bool isEnabled() { return enabled_; }

private:
    // Timing trackers
    unsigned long activeTimeMs_;
    unsigned long sleepTimeMs_;
    unsigned long wifiTxTimeMs_;
    unsigned long totalTimeMs_;
    unsigned long sessionStartMs_;
    
    // Clock scaling trackers
    unsigned long time80MhzMs_;   // Time spent at 80 MHz
    unsigned long time160MhzMs_;  // Time spent at 160 MHz
    
    // Operation tracking
    const char* currentOperation_;
    unsigned long operationStartMs_;
    
    // Current consumption constants (mA) - typical ESP8266 values
    static constexpr float CURRENT_ACTIVE_IDLE = 80.0;        // Active but idle (80 MHz)
    static constexpr float CURRENT_ACTIVE_80MHZ = 80.0;       // CPU at 80 MHz
    static constexpr float CURRENT_ACTIVE_160MHZ = 95.0;      // CPU at 160 MHz
    static constexpr float CURRENT_ACTIVE_PROCESSING = 90.0;  // CPU processing (average)
    static constexpr float CURRENT_SLEEP = 15.0;              // Light sleep
    static constexpr float CURRENT_WIFI_TX = 170.0;           // WiFi transmitting
    static constexpr float CURRENT_WIFI_RX = 100.0;           // WiFi receiving
    
    // Operation counters
    unsigned long pollCount_;
    unsigned long uploadCount_;
    unsigned long configRequestCount_;
    unsigned long clockScalingCount_;  // Number of frequency changes
    
    bool enabled_;
    
    void updateTotalTime();
};

extern ESP8266PowerMonitor powerMonitor;

#endif // ESP8266_POWER_MONITOR_H

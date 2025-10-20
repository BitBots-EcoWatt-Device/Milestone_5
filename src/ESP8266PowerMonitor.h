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
    
    // Operation tracking
    const char* currentOperation_;
    unsigned long operationStartMs_;
    
    // Current consumption constants (mA) - typical ESP8266 values
    static constexpr float CURRENT_ACTIVE_IDLE = 80.0;        // Active but idle
    static constexpr float CURRENT_ACTIVE_PROCESSING = 90.0;  // CPU processing
    static constexpr float CURRENT_SLEEP = 15.0;              // Light sleep
    static constexpr float CURRENT_WIFI_TX = 170.0;           // WiFi transmitting
    static constexpr float CURRENT_WIFI_RX = 100.0;           // WiFi receiving
    
    // Operation counters
    unsigned long pollCount_;
    unsigned long uploadCount_;
    unsigned long configRequestCount_;
    
    bool enabled_;
    
    void updateTotalTime();
};

extern ESP8266PowerMonitor powerMonitor;

#endif // ESP8266_POWER_MONITOR_H

#include "ESP8266PowerMonitor.h"

ESP8266PowerMonitor powerMonitor;

ESP8266PowerMonitor::ESP8266PowerMonitor()
    : activeTimeMs_(0),
      sleepTimeMs_(0),
      wifiTxTimeMs_(0),
      totalTimeMs_(0),
      sessionStartMs_(0),
      currentOperation_(nullptr),
      operationStartMs_(0),
      pollCount_(0),
      uploadCount_(0),
      configRequestCount_(0),
      enabled_(false)
{
}

void ESP8266PowerMonitor::enable(bool enabled)
{
    enabled_ = enabled;
    if (enabled)
    {
        reset();
        sessionStartMs_ = millis();
        Serial.println("[POWER] Power monitoring enabled");
    }
    else
    {
        Serial.println("[POWER] Power monitoring disabled");
    }
}

void ESP8266PowerMonitor::startOperation(const char *operationName)
{
    if (!enabled_)
        return;

    currentOperation_ = operationName;
    operationStartMs_ = millis();
}

void ESP8266PowerMonitor::endOperation()
{
    if (!enabled_ || currentOperation_ == nullptr)
        return;

    unsigned long duration = millis() - operationStartMs_;
    activeTimeMs_ += duration;

    // Track specific operations
    if (strcmp(currentOperation_, "poll") == 0)
    {
        pollCount_++;
    }
    else if (strcmp(currentOperation_, "upload") == 0)
    {
        uploadCount_++;
    }
    else if (strcmp(currentOperation_, "config") == 0)
    {
        configRequestCount_++;
    }

    Serial.print("[POWER] ");
    Serial.print(currentOperation_);
    Serial.print(" completed in ");
    Serial.print(duration);
    Serial.println(" ms");

    currentOperation_ = nullptr;
}

void ESP8266PowerMonitor::recordSleep(unsigned long durationMs)
{
    if (!enabled_)
        return;
    sleepTimeMs_ += durationMs;
}

void ESP8266PowerMonitor::recordWiFiActivity(unsigned long durationMs)
{
    if (!enabled_)
        return;
    wifiTxTimeMs_ += durationMs;
}

void ESP8266PowerMonitor::updateTotalTime()
{
    if (sessionStartMs_ > 0)
    {
        totalTimeMs_ = millis() - sessionStartMs_;
    }
}

float ESP8266PowerMonitor::getAverageCurrent()
{
    updateTotalTime();

    if (totalTimeMs_ == 0)
        return 0;

    // Calculate weighted average based on time spent in each state
    float totalCharge = (activeTimeMs_ * CURRENT_ACTIVE_PROCESSING +
                         sleepTimeMs_ * CURRENT_SLEEP +
                         wifiTxTimeMs_ * CURRENT_WIFI_TX);

    // Account for time not explicitly tracked (assume idle)
    unsigned long trackedTime = activeTimeMs_ + sleepTimeMs_ + wifiTxTimeMs_;
    unsigned long idleTime = (totalTimeMs_ > trackedTime) ? (totalTimeMs_ - trackedTime) : 0;
    totalCharge += (idleTime * CURRENT_ACTIVE_IDLE);

    return totalCharge / totalTimeMs_;
}

float ESP8266PowerMonitor::getEstimatedBatteryLife(float batteryCapacityMah)
{
    float avgCurrent = getAverageCurrent();
    if (avgCurrent == 0)
        return 0;

    return batteryCapacityMah / avgCurrent; // Hours
}

void ESP8266PowerMonitor::printReport()
{
    if (!enabled_)
    {
        Serial.println("[POWER] Power monitoring is disabled");
        return;
    }

    updateTotalTime();

    float avgCurrent = getAverageCurrent();
    float avgPower = avgCurrent * 3.3; // Assuming 3.3V operation

    Serial.println("\n========== POWER REPORT ==========");
    Serial.print("Session Duration: ");
    Serial.print(totalTimeMs_ / 1000.0, 2);
    Serial.println(" seconds");

    Serial.print("Average Current: ");
    Serial.print(avgCurrent, 2);
    Serial.println(" mA");

    Serial.print("Average Power: ");
    Serial.print(avgPower, 2);
    Serial.println(" mW");

    Serial.println("\n--- Battery Life Estimates ---");
    Serial.print("1000mAh battery: ");
    Serial.print(getEstimatedBatteryLife(1000), 1);
    Serial.println(" hours");

    Serial.print("2000mAh battery: ");
    Serial.print(getEstimatedBatteryLife(2000), 1);
    Serial.println(" hours");

    Serial.print("5000mAh battery: ");
    Serial.print(getEstimatedBatteryLife(5000), 1);
    Serial.println(" hours");

    Serial.println("==================================\n");
}

void ESP8266PowerMonitor::printDetailedReport()
{
    if (!enabled_)
    {
        Serial.println("[POWER] Power monitoring is disabled");
        return;
    }

    updateTotalTime();

    unsigned long trackedTime = activeTimeMs_ + sleepTimeMs_ + wifiTxTimeMs_;
    unsigned long idleTime = (totalTimeMs_ > trackedTime) ? (totalTimeMs_ - trackedTime) : 0;

    Serial.println("\n========== DETAILED POWER REPORT ==========");

    // Time breakdown
    Serial.println("--- Time Breakdown ---");
    Serial.print("Total Session Time: ");
    Serial.print(totalTimeMs_);
    Serial.println(" ms");

    Serial.print("Active Processing: ");
    Serial.print(activeTimeMs_);
    Serial.print(" ms (");
    Serial.print((activeTimeMs_ * 100.0) / totalTimeMs_, 1);
    Serial.println("%)");

    Serial.print("Sleep Time: ");
    Serial.print(sleepTimeMs_);
    Serial.print(" ms (");
    Serial.print((sleepTimeMs_ * 100.0) / totalTimeMs_, 1);
    Serial.println("%)");

    Serial.print("WiFi Activity: ");
    Serial.print(wifiTxTimeMs_);
    Serial.print(" ms (");
    Serial.print((wifiTxTimeMs_ * 100.0) / totalTimeMs_, 1);
    Serial.println("%)");

    Serial.print("Idle (untracked): ");
    Serial.print(idleTime);
    Serial.print(" ms (");
    Serial.print((idleTime * 100.0) / totalTimeMs_, 1);
    Serial.println("%)");

    // Operation counts
    Serial.println("\n--- Operation Counts ---");
    Serial.print("Sensor Polls: ");
    Serial.println(pollCount_);
    Serial.print("Data Uploads: ");
    Serial.println(uploadCount_);
    Serial.print("Config Requests: ");
    Serial.println(configRequestCount_);

    // Power calculations
    Serial.println("\n--- Power Consumption ---");
    float avgCurrent = getAverageCurrent();
    float avgPower = avgCurrent * 3.3;

    Serial.print("Average Current: ");
    Serial.print(avgCurrent, 2);
    Serial.println(" mA");

    Serial.print("Average Power: ");
    Serial.print(avgPower, 2);
    Serial.println(" mW");

    Serial.print("Energy Consumed: ");
    Serial.print((avgCurrent * totalTimeMs_) / 3600000.0, 4); // mAh
    Serial.println(" mAh");

    // Current breakdown by state
    Serial.println("\n--- Estimated Current by State ---");
    Serial.print("Processing: ");
    Serial.print(CURRENT_ACTIVE_PROCESSING, 1);
    Serial.println(" mA");

    Serial.print("Idle: ");
    Serial.print(CURRENT_ACTIVE_IDLE, 1);
    Serial.println(" mA");

    Serial.print("Sleep: ");
    Serial.print(CURRENT_SLEEP, 1);
    Serial.println(" mA");

    Serial.print("WiFi TX: ");
    Serial.print(CURRENT_WIFI_TX, 1);
    Serial.println(" mA");

    // Battery life estimates
    Serial.println("\n--- Battery Life Estimates ---");
    Serial.print("500mAh:  ");
    Serial.print(getEstimatedBatteryLife(500), 2);
    Serial.println(" hours");

    Serial.print("1000mAh: ");
    Serial.print(getEstimatedBatteryLife(1000), 2);
    Serial.println(" hours");

    Serial.print("2000mAh: ");
    Serial.print(getEstimatedBatteryLife(2000), 2);
    Serial.println(" hours");

    Serial.print("5000mAh: ");
    Serial.print(getEstimatedBatteryLife(5000), 2);
    Serial.println(" hours");

    Serial.println("===========================================\n");
}

void ESP8266PowerMonitor::reset()
{
    activeTimeMs_ = 0;
    sleepTimeMs_ = 0;
    wifiTxTimeMs_ = 0;
    totalTimeMs_ = 0;
    pollCount_ = 0;
    uploadCount_ = 0;
    configRequestCount_ = 0;
    sessionStartMs_ = millis();
    currentOperation_ = nullptr;
    operationStartMs_ = 0;

    Serial.println("[POWER] Power monitor reset");
}

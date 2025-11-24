# ESP8266 Firmware Configuration

This document explains how to configure your ESP8266 device for the BitBots EcoWatt monitoring system.

## Initial Setup

### 1. WiFi Configuration

The device needs to be configured with your WiFi credentials. You can do this in two ways:

#### Method A: Modify the defaults in code

Edit the `ESP8266Config.cpp` file and change these lines:

```cpp
strcpy(config_.wifi.ssid, "YourWiFiSSID");
strcpy(config_.wifi.password, "YourWiFiPassword");
```

#### Method B: Use Serial Configuration (Future Enhancement)

Connect via serial monitor and use configuration commands.

### 2. Server Configuration

Update the server endpoint in `ESP8266Config.cpp`:

```cpp
strcpy(config_.api.read_url, "http://YOUR_SERVER:8080/api/inverter/read");
strcpy(config_.api.write_url, "http://YOUR_SERVER:8080/api/inverter/write");
strcpy(config_.api.upload_url, "http://YOUR_SERVER:5000/upload");
strcpy(config_.api.config_url, "http://YOUR_SERVER:5000/config");
```

Also update the upload endpoint in `main.cpp`:

```cpp
httpClient.begin(wifiClient, "http://YOUR_SERVER:5000/upload");
```

And optionally set a dedicated configuration endpoint:

```cpp
strcpy(config_.api.config_url, "http://YOUR_SERVER:5000/config");
```

## Hardware Connections

### ESP8266 NodeMCU Pinout

The current firmware uses HTTP API communication, so no direct Modbus RTU connections are needed initially.

For future direct Modbus RTU connection:

- **TX Pin (GPIO1)**: Connect to Modbus RTU TX
- **RX Pin (GPIO3)**: Connect to Modbus RTU RX
- **GND**: Common ground
- **VCC**: 3.3V power supply

## Remote Configuration

The ESP8266 firmware implements a separate configuration update system that follows the defined Remote Configuration Message Format Specification.

### Configuration Request Cycle

The device automatically sends configuration requests to the cloud every 5 minutes using this format:

**Device → Cloud Request:**

```json
{
  "device_id": "EcoWatt001",
  "status": "ready"
}
```

### Configuration Updates

The cloud can respond with configuration updates using this format:

**Cloud → Device Response:**

```json
{
  "config_update": {
    "sampling_interval": 5000,
    "registers": ["voltage", "current", "frequency", "temperature", "power"]
  }
}
```

Supported register names:

- `voltage` - AC voltage measurement
- `current` - AC current measurement
- `frequency` - AC frequency measurement
- `temperature` - Inverter temperature
- `power` - Output power
- `pv1_voltage`, `pv2_voltage` - PV input voltages
- `pv1_current`, `pv2_current` - PV input currents
- `export_power_percent` - Export power percentage

### Configuration Acknowledgment

After processing the configuration update, the device sends an acknowledgment:

**Device → Cloud Acknowledgment:**

```json
{
  "config_ack": {
    "accepted": ["sampling_interval", "registers"],
    "rejected": [],
    "unchanged": []
  }
}
```

### Configuration Validation

- `sampling_interval`: Must be between 1000-60000 milliseconds
- `registers`: Must contain valid register names from the supported list
- Invalid parameters are rejected and listed in the acknowledgment
- Configuration changes are stored immediately but take effect only after the next successful upload cycle
- All changes are persisted to EEPROM to survive power cycles

### Configuration Application Timing

Configuration updates follow this sequence:

1. Device sends periodic configuration request
2. Cloud responds with configuration update (if any)
3. Device validates and stores the new configuration in EEPROM
4. Device sends acknowledgment back to cloud
5. **Configuration changes take effect after the next successful data upload**

This approach ensures:

- Data consistency (no mid-cycle parameter changes)
- Atomic configuration updates
- Reliable rollback if upload fails
- Clear state transitions

You can check for pending configuration updates using the `status` serial command.

### Manual Configuration Request

You can trigger a manual configuration request using the serial command:

```
config
```

## Command Execution

The ESP8266 firmware implements a command execution system that allows the EcoWatt Cloud to queue write commands for execution on the inverter.

### Command Execution Flow

1. **Cloud Queues Command** - EcoWatt Cloud queues a write command
2. **Device Receives Command** - At the next configuration request, the device receives the queued command
3. **Command Execution** - The device immediately executes the command on the Inverter SIM
4. **Result Reporting** - At the next upload cycle, the device reports the execution result back to EcoWatt Cloud

### Command Message Format

**Cloud → Device Command:**

```json
{
  "command": {
    "action": "write_register",
    "target_register": "export_power_percent",
    "value": 75
  }
}
```

**Device → Cloud Execution Result:**

```json
{
  "command_result": {
    "status": "success",
    "executed_at": "2025-10-10T14:12:00Z"
  }
}
```

Or in case of error:

```json
{
  "command_result": {
    "status": "error",
    "error_message": "Register 'invalid_reg' is not writable"
  }
}
```

### Supported Commands

- **Action**: `write_register` (only action currently supported)
- **Writable Registers**:
  - `export_power_percent` - Set export power percentage (0-100)

### Command Execution Timing

- Commands are received during configuration request cycles (every 5 minutes)
- Commands are executed immediately when received
- Execution results are included in the next data upload
- Results are cleared after successful upload

### Manual Command Testing

You can test command execution using the serial interface:

```
write export_power_percent 50
```

This will queue a test command for immediate execution.

## Serial Commands

Connect to the serial monitor at 115200 baud to use these commands:

- `status` - Show system status
- `restart` - Restart the device
- `test` - Run a test sensor poll
- `upload` - Trigger immediate data upload
- `config` - Request configuration update from cloud
- `write <register> <value>` - Test write command execution
- `wifi` - Show WiFi connection status
- `help` - Show available commands

## Monitoring

### LED Indicators

- **Built-in LED**: Blinks during WiFi connection attempts
- **Serial Monitor**: Shows detailed logging and status information

### Status Information

The device prints comprehensive status information including:

- WiFi connection status and IP address
- Available heap memory
- Data buffer usage
- Polling and upload intervals
- Sensor reading results

## Configuration Parameters

### Polling Configuration

Default enabled parameters:

- AC Voltage
- AC Current
- AC Frequency
- Temperature
- Output Power

### Timing Configuration

- **Poll Interval**: 5000ms (5 seconds)
- **Upload Interval**: 30000ms (30 seconds)
- **Buffer Size**: 10 samples
- **HTTP Timeout**: 5000ms

### Memory Considerations

The ESP8266 has limited RAM (~80KB available). The firmware is optimized for:

- Small data buffers (10 samples max)
- Minimal JSON payloads
- Efficient string handling
- Static memory allocation where possible

## Adding New Parameters

To add a new parameter to the system, follow these steps:

1. **Add to ParameterType enum** in `src/ESP8266DataTypes.h`:

   ```cpp
   enum class ParameterType : uint8_t {
       // ... existing parameters ...
       NEW_PARAMETER = 10  // Use next available number
   };
   ```

2. **Add to parameter descriptor table** in `src/ESP8266Parameters.cpp`:

   ```cpp
   const ParamDesc kParams[] PROGMEM = {
       // ... existing parameters ...
       {ParameterType::NEW_PARAMETER, PSTR("New Parameter"), PSTR("unit"), register_addr, scale_factor}
   };
   ```

3. **Include in polling configuration** by updating the parameters list in `main.cpp`:
   ```cpp
   std::vector<ParameterType> params = {
       // ... existing parameters ...
       ParameterType::NEW_PARAMETER
   };
   ```

That's it! The unified parameter system will automatically handle:

- Reading from the correct Modbus register with proper scaling
- Displaying the parameter with correct name and unit
- Including it in JSON serialization and data uploads

### Parameter Descriptor Table

The system uses a centralized parameter descriptor table that eliminates duplication between the polling configuration and inverter driver. Each parameter is defined once with:

- **ID**: Unique ParameterType enum value
- **Name**: Human-readable name (stored in flash memory)
- **Unit**: Measurement unit (stored in flash memory)
- **Register**: Modbus register address
- **Scale**: Division factor to convert raw register value to actual value

This approach provides:

- Single source of truth for parameter metadata
- No duplicated names, units, registers, or scales
- Easy addition/removal of parameters
- Optimized flash memory usage
- Backward compatibility with legacy API

## Troubleshooting

### Common Issues

1. **WiFi Connection Fails**

   - Check SSID and password
   - Ensure WiFi network is 2.4GHz (ESP8266 doesn't support 5GHz)
   - Check signal strength

2. **HTTP Requests Fail**

   - Verify server URL and port
   - Check firewall settings
   - Ensure server is accessible from ESP8266's network

3. **Memory Issues**

   - Reduce buffer size if out of memory errors occur
   - Monitor heap usage with `status` command

4. **Sensor Reading Fails**
   - Check Modbus server connectivity
   - Verify API endpoints are correct
   - Check API key if authentication is required

### Debug Information

Enable detailed logging by monitoring the serial output. All operations are logged with prefixes:

- `[WiFi]` - WiFi connection status
- `[HTTP]` - HTTP request/response details
- `[POLL]` - Sensor polling results
- `[BUFFER]` - Data buffer operations
- `[UPLOAD]` - Data upload operations
- `[CONFIG]` - Configuration loading/saving

## Building and Flashing

### Requirements

- PlatformIO or Arduino IDE
- ESP8266 board package
- Required libraries (see platformio.ini)

### Build Command

```bash
cd esp8266_firmware
pio run
```

### Flash Command

```bash
pio run --target upload
```

### Monitor Serial Output

```bash
pio device monitor
```

---

## Migration from Laptop to ESP8266

This project represents a complete migration from laptop-based firmware to ESP8266 embedded device. The migration involved significant architectural changes to accommodate the resource constraints and real-time requirements of embedded systems.

### Key Architectural Changes

#### 1. Threading to Event-Driven Architecture

**Original (Laptop):**
```cpp
std::thread pollT(pollLoop, ...);
std::thread upT(uploadLoop, ...);
```

**ESP8266:**
```cpp
Ticker pollTicker;
Ticker uploadTicker;
pollTicker.attach_ms(deviceConfig.poll_interval_ms, pollSensors);
uploadTicker.attach_ms(deviceConfig.upload_interval_ms, uploadData);
```

#### 2. HTTP Client Migration

**Original (Laptop):**
```cpp
#include <curl/curl.h>
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, url);
```

**ESP8266:**
```cpp
#include <ESP8266HTTPClient.h>
HTTPClient httpClient;
httpClient.begin(wifiClient, url);
httpClient.POST(payload);
```

#### 3. Configuration System

**Original:** File-based config.ini parsing

**ESP8266:** EEPROM-based configuration with serial command interface

```cpp
struct ESP8266Config {
    WiFiConfig wifi;
    APIConfig api;
    DeviceConfig device;
};
EEPROM.put(0, config_);
```

#### 4. Memory Management

**Original:** Large buffers and standard library containers

**ESP8266:** Optimized for limited memory
- Buffer size reduced from 30 to 10 samples
- Static arrays where possible
- Careful memory allocation

#### 5. JSON Processing

**Original:** Custom JSON string building

**ESP8266:** ArduinoJson library with memory-efficient document handling
```cpp
DynamicJsonDocument jsonDoc(4096);
JsonArray samplesArray = jsonDoc.createNestedArray("samples");
```

### Migration Status

#### ✅ Completed Features

- WiFi connection management with auto-reconnect
- HTTP-based Modbus communication
- Sensor data polling (AC voltage, current, frequency, temperature, power)
- JSON data upload to server
- Serial command interface
- EEPROM configuration storage
- Memory-optimized operation
- Watchdog functionality

#### 🔄 Partially Implemented

- Compression algorithms (can be ported if needed)
- Advanced packetization (simplified for ESP8266)
- Error recovery mechanisms

#### ⏳ Future Enhancements

- Direct Modbus RTU over serial
- Over-the-air (OTA) updates
- Web-based configuration interface
- Advanced data compression
- Multiple inverter support

### Performance Considerations

#### Memory Usage
- **Heap Usage**: ~40KB typical, 80KB maximum
- **Buffer Size**: Limited to 10 samples (vs 30 on laptop)
- **JSON Payload**: <4KB per upload

#### Timing
- **Poll Interval**: 5 seconds (configurable)
- **Upload Interval**: 30 seconds (configurable)
- **HTTP Timeout**: 5 seconds
- **WiFi Reconnect**: 15 seconds timeout

#### Power Consumption
- **Active**: ~200mA @ 3.3V (unoptimized)
- **Active (optimized)**: ~23-28mA average with power optimizations
- **Sleep Mode**: Implemented for battery operation

For complete migration details, see [MIGRATION.md](MIGRATION.md).

---

## Power Optimization Techniques

The ESP8266 firmware implements multiple power optimization techniques to achieve **73-75% power reduction** and extend battery life by **3.7× or more**.

### 1. WiFi Light Sleep Mode

**Implementation:**
```cpp
wifi_set_sleep_type(LIGHT_SLEEP_T);
```

**Benefits:**
- Reduces idle current from 80mA to 15mA
- **60-75% power reduction** during idle periods
- WiFi connection maintained automatically
- No impact on functionality

**How It Works:**
- ESP8266 enters light sleep during delays and idle periods
- WiFi modem powered down when not transmitting
- Automatic wake-up for Ticker callbacks and WiFi activity
- Seamless operation without code changes needed

### 2. Dynamic CPU Clock Scaling

**Strategy:**
- **Default**: 80MHz (lower power consumption ~80mA)
- **Burst Mode**: 160MHz for heavy processing (WiFi, JSON, compression)
- **Benefit**: Tasks complete 2× faster at 160MHz, spending less total time in active mode

**Implementation:**
```cpp
// Boost for heavy processing
system_update_cpu_freq(160);
// Heavy operations (JSON, WiFi, compression)
system_update_cpu_freq(80);  // Return to power-saving mode
```

**Power Savings:**
- Additional **5-10% power reduction** on top of light sleep
- Tasks at 160MHz complete in half the time
- Current increases only 19% (80mA → 95mA)
- Net result: 40.6% less energy per upload operation

**When Clock Scaling is Applied:**
- Data upload operations (JSON generation, compression, HTTP transmission)
- Configuration request processing
- Error log operations
- All other times: stays at 80MHz

### 3. Combined Optimization Results

| Technique | Current Reduction | Implementation Complexity |
|-----------|------------------|--------------------------|
| Light Sleep | 60-75% | Low |
| Dynamic Clock Scaling | 5-10% additional | Medium |
| Combined (Sleep + Clock) | **73-75%** | Medium |

### Power Consumption Comparison

| State | Before Optimization | After Optimization |
|-------|-------------------|-------------------|
| Average Current | 75-85 mA | 23-28 mA |
| Idle Current | 60-80 mA | 15-20 mA |
| Active Current | 150-200 mA | 95-170 mA |

### Battery Life Improvement

**1000mAh Battery:**
- Before: 85.87 mA → 11.6 hours
- After: 23.00 mA → 43.5 hours
- **Improvement: 3.7× longer**

**2000mAh Battery:**
- Before: 23.3 hours
- After: 87.0 hours  
- **Improvement: 3.7× longer**

### Serial Monitor Indicators

**Boot Messages:**
```
[POWER] CPU Clock: 80MHz (power-saving mode)
[POWER] Will boost to 160MHz during heavy processing
[POWER] Light Sleep Mode enabled (WIFI_LIGHT_SLEEP)
[POWER] Combined optimizations: 65-80% power reduction
```

**During Operation:**
```
[POWER] CPU boosted to 160MHz for upload
[UPLOAD] Starting data upload...
[UPLOAD] Upload successful
[POWER] CPU returned to 80MHz (power-saving mode)
```

### Technical Details

#### Current Consumption by State

| Mode | Clock | Current | Use Case |
|------|-------|---------|----------|
| Active | 80 MHz | ~80 mA | Sensor polling, idle |
| Active | 160 MHz | ~95 mA | WiFi, JSON, compression |
| Light Sleep | 80 MHz | ~15 mA | Waiting, delays |
| Deep Sleep | N/A | ~20 µA | Not used (loses state) |

#### Why This Approach Works

1. **CPU-intensive operations are intermittent** - Most time spent waiting/sleeping
2. **WiFi operations benefit from speed** - Faster transmission = less active time
3. **Light sleep maintains state** - No reconnection overhead
4. **Dynamic scaling is automatic** - No manual tuning required

For detailed implementation, see:
- [CLOCK_SCALING_IMPLEMENTATION.md](CLOCK_SCALING_IMPLEMENTATION.md)
- [POWER_OPTIMIZATION_IMPLEMENTATION.txt](POWER_OPTIMIZATION_IMPLEMENTATION.txt)

---

## Power Measurement Guide

The firmware includes a built-in **software-based power monitoring system** that estimates power consumption without requiring external measurement hardware.

### Quick Start

#### 1. Enable Power Monitoring
Open serial monitor (115200 baud) and type:
```
power-on
```

#### 2. Let it Run
Allow the device to operate normally for at least **10-30 minutes** to get accurate averages.

#### 3. View Results
Quick summary:
```
power-report
```

Detailed breakdown:
```
power-detailed
```

#### 4. Reset for New Test
```
power-reset
```

#### 5. Disable When Done
```
power-off
```

### Example Output

**Basic Report:**
```
========== POWER REPORT ==========
Session Duration: 300.00 seconds
Average Current: 78.45 mA
Average Power: 258.89 mW

--- Battery Life Estimates ---
1000mAh battery: 12.7 hours
2000mAh battery: 25.5 hours
5000mAh battery: 63.7 hours
==================================
```

**Detailed Report:**
```
========== DETAILED POWER REPORT ==========
--- Time Breakdown ---
Total Session Time: 300000 ms
Active Processing: 12450 ms (4.2%)
Sleep Time: 0 ms (0.0%)
WiFi Activity: 8920 ms (3.0%)
Idle (untracked): 278630 ms (92.8%)

--- Operation Counts ---
Sensor Polls: 60
Data Uploads: 20
Config Requests: 60

--- Power Consumption ---
Average Current: 78.45 mA
Average Power: 258.89 mW
Energy Consumed: 0.0065 mAh

--- Battery Life Estimates ---
500mAh:  6.37 hours
1000mAh: 12.75 hours
2000mAh: 25.49 hours
5000mAh: 63.73 hours
===========================================
```

### How Power Estimation Works

The power monitor tracks operational states and calculates weighted average current consumption:

| State | Typical Current |
|-------|----------------|
| Active Processing | 90 mA |
| Idle (no sleep) | 80 mA |
| Light Sleep | 15 mA |
| WiFi Transmit | 170 mA |
| WiFi Receive | 100 mA |

**Accuracy:** Software estimates are typically within ±10-20% of actual hardware measurements.

### Testing Protocol

#### Baseline Measurement (Before Optimization)

1. Flash unmodified firmware
2. Enable power monitoring: `power-on`
3. Run for 10+ minutes
4. Record results: `power-detailed`
5. Save output to file: `BASELINE_POWER_MEASUREMENT.txt`

#### Post-Optimization Measurement

1. Implement optimization (e.g., light sleep mode)
2. Flash optimized firmware
3. Enable power monitoring: `power-on`
4. Run for 10+ minutes (same conditions as baseline)
5. Record results: `power-detailed`
6. Save output to file: `OPTIMIZED_POWER_MEASUREMENT.txt`

#### Calculate Improvement

```
Power Savings (%) = ((Baseline - Optimized) / Baseline) × 100

Example:
Baseline:  78.45 mA
Optimized: 28.30 mA
Savings:   ((78.45 - 28.30) / 78.45) × 100 = 63.9%
```

### Power Monitoring Commands

| Command | Description |
|---------|-------------|
| `power-on` | Enable power monitoring |
| `power-off` | Disable power monitoring |
| `power-report` | Show quick summary |
| `power-detailed` | Show detailed breakdown |
| `power-reset` | Reset statistics |

### Tips for Accurate Measurement

1. **Consistent conditions:**
   - Same WiFi network and signal strength
   - Same server latency
   - Same polling/upload intervals
   - Same configuration settings

2. **Adequate measurement duration:**
   - Minimum: 10 minutes
   - Recommended: 30-60 minutes
   - Longer periods = more accurate averages

3. **Exclude outliers:**
   - First 1-2 minutes after boot (initialization)
   - Error/retry scenarios
   - Use `power-reset` to clear initial data

4. **Compare apples to apples:**
   - Same session duration for before/after
   - Similar number of polls and uploads
   - Verify no errors occurred during measurement

### What to Look For

#### Time Breakdown
- **Active Processing**: Time doing work (polling, uploading)
- **Sleep Time**: Time in low-power mode (target: maximize this!)
- **WiFi Activity**: Time transmitting data
- **Idle (untracked)**: Optimization opportunity (should decrease after light sleep)

#### Success Criteria for Light Sleep
- ✅ Idle time should show as "Sleep Time" instead of "Idle (untracked)"
- ✅ Average current should drop by 60-75%
- ✅ Battery life should increase 2.5-3.5×
- ✅ Device still polls and uploads normally

### Real-World Example

**Scenario: 2000mAh Battery**

**Before Optimization:**
- Average current: 80 mA
- Battery life: 2000mAh / 80mA = 25 hours

**After Light Sleep:**
- Average current: 28 mA  
- Battery life: 2000mAh / 28mA = 71 hours

**Result: 2.8× longer battery life!**

For complete measurement guide, see:
- [POWER_MEASUREMENT_GUIDE.md](POWER_MEASUREMENT_GUIDE.md)
- [QUICK_MEASUREMENT_GUIDE.md](QUICK_MEASUREMENT_GUIDE.md)

---

## Error Logging System

A comprehensive error logging system captures and logs all communication faults to persistent storage while maintaining all Serial Monitor output for debugging.

### Features

✅ **Dual Output**: All errors are both printed to Serial Monitor AND logged to file  
✅ **Persistent Storage**: Errors saved to LittleFS (`/error_log.txt`)  
✅ **Automatic Rotation**: Log file auto-trims when exceeding 8KB  
✅ **Statistics**: Real-time error counters by type  
✅ **Timestamped**: Each error includes precise timestamp  

### Error Types Logged

#### 1. Modbus Exceptions
**Triggered by**: Inverter returning exception codes
```
[MODBUS] Exception: Illegal Data Address
```
**Log Entry**:
```
[00:05:23.456] MODBUS_EXCEPTION: Code 0x02 - Illegal Data Address
```

#### 2. CRC Errors
**Triggered by**: CRC checksum mismatch in Modbus response
```
[MODBUS] CRC mismatch
```
**Log Entry**:
```
[00:05:23.789] CRC_ERROR: Read operation - Expected: 0x1A2B, Received: 0x1A2C
```

#### 3. Corrupt Responses
**Triggered by**: 
- Response too short
- Invalid byte count
- JSON parsing errors
- Missing fields

**Log Entries**:
```
[00:05:24.123] CORRUPT_RESPONSE: Response too short (size: 3)
[00:05:24.456] CORRUPT_RESPONSE: JSON parsing failed: InvalidInput
[00:05:24.789] CORRUPT_RESPONSE: HTTP response missing 'frame' field
```

#### 4. Packet Drops
**Triggered by**: Communication failures
```
[MODBUS] Failed to send read request
[HTTP] Error: connection refused
```
**Log Entries**:
```
[00:05:25.123] PACKET_DROP: Failed to send Modbus read request
[00:05:25.456] PACKET_DROP: HTTP error: connection refused
```

#### 5. Timeouts
**Triggered by**: HTTP request timeouts
```
[HTTP] Error: timeout
```
**Log Entry**:
```
[00:05:26.789] TIMEOUT: Timeout during: HTTP POST request
```

### Serial Commands

#### View Complete Error Log
```
error-log
```
Displays entire error log file contents.

**Example Output**:
```
========== ERROR LOG ==========
[00:05:23.456] MODBUS_EXCEPTION: Code 0x02 - Illegal Data Address
[00:05:23.789] CRC_ERROR: Read operation - Expected: 0x1A2B, Received: 0x1A2C
[00:05:24.123] CORRUPT_RESPONSE: Response too short (size: 3)
[00:05:25.123] PACKET_DROP: Failed to send Modbus read request
[00:05:26.789] TIMEOUT: Timeout during: HTTP POST request
==============================
```

#### View Error Statistics
```
error-stats
```
Shows counters for each error type.

**Example Output**:
```
========== ERROR STATISTICS ==========
Modbus Exceptions:  5
CRC Errors:         12
Corrupt Responses:  8
Packet Drops:       3
Timeouts:           2
--------------------------------------
TOTAL ERRORS:       30
======================================
```

#### Clear Error Log
```
error-clear
```
Deletes the error log file and resets all counters.

**Output**:
```
[ERROR_LOG] Error log cleared
```

### Log File Details

#### Location and Management
- **File**: `/error_log.txt`
- **Storage**: LittleFS filesystem
- **Max Size**: 8KB (auto-trims to 4KB when exceeded)
- **Format**: `[HH:MM:SS.mmm] ERROR_TYPE: Details`

#### Automatic Trimming
When the log file exceeds 8KB:
1. Keeps most recent 4KB of logs
2. Discards older entries
3. Adds a `[TRIMMED]` marker
4. Continues logging normally

### Testing with Fault Injection

#### 1. Start Device
```bash
pio run --target upload && pio device monitor
```

#### 2. Clear Previous Logs
```
error-clear
```

#### 3. Inject Faults from Simulator
Run your fault injection simulator with various fault types.

#### 4. Monitor Real-Time
Watch Serial Monitor for error messages (all still printed).

#### 5. View Results
After testing:
```
error-stats    # View summary
error-log      # View detailed log
```

### Expected Behavior

#### Normal Operation
```
[POLL] Starting sensor polling...
[POLL] AC Voltage: 230.50 V
[POLL] AC Current: 4.32 A
[BUFFER] Sample added, buffer size: 1
```

#### When Fault is Injected
```
[MODBUS] Exception: Illegal Data Address
[POLL] Failed to read AC Voltage
[POLL] Poll failed for some parameters
```
**AND simultaneously logged to file**:
```
[00:10:45.234] MODBUS_EXCEPTION: Code 0x02 - Illegal Data Address
```

### Integration Points

- **ESP8266ModbusHandler.cpp**: Logs Modbus exceptions, CRC errors, corrupt responses, packet drops
- **ESP8266ProtocolAdapter.cpp**: Logs JSON parsing errors, missing fields, HTTP errors, timeouts
- **main.cpp**: Provides error logger initialization and serial commands

### Benefits

1. **Debugging**: Comprehensive error history for analysis
2. **Testing**: Easy verification of fault injection scenarios
3. **Reliability**: Identify patterns in communication failures
4. **Documentation**: Timestamped evidence of system behavior
5. **Analysis**: Statistics help identify most common issues

### Performance Impact

- **Minimal**: Logging is asynchronous
- **Storage**: Max 8KB on flash
- **Processing**: <1ms per error log entry
- **Memory**: ~200 bytes RAM overhead


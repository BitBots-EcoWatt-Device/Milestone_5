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

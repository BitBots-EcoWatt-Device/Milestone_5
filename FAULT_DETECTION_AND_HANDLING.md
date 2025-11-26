# Fault Detection and Handling

## 1. Overview

The EcoWatt firmware detects and handles faults at multiple protocol layers to maintain reliable communication with the inverter and cloud server.

## 2. Fault Types Detected

### 2.1 Modbus Communication Faults

#### CRC Errors
- **What it is**: Checksum mismatch in Modbus response
- **Detection**: CRC-16 calculation compared against received value
- **Cause**: Electrical noise, wiring issues, RS-485 problems
- **How handled**: 
  - Error logged with expected vs. received CRC values
  - Operation fails and retries on next polling cycle
  - **Critical fix**: CRC validated *before* checking exceptions to prevent corrupted data being misread as exception 0x00

#### Modbus Exceptions
- **What it is**: Inverter returns error code instead of data
- **Detection**: Function code high bit set (0x80)
- **Common codes**:
  - `0x01` - Illegal Function (unsupported command)
  - `0x02` - Illegal Data Address (invalid register)
  - `0x03` - Illegal Data Value (out of range write)
  - `0x04` - Slave Device Failure (inverter hardware issue)
- **How handled**:
  - Exception code and message logged
  - Operation skipped for current cycle
  - Automatic retry next cycle

#### Corrupt Response Frames
- **What it is**: Incomplete or malformed Modbus response
- **Detection**: 
  - Response too short (< 5 bytes minimum)
  - Byte count doesn't match frame size
- **Cause**: Buffer overflows, incomplete transmission, protocol violations
- **How handled**:
  - Frame discarded with details logged
  - Retry on next cycle

### 2.2 Network Communication Faults

#### Packet Drops
- **What it is**: Failed to send/receive data over network
- **Detection**: HTTP/Modbus send operation returns failure
- **Cause**: WiFi disconnection, network congestion, server unavailable
- **How handled**:
  - Error logged with context
  - Exponential backoff retry (1s, 2s, 4s delays)
  - Maximum 3 attempts before giving up

#### Timeouts
- **What it is**: Operation exceeds time limit
- **Detection**: HTTP request timeout (configured: 10-30 seconds)
- **Cause**: Slow network, server overload, DNS issues
- **How handled**:
  - Timeout logged with operation name
  - Exponential backoff retry up to 3 attempts
  - If all attempts fail, operation abandoned until next cycle

### 2.3 Application Layer Faults

#### JSON Parse Failures
- **What it is**: Invalid JSON received from server
- **Detection**: ArduinoJson deserialization error
- **Cause**: Malformed JSON, incomplete response, encoding issues
- **How handled**:
  - Error logged with parse error message
  - Response discarded
  - Retry on next request

#### Missing Required Fields
- **What it is**: Expected data fields absent in JSON response
- **Detection**: Field presence check (e.g., missing "frame" field)
- **Cause**: Server bug, API version mismatch
- **How handled**:
  - Error logged specifying missing field
  - Response treated as invalid
  - Operation fails gracefully

## 3. Error Logging

All detected faults are logged to:
- **Serial Monitor**: Real-time debugging output
- **LittleFS file**: Persistent log (`/error_log.txt`) survives reboots
- **Statistics counters**: Track total count per error type

### Log Format
```
[HH:MM:SS.mmm] ERROR_TYPE: details
```

### Example Logs
```
[00:05:23.456] MODBUS_EXCEPTION: Code 0x02 - Illegal Data Address
[00:05:23.789] CRC_ERROR: Read operation - Expected: 0x1A2B, Received: 0x1A2C
[00:05:24.123] CORRUPT_RESPONSE: Response too short (size: 3)
[00:05:25.123] PACKET_DROP: Failed to send Modbus read request
[00:05:26.789] TIMEOUT: HTTP POST request
```

### Serial Commands
- `error-log` - View complete error log
- `error-stats` - Show error counts by type
- `error-clear` - Reset log and counters

## 4. Recovery Mechanisms

### Automatic Retry
- **Upload failures**: 3 attempts with exponential backoff (1s, 2s, 4s)
- **Config requests**: Same retry strategy
- **Polling errors**: Automatic retry next scheduled cycle

### Graceful Degradation
- **Single parameter failure**: Other parameters still read and uploaded
- **Upload failure**: Data buffered until next successful upload
- **Config fetch failure**: Continue with existing configuration
- **WiFi disconnect**: Automatic reconnection attempted

### Watchdog Protection
- **Timeout**: 60 seconds of no loop activity
- **Action**: Automatic system restart to recover from deadlock

## 5. Troubleshooting

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Frequent CRC errors | Wiring/interference | Check cables, grounding, RS-485 termination |
| Persistent timeouts | Network issues | Check WiFi signal, server availability |
| Modbus exception 0x02 | Wrong register | Verify register addresses match inverter |
| Packet drops | WiFi unstable | Improve signal or relocate device |

---

**For detailed implementation**: See `ERROR_LOGGING_OVERVIEW.md`  
**For testing procedures**: Use fault injection simulator with `error-clear` → inject → `error-stats`

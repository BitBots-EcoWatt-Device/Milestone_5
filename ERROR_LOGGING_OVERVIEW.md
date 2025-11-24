# ESP8266 Error Logging System

## 1. Why the Logger Exists
The EcoWatt firmware now captures every simulator-injected fault in a persistent, timestamped log while keeping all existing Serial Monitor output untouched. This provides:
- Real-time visibility (Serial) plus historical evidence (LittleFS `/error_log.txt`).
- Turnkey commands for viewing logs, statistics, and clearing history.
- Automatic maintenance—log rotation, counters, and watchdog-friendly writes.

## 2. Capabilities at a Glance
| Feature | Details |
| --- | --- |
| Fault Coverage | Modbus exceptions, CRC errors, corrupt responses, packet drops, timeouts |
| Destinations | Serial Monitor + LittleFS (`/error_log.txt`) |
| Persistence | Survives reboots, auto-trims from 8 KB → 4 KB when needed |
| Statistics | Live counters for every fault type (`error-stats`) |
| Commands | `error-log`, `error-stats`, `error-clear` |
| Performance | <1 ms per entry, ~200 B RAM, asynchronous writes |

## 3. What Gets Logged
Each entry uses `[HH:MM:SS.mmm] ERROR_TYPE: details` with timestamp since boot.

| Fault Type | Trigger Examples | Sample Entry |
| --- | --- | --- |
| **MODBUS_EXCEPTION** | Inverter returns illegal address/function | `[00:05:23.456] MODBUS_EXCEPTION: Code 0x02 - Illegal Data Address` |
| **CRC_ERROR** | Modbus checksum mismatch | `[00:05:23.789] CRC_ERROR: Read operation - Expected: 0x1A2B, Received: 0x1A2C` |
| **CORRUPT_RESPONSE** | Short frame, invalid byte count, JSON parse failure, missing fields | `[00:05:24.123] CORRUPT_RESPONSE: Response too short (size: 3)` |
| **PACKET_DROP** | Modbus send failure, HTTP connection refused | `[00:05:25.123] PACKET_DROP: Failed to send Modbus read request` |
| **TIMEOUT** | HTTP request exceeded deadline | `[00:05:26.789] TIMEOUT: Timeout during: HTTP POST request` |

## 4. Architecture & Key Files
- `ESP8266ErrorLogger.{h,cpp}` — Owns LittleFS file I/O, counters, rotation, and Serial command handlers.
- `ESP8266ModbusHandler.cpp` — Emits Modbus exceptions, CRC failures, corrupt frames, packet drops.
- `ESP8266ProtocolAdapter.cpp` — Captures HTTP/JSON issues, packet drops, and timeouts.
- `main.cpp` — Initializes the logger inside `initializeSystem()`, registers Serial commands, and enhances `help` text.

### Initialization Snippet
```cpp
if (!errorLogger.begin()) {
    Serial.println("[INIT] Warning: Error logger initialization failed");
}
```
If LittleFS fails to mount, the system warns but continues running.

## 5. Using the Logger
### Serial Commands
| Command | Purpose | Example Output |
| --- | --- | --- |
| `error-log` | Dump entire log file | `========== ERROR LOG ========== ...` |
| `error-stats` | Show counters per type plus total | `Modbus Exceptions: 5` etc. |
| `error-clear` | Delete `/error_log.txt` and reset counters | `[ERROR_LOG] Error log cleared` |

### Typical Test Session
1. **Upload & monitor**
   ```powershell
   pio run --target upload
   pio device monitor
   ```
2. **Reset history** — `error-clear`
3. **Inject faults** via simulator (CRC, timeouts, corrupt frames, etc.)
4. **Watch live Serial** messages (unchanged)
5. **Inspect results** — `error-stats`, then `error-log`

## 6. Log File Management
- **Location:** `/error_log.txt` (LittleFS)
- **Max size:** 8 KB; when exceeded, oldest entries trimmed to 4 KB and `[TRIMMED]` marker added.
- **Format:** One entry per line with millisecond-resolution timestamp.
- **Durability:** File persists across resets; manual clear via command when needed.

## 7. Expected Runtime Behavior
### Normal Polling
```
[POLL] Starting sensor polling...
[POLL] AC Voltage: 230.50 V
```

### During Fault Injection
```
[MODBUS] Exception: Illegal Data Address
[POLL] Poll failed for some parameters
```
Simultaneously, the persistent log receives the formatted entry, and counters increment.

## 8. Testing & Verification Checklist
1. Boot firmware and confirm boot message: `[ERROR_LOG] Error logging system initialized`.
2. Issue `error-clear` to ensure a clean slate.
3. Run simulator scenarios covering all fault classes.
4. Ensure each injected fault:
   - Appears on Serial exactly as before.
   - Adds a timestamped record to `/error_log.txt`.
   - Increments the appropriate statistic.
5. Verify auto-trimming by seeding >8 KB of logs (optional stress test).

## 9. Troubleshooting Tips
| Symptom | Likely Cause | Fix |
| --- | --- | --- |
| No log output | LittleFS not mounted | Re-upload filesystem or inspect `[INIT]` warnings |
| Counters not moving | Logger not initialized | Check `initializeSystem()` call order |
| `error-log` empty | Cleared recently or file trimmed | Re-run fault injection |
| Cannot open log | LittleFS corruption | `pio run --target uploadfs` |

## 10. Benefits Recap
- Full traceability of simulator-driven faults for lab reports and QA.
- Faster root-cause analysis thanks to categorized, timestamped history.
- Automatic log hygiene and negligible performance overhead.
- Drop-in Serial commands usable by QA, firmware, and support teams alike.

Keep this document alongside the simulator instructions so every test run captures consistent, auditable error data.

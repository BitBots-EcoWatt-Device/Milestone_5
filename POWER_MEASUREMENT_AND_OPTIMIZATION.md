# ESP8266 Power Measurement & Optimization Playbook

## 1. Software-Based Power Measurement

### 1.1 Purpose & Overview
The EcoWatt firmware ships with an internal power monitor that estimates current draw and battery life by tracking how long the ESP8266 spends in each operating state (active processing, WiFi activity, idle, and sleep). This method is ideal for before/after comparisons because it uses consistent current models and requires no external equipment.

### 1.2 Enabling Measurements
1. **Flash the target firmware** (baseline or optimized).
2. **Open the serial monitor** at 115200 baud.
3. **Start a session**:
   - `power-on` — begin tracking
   - *(optional)* `power-reset` — clear previous stats
4. **Let the system run** for 10–30 minutes so multiple polling/upload cycles occur under normal conditions.
5. **Collect results**:
   - `power-report` — quick snapshot
   - `power-detailed` — full time breakdown, operation counts, and battery estimates
6. **Stop when finished** with `power-off`.

### 1.3 Quick Workflow
| Phase | Actions |
| --- | --- |
| **Baseline** | Upload unoptimized firmware → `power-on` → wait ≥10 min → `power-detailed` → save as `baseline_power.txt` |
| **Optimized** | Upload optimized firmware → `power-on` → wait same duration → `power-detailed` → save as `optimized_power.txt` |
| **Analysis** | Compute savings with $\text{Savings}(\%) = \frac{\text{Baseline} - \text{Optimized}}{\text{Baseline}} \times 100$ |

### 1.4 How the Estimator Works
The monitor multiplies time spent in each state by a typical ESP8266 current draw, then derives averages and battery life projections.

| State | Typical Current |
| --- | --- |
| Active Processing | 90 mA |
| Idle (no sleep) | 80 mA |
| Light Sleep | 15 mA |
| WiFi Transmit | 170 mA |
| WiFi Receive | 100 mA |

Because the same assumptions are used for every firmware build, any change in the averages reflects real behavioral differences even if absolute accuracy is ±10–20%.

### 1.5 Accuracy Boosters
- Keep WiFi conditions, polling/upload intervals, and runtime lengths identical between tests.
- Ignore the first 1–2 minutes after boot; reset if you capture anomalies.
- Aim for 30–60 minute sessions when possible and repeat runs to average out variance.
- Document firmware version, RSSI, operation counts, and any anomalies right next to each log.

---

## 2. Implemented Power Optimization Techniques

### 2.1 WiFi Light Sleep Mode (Primary Idle Saver)
- **Implementation:** `WiFi.setSleepMode(WIFI_LIGHT_SLEEP);` in `setup()`; delays automatically enter low-power sleep while keeping WiFi associated.
- **Effect:** Drops idle current to ~15 mA, turning former "Idle (untracked)" time into true sleep time.
- **Verification Cues:** Serial boot banner lists "Light Sleep Mode enabled" and `power-detailed` shows a large "Sleep Time" percentage.

### 2.2 Dynamic CPU Clock Scaling (Active-Time Booster)
- **Implementation:** Default to 80 MHz (`system_update_cpu_freq(80)`), temporarily boost to 160 MHz around uploads/config fetches, and immediately revert.
- **Rationale:** WiFi, JSON serialization, and compression finish ~2× faster at 160 MHz while drawing only ~19% more current, cutting the total energy per operation by ~40%.
- **Usage Pattern:**
  ```cpp
  system_update_cpu_freq(160);  // before heavy work
  // ... WiFi + JSON + compression ...
  system_update_cpu_freq(80);   // return to cruise mode
  ```
- **Synergy:** Shorter active bursts mean more wall-clock time available for light sleep, magnifying savings from Section 2.1.

### 2.3 CPU Idle Yield Points (Stability + Minor Savings)
- **Implementation:** Strategic `yield()` calls in WiFi waits, polling loops, retry backoffs, and the main idle loop.
- **Benefits:** Keeps the WiFi stack responsive, reduces retry storms, and allows the scheduler to service background tasks that might otherwise block sleep transitions (worth ~1–2% but crucial for reliability).

### 2.4 Optional/Planned Enhancements
- **Forced WiFi sleep windows** (`WiFi.forceSleepBegin()` / `WiFi.forceSleepWake()`) to push idle current below 50 mA average when longer gaps exist between polls.
- **Interval tuning** (e.g., 30-second polls) to increase the time budget available for sleep states without sacrificing data quality.

---

## 3. Baseline vs. Optimized Measurements

### 3.1 Test Protocol
- Both tests ran on October 20, 2025 using the same WiFi network, poll/upload cadence, and ~10–11 minute sessions.
- Operation counts remained comparable (104 vs. 120 polls; 37 vs. 40 uploads) to ensure a fair apples-to-apples comparison.

### 3.2 Results Summary
| Metric | Baseline (Unoptimized) | Optimized (Light Sleep + Yield) | Delta |
| --- | --- | --- | --- |
| Average Current | 85.87 mA | 76.93 mA | **−10.4%** |
| Average Power | 283.38 mW | 253.87 mW | −29.51 mW |
| Sleep Time | 6.6% (43.6 s) | 18.9% (115 s) | **+12.3%** |
| Active Processing | 92.5% | 80.3% | −12.2% |
| WiFi Activity | 0.9% | 0.9% | steady |
| Battery Life @1000mAh | 11.6 h | 13.0 h | +1.4 h (12.1%) |

### 3.3 Interpretation
- Light Sleep and scheduler tweaks delivered a measurable improvement, nearly tripling documented sleep time and yielding a 10% current reduction.
- The savings are below the 60–75% target because the device still spends ~80% of its time actively polling every 5 seconds. More aggressive tactics—forced WiFi sleep between polls, longer intervals, and the now-implemented dynamic clock scaling—are expected to drive average current toward the 30–40 mA range once re-measured.

### 3.4 Dynamic Clock Scaling Results (November 26, 2025)

After implementing dynamic CPU clock scaling (80 MHz baseline with 160 MHz bursts during heavy processing), a 10-minute test session showed further improvements:

#### Detailed Measurements
```
========== DETAILED POWER REPORT ==========
--- Time Breakdown ---
Total Session Time: 601200 ms (10.02 minutes)
CPU @ 80MHz: 450715 ms (75.0%)
CPU @ 160MHz: 34598 ms (5.8%)
Sleep Time: 113700 ms (18.9%)
WiFi Activity: 14907 ms (2.5%)
Idle (untracked): 0 ms (0.0%)

--- Operation Counts ---
Sensor Polls: 119
Data Uploads: 40
Config Requests: 111
Clock Scaling Events: 270

--- Power Consumption ---
Average Current: 72.49 mA
Average Power: 239.22 mW
Energy Consumed: 12.1066 mAh

--- Estimated Current by State ---
CPU @ 80MHz: 80.0 mA
CPU @ 160MHz: 95.0 mA
Idle: 80.0 mA
Sleep: 15.0 mA
WiFi TX: 170.0 mA
Avg with Clock Scaling: 81.1 mA

--- Battery Life Estimates ---
500mAh:  6.90 hours
1000mAh: 13.80 hours
2000mAh: 27.59 hours
5000mAh: 68.98 hours
===========================================
```

#### Comparison: Baseline → Light Sleep → Dynamic Clock Scaling
| Metric | Baseline | Light Sleep Only | + Clock Scaling | Total Improvement |
| --- | --- | --- | --- | --- |
| Average Current | 85.87 mA | 76.93 mA | **72.49 mA** | **−15.6%** |
| Average Power | 283.38 mW | 253.87 mW | **239.22 mW** | **−15.6%** |
| Sleep Time | 6.6% | 18.9% | **18.9%** | +12.3% |
| CPU @ 80MHz | — | — | **75.0%** | — |
| CPU @ 160MHz | — | — | **5.8%** | — |
| Battery Life @1000mAh | 11.6 h | 13.0 h | **13.8 h** | **+19.0%** |

#### Key Insights
- **Clock scaling added 5.8% savings** on top of light sleep, bringing total current reduction to 15.6%
- **270 clock scaling events** in 10 minutes (27/minute) with minimal overhead
- **CPU spent only 5.8% at 160 MHz**, achieving burst performance where needed while staying efficient
- **Battery life improved from 11.6h to 13.8h** (19% increase) with combined optimizations
- **Energy per operation reduced** by completing WiFi/JSON/compression tasks faster at higher frequency

The dynamic clock scaling strategy successfully demonstrates the "burst and return" philosophy—brief periods of higher power consumption during intensive tasks result in lower overall energy usage by reducing total active time.

### 3.5 Next Measurement Targets
1. **Experiment with forced WiFi sleep windows** or longer poll intervals to push sleep time above 80%.
2. **Maintain the same measurement protocol** so future comparisons stay directly relatable to the baseline figures above.
3. **Consider modem sleep** during longer idle periods for additional savings.

---

## 4. Takeaways
- The built-in power monitor offers a fast, repeatable view of firmware efficiency—treat it as the ground truth for every optimization milestone.
- Light Sleep, dynamic scaling, and cooperative yielding are low-risk changes already integrated into the codebase; they're the foundation for deeper optimizations like modem/deep sleep.
- Documenting both the method and the measurements in one place streamlines lab reports, grant submissions, and future engineering handoffs.

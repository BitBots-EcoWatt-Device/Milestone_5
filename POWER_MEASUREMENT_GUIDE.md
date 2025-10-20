# Power Measurement Guide

## Overview
This guide explains how to measure power consumption before and after implementing optimization techniques on the ESP8266 EcoWatt device using the built-in software-based power monitoring system.

---

## Software-Based Power Monitoring

### Setup
The firmware now includes a built-in power monitor that estimates power consumption based on operational states.

### Usage

#### 1. **Enable Power Monitoring**
Open serial monitor (115200 baud) and type:
```
power-on
```

#### 2. **Let it Run**
Allow the device to operate normally for at least 5-10 minutes to get accurate averages:
- It will track sensor polling
- It will track data uploads
- It will track WiFi activity
- It will track idle/sleep time

#### 3. **View Report**
Quick summary:
```
power-report
```

Detailed breakdown:
```
power-detailed
```

#### 4. **Reset for New Test**
To start fresh measurements:
```
power-reset
```

#### 5. **Disable When Done**
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

---

## How Power Estimation Works

The power monitor uses typical ESP8266 current consumption values for different operational states:

| State | Typical Current |
|-------|----------------|
| Active Processing | 90 mA |
| Idle (no sleep) | 80 mA |
| Light Sleep | 15 mA |
| WiFi Transmit | 170 mA |
| WiFi Receive | 100 mA |

By tracking how long the device spends in each state, the system calculates a weighted average current consumption and estimates battery life.

### Accuracy
- Software estimates are typically within ±10-20% of actual values
- Accuracy improves with longer measurement periods (30+ minutes recommended)
- Results are consistent for comparing before/after optimization

---

## Testing Protocol

### Baseline Measurement (BEFORE Optimization)

1. **Flash unmodified firmware**
2. **Enable power monitoring:**
   ```
   power-on
   ```
3. **Run for 10 minutes minimum**
4. **Record results:**
   ```
   power-detailed
   ```
5. **Save output to file:** `baseline_power.txt`

### Post-Optimization Measurement

1. **Implement light sleep mode** (or other optimization)
2. **Flash optimized firmware**
3. **Enable power monitoring:**
   ```
   power-on
   ```
4. **Run for 10 minutes minimum** (same conditions as baseline)
5. **Record results:**
   ```
   power-detailed
   ```
6. **Save output to file:** `optimized_power.txt`

### Comparison

Calculate improvement:
```
Power Savings (%) = ((Baseline - Optimized) / Baseline) × 100
```

Example:
```
Baseline:  78.45 mA
Optimized: 28.30 mA
Savings:   ((78.45 - 28.30) / 78.45) × 100 = 63.9%
```

---

## Expected Results by Technique

| Technique | Current Reduction | Implementation Complexity |
|-----------|------------------|--------------------------|
| Light Sleep | 60-75% | Low |
| Dynamic Clock Scaling | 10-15% | Medium |
| Peripheral Gating (WiFi) | 30-40% | High |
| Combined (Sleep + Clock) | 65-80% | Medium |

---

## Tips for Accurate Measurement

1. **Consistent conditions:**
   - Same WiFi network
   - Same server distance/latency
   - Same polling intervals
   - Same configuration settings

2. **Measure for adequate time:**
   - Minimum 10 minutes
   - Better: 30-60 minutes
   - Include multiple poll/upload cycles
   - Longer periods = more accurate averages

3. **Exclude outliers:**
   - First 1-2 minutes after boot (initialization)
   - Any error/retry scenarios
   - Use `power-reset` to clear initial data

4. **Document everything:**
   - Firmware version
   - Configuration settings (poll interval, upload interval)
   - WiFi RSSI (signal strength)
   - Number of operations completed
   - Session duration

5. **Repeat measurements:**
   - Take 2-3 measurements for each configuration
   - Calculate average if results vary
   - Check operation counts are similar between runs

6. **Compare apples to apples:**
   - Same session duration for before/after
   - Similar number of polls and uploads
   - Verify no errors occurred during measurement

---

## Serial Commands Quick Reference

| Command | Description |
|---------|-------------|
| `power-on` | Enable power monitoring |
| `power-off` | Disable power monitoring |
| `power-report` | Show quick summary |
| `power-detailed` | Show detailed breakdown |
| `power-reset` | Reset statistics |
| `status` | Show system status |
| `help` | Show all commands |

---

## Real-World Example

### Scenario: 2000mAh Battery

**Before Optimization:**
- Average current: 80 mA
- Battery life: 2000mAh / 80mA = 25 hours

**After Light Sleep:**
- Average current: 28 mA  
- Battery life: 2000mAh / 28mA = 71 hours

**Result:** 2.8× longer battery life!

---

## Next Steps

1. ✅ Measure baseline (current firmware)
2. ✅ Implement light sleep mode
3. ✅ Measure optimized version
4. ✅ Document improvements
5. ✅ Consider additional optimizations if needed

---

## Troubleshooting

**Q: Power monitor shows 0 mA or no data**
- A: Power monitoring might be disabled. Type `power-on`
- A: You may not have waited long enough. Wait at least 5 minutes before checking

**Q: "Idle (untracked)" time is very high**
- A: This is normal BEFORE optimization - this is the time you want to reduce!
- A: After implementing light sleep, this should move to "Sleep Time"

**Q: Numbers seem inconsistent between runs**
- A: Run for longer periods (30+ minutes) for stable averages
- A: Check that operation counts are similar between measurements
- A: Ensure WiFi conditions are consistent (check RSSI)

**Q: All time shows as "Active Processing"**
- A: The delay(100) calls should be tracked as sleep/idle time
- A: Check that power monitoring was enabled before operations started

**Q: How long should I measure?**
- A: Minimum 10 minutes for quick checks
- A: 30-60 minutes recommended for accurate comparisons
- A: Longer is better - system calculates running average

**Q: Battery life estimate seems too short/long**
- A: Estimates are based on typical ESP8266 values
- A: Actual battery life depends on battery quality, temperature, and age
- A: Use estimates for relative comparison (before vs after)

**Q: Want to measure just one operation (e.g., polling)**
- A: Use `power-reset` to clear statistics
- A: Manually trigger the operation with `test`, `upload`, or `config` commands
- A: View results with `power-detailed`

---

## Important Notes

### About Software Estimates
- Based on typical ESP8266 current consumption values from datasheet
- Accuracy: ±10-20% compared to actual hardware measurements
- Best used for **relative comparison** (before vs after optimization)
- Longer measurement periods provide more accurate averages

### Factors Affecting Accuracy
- WiFi signal strength (RSSI) - weaker signal = higher power
- Network latency - longer uploads = more WiFi active time
- Temperature - hotter conditions slightly increase current
- Flash memory wear - older devices may consume slightly more power

### When Software Estimation Works Best
- ✅ Comparing different firmware versions
- ✅ Testing optimization techniques
- ✅ Identifying which operations consume most power
- ✅ Estimating battery life for different usage patterns
- ✅ Academic/educational projects

### Limitations
- Cannot detect hardware issues (shorts, damaged components)
- Cannot measure instantaneous current spikes
- Assumes typical ESP8266 current values (your module may vary slightly)
- Does not account for external peripherals or sensors

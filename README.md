# Noise Traffic Light 🚦

A real-time noise monitor that displays sound levels as a traffic light using RGB LEDs.

## 📋 Features

- **Real-time noise detection** via I2S MEMS microphone
- **3-color traffic light display**:
  - 🟢 **GREEN** – Noise is low (quieter than threshold)
  - 🟡 **YELLOW** – Noise is moderate (within range)
  - 🔴 **RED** – Noise is high (exceeded threshold)
- **12 NeoPixel RGB LED strip** for bright, visible feedback
- **A-weighted sound level measurement** (dBA) for realistic perception
- **Configurable thresholds** via persistent storage
- **Designed for classroom environments** (student noise management)

## 🔧 Hardware Setup

### Pin Configuration

| Component | Pin | Color | Description |
|-----------|-----|-------|-------------|
| **WS2812 LED Data** | GPIO 2 | - | NeoPixel strip (12 LEDs) |
| **I2S L/R Select** | GPIO 4 | Green | Microphone channel select (set HIGH = RIGHT channel) |
| **I2S Word Select** | GPIO 5 | Blue | Microphone L/R clock |
| **I2S Serial Clock** | GPIO 6 | White | Microphone bit clock |
| **I2S Serial Data** | GPIO 7 | Yellow | Microphone audio data |
| **Power (5V)** | 5V | - | USB power supply |
| **GND** | GND | - | Ground |

### Wiring Diagram

```
ESP32                          I2S Microphone (e.g., INMP441)
───────                        ──────────────────
GPIO 4 (green, HIGH)───────────→ L/R (channel select = RIGHT)
GPIO 5 (blue) ─────────────────→ WS (L/R Clock)
GPIO 6 (white) ─────────────────→ SCK (Bit Clock)
GPIO 7 (yellow)─────────────────→ SD (Serial Data)
GND ───────────────────────────→ GND
3V3 ───────────────────────────→ VDD (if 3.3V version)
```

```
ESP32                          NeoPixel Strip (WS2812)
───────                        ──────────────────
GPIO 2 ────────────────────────→ DI (Data In)
5V ────────────────────────────→ 5V
GND ───────────────────────────→ GND
```

## 📊 How It Works

### Audio Processing Pipeline

1. **I2S Sampling** (48 kHz, 32-bit)
   - Captures continuous audio from the microphone
   - High-precision digital sampling

2. **IIR Filtering**
   - **Equalizer filter** (INMP441): Flattens microphone frequency response
   - **A-weighting filter**: Mimics human ear sensitivity (emphasizes mid-frequencies)

3. **Sound Level Calculation**
   - Converts filtered samples to dB (decibels)
   - Uses microphone calibration: 94 dB SPL reference
   - LEQ (Equivalent Continuous Sound Level) over 0.15 second blocks

4. **Decision Logic**
   ```
   if (Leq < dB_min)     → GREEN   (quiet)
   if (dB_min ≤ Leq < dB_max) → YELLOW  (moderate)
   if (Leq ≥ dB_max)     → RED     (loud)
   ```

## 🎚️ Configuration

### Default Thresholds

```cpp
dB_min_default = 40   // Below 40 dB → GREEN
dB_max_default = 60   // Above 60 dB → RED
```

### Recommended Settings

| Environment | dB_min | dB_max | Notes |
|------------|--------|--------|-------|
| Library/Silent | 30 | 50 | Very quiet spaces |
| Classroom | 40 | 60 | Normal teaching |
| Active classroom | 45 | 70 | Group work sessions |
| Workshop | 60 | 80 | Tolerate machinery |

## 🚀 Building & Uploading

### Prerequisites

- PlatformIO (VS Code extension or CLI)
- Arduino IDE (optional, for direct compilation)

### Build

```bash
# Using PlatformIO
platformio run -e esp32-s3-devkitc1-n4r2

# Or in VS Code: Ctrl+Shift+B → Build
```

### Upload

```bash
# Using PlatformIO
platformio run -t upload -e esp32-s3-devkitc1-n4r2

# Or in VS Code: Ctrl+Shift+B → Upload
```

## 📈 Serial Output

The device outputs dB measurements to the serial monitor (115200 baud):

```
Leq: 45.3 dB | Min: 40 | Max: 60 | GREEN
Leq: 62.1 dB | Min: 40 | Max: 60 | RED
Leq: 55.2 dB | Min: 40 | Max: 60 | YELLOW
```

## 🔧 Calibration

### Microphone Sensitivity

If readings are consistently off:

1. **Measure known sound level** (e.g., using phone app)
2. **Compare with serial output**
3. **Adjust MIC_OFFSET_DB** in `main.cpp`:
   ```cpp
   #define MIC_OFFSET_DB 3.0103  // Increase/decrease calibration offset
   ```

### Optimal Setup

- Mount microphone in **center of room**
- **Avoid** placing near walls or corners (reflections)
- **Keep away** from vibration sources
- Ensure **clear air path** (not covered)

## 🔴 Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Always GREEN | Microphone not connected | Check I2S pins (GPIO 4,6,7) |
| Always RED | Calibration off | Adjust MIC_OFFSET_DB |
| No serial output | Wrong baud rate | Set to 115200 |
| No LED response | Wrong LED pin | Verify GPIO 2 connection |
| Microphone noise floor high | Electrical noise | Shield data lines, check power supply |

## 📚 References

- **Original deciLight Project**: https://github.com/bbbenji/deciLight
- **FastLED Documentation**: http://fastled.io/
- **ESP-IDF I2S API**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html
- **A-weighting Filter**: https://en.wikipedia.org/wiki/A-weighting

## 📝 License

This project is adapted from [deciLight](https://github.com/bbbenji/deciLight) (GPL-3.0).
Licensed under GNU General Public License v3.0.

## 🎨 Future Enhancements

- [ ] IR remote control for threshold adjustment
- [ ] MQTT integration for remote monitoring
- [ ] WiFi display (web dashboard)
- [ ] Multiple noise zones (classroom network)
- [ ] Adjustable color mapping
- [ ] Sound event logging

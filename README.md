# Edge Weather Station (Pi Zero W) - C++ Edition

A lightweight, high-performance C++ sensor daemon running on a Raspberry Pi Zero W. It reads environmental telemetry (temperature, humidity, and atmospheric pressure) directly from a **BME280** sensor and serves a clean JSON HTTP endpoint on port `8080` for the central hub (`RainbowPi`) to scrape.

---

## 🏗️ Architecture & Stack

- **Language:** C++ (low-overhead, zero-dependency socket/HTTP handler).
- **Hardware:** Raspberry Pi Zero W + BME280 Sensor (I2C interface).
- **Networking:** Serves telemetry JSON on port `8080`, secured via UFW exclusively for the RPi 5 hub (`192.168.1.177`).

---

## 🚀 Building & Running

### 1. Clone the Repository

```bash
git clone https://github.com/AccessiT3ch/weather-station.git
cd weather-station
```

### 2. Compile the C++ Daemon

```bash
mkdir -p build && cd build
cmake .. # or make, depending on your build setup
make
cd ..
```

### 3. Test Local Telemetry

```bash
./test_sensor.sh
```



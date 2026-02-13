# Mini LiDAR System – Raspberry Pi 2

This project implements a simple **rotating LiDAR system** using a Raspberry Pi 2, a VL53L1X Time-of-Flight sensor, and a 28BYJ-48 stepper motor.
The system performs angular distance scanning and visualizes measurements in real time on a PC.

---

# System Overview

The project consists of **three main programs**:

| Program          | Language | Purpose                                                                                           |
| ---------------- | -------- | ------------------------------------------------------------------------------------------------- |
| `stepper_node.c` | C        | Controls the stepper motor, communicates with the sensor via UDP, and publishes data using ZeroMQ |
| `sensor_node.py` | Python   | Reads distance data from the VL53L1X sensor and responds via UDP                                  |
| `plot_node.py`   | Python   | Visualizes measurements in real time using polar plots                                            |

> **Note:** `sensor_node.py` uses the Python VL53L1X driver, which must be installed before running the system: [VL53L1X Python Driver](https://github.com/pimoroni/vl53l1x-python.git
---

# System Workflow

1. **Motor Control (`stepper_node.c`)**  
   `stepper_node.c` controls the rotation of the stepper motor and defines the current angular position.

2. **Measurement Request (UDP Communication)**  
   After each motor step:
   - A UDP request is sent to `sensor_node.py`
   - The program waits for the corresponding distance measurement

3. **Distance Acquisition (`sensor_node.py`)**  
   Upon receiving a request:
   - Five consecutive distance measurements are performed
   - Invalid (zero) readings are filtered out
   - The average of valid measurements is computed
   - If all readings are zero, the last valid distance value is returned
   - The final distance value is sent back via UDP

4. **Data Processing and Publishing (`stepper_node.c`)**  
   After receiving the distance:
   - The measurement is stored in an angle-indexed buffer
   - The new value is compared with the previous measurement
   - If the absolute difference is greater than or equal to `DELTA`, the complete measurement array is published via ZeroMQ

5. **Visualization (`plot_node.py`)**  
   `plot_node.py` subscribes to the published data and:
   - Displays the initial scan in **blue**
   - Highlights measurements with significant changes (greater than `DELTA`) in **red**

---

# Hardware Description

The system is designed as a rotating sensing platform.

## Components

* **Computing Unit:** Raspberry Pi 2 Model B
* **Sensor:** VL53L1X Time-of-Flight (ToF) Long Range Distance Sensor
* **Actuator:** 28BYJ-48 Stepper Motor (5V)
* **Motor Driver IC:** ULN2003 Darlington Transistor Array
* **Mechanical Interface:**

  * Metal flange motor shaft coupling
  * Custom 3D-printed sensor adapter (located in `CAD/`)

---

# Wiring

## VL53L1X - Raspberry Pi (I2C)

| VL53L1X Pin | RPi Pin | Function           |
| ----------- | ------- | ------------------ |
| VCC         | Pin 1   | 3.3V               |
| GND         | Pin 9   | Ground             |
| SDA         | Pin 3   | GPIO 2 (I2C Data)  |
| SCL         | Pin 5   | GPIO 3 (I2C Clock) |
| XSHUT       | Pin 17  | GPIO 11            |

---

## ULN2003 - Raspberry Pi

| ULN2003 Pin | RPi Pin | Function |
| ----------- | ------- | -------- |
| VCC         | Pin 2   | 5V       |
| GND         | Pin 6   | Ground   |
| IN1         | Pin 11  | GPIO 17  |
| IN2         | Pin 12  | GPIO 18  |
| IN3         | Pin 15  | GPIO 22  |
| IN4         | Pin 16  | GPIO 23  |

> The ULN2003 output connects directly to the 28BYJ-48 motor (5-pin JST connector).

---

# Software Architecture

The software is structured into three functional layers:

1. **Control Layer** – `stepper_node.c`  
2. **Sensing Layer** – `sensor_node.py`  
3. **Visualization Layer** – `plot_node.py`  

Communication between these layers is implemented using two different mechanisms:

- **UDP** for deterministic request–response communication  
- **ZeroMQ (PUB/SUB)** for asynchronous data streaming  

This separation ensures low-latency sensing while keeping visualization independent from real-time motor control.

---

## UDP Communication (Control ↔ Sensor Layer)

UDP is used for direct communication between the stepper controller and the distance sensor node.

### Roles

- `stepper_node.c` → **UDP Client**  
- `sensor_node.py` → **UDP Server**  
- Port: **32501**  
- Protocol: IPv4  

### Communication Flow

For every angular position:

1. `stepper_node.c` sends a request packet:
   ```
   "REQ"
   ```

3. Upon receiving the request, `sensor_node.py`:
   - Performs **5 consecutive distance measurements**
   - Filters out invalid (zero) readings
   - Computes the average of valid samples
   - If all measurements are zero, returns the **last valid distance**
   - Sends back a 4-byte unsigned integer (`uint32_t`) representing the measured distance in millimeters

4. `stepper_node.c` receives:
   ```
   uint32_t distance (4 bytes)
   ```
---

## ZeroMQ Communication (Control ↔ Visualization Layer)

ZeroMQ is used for asynchronous publishing of measurement data toward the visualization node.

### Roles

- `stepper_node.c` → **ZeroMQ Publisher**
  
  ```
  tcp://*:5555
  ```
- `plot_node.py` → **ZeroMQ Subscriber**

### Publishing Logic

After each new measurement:

1. The distance value is stored in an angle-indexed buffer.
2. The new value is compared with the previously published value.
3. The entire measurement array is published. If the absolute difference satisfies:

   ```
   |new_value - previous_value| ≥ DELTA
   ```

This prevents unnecessary data transmission and reduces CPU load on the visualization side.

### Data Format

Each published message contains:

```
NUM_OF_ANGLES × uint32_t
```

Where:
- `NUM_OF_ANGLES = 64`
- Each element corresponds to one discrete angular position
- Values represent distance in millimeters

---

# Visualization

The `plot_node.py` application subscribes to ZeroMQ messages and renders the data using polar plots in real time.

---

## Plot Configuration

Two independent figures are generated:

### Figure 1 — Initial Scan
- Displays the first complete measurement set  
- Represents the reference environment state  
- Points are plotted in **blue**

### Figure 2 — Change Detection
- Displays only points where:

  ```
  |current_value - initial_value| > DELTA
  ```

- Significant environmental changes are plotted in **red**  
- If no changes exceed the threshold, the plot remains empty  

---

## Polar Coordinate System

The visualization uses a polar coordinate system configured as follows:

- 0° aligned with **North**  
- Counter-clockwise angular direction  
- Maximum radial distance: **2000 mm**

Angular resolution:

```
NUM_OF_ANGLES = 64
Angular step = 360° / 64
```

Each array index directly corresponds to a fixed angular position in space.

---

# How to Run the System

You need:

* 2 SSH terminals (connected to Raspberry Pi)
* 1 local terminal (PC for visualization)

---

## Start the Sensor Node (SSH)

```bash
python3 sensor_node.py
```

---

## Start the Plot Node (Local PC)

```bash
python3 plot_node.py
```

---

## Build and Start GPIO Driver (SSH)

Navigate to the driver folder:

```bash
make start
make run
```

---

## Build and Run Stepper Node (SSH)

```bash
./waf configure
./waf build
sudo ./build/stepper_node
```

---

# Stopping the System

While `stepper_node` is running, press:

```
q
```

The motor will return to its initial position and the program will exit safely.

---

# Project Structure

```
.
├── CAD/
│   └── Sensor Flange Adapter.stl        # 3D model for mounting the sensor
│
├── Docs/
│   └── README.md                        # Project documentation
│
└── SW/
    ├── App/
    │   ├── stepper_node.c               # Main stepper + UDP + ZeroMQ application
    │   ├── sensor_node.py               # UDP sensor server (VL53L1X)
    │   ├── plot_node.py                 # Real-time visualization (PC side)
    │   ├── wscript                      # Waf build configuration
    │   └── waf                          # Waf build system
    │                          
    └── Driver/
        └── gpio_ctrl/
            ├── gpio.c                   # GPIO driver implementation
            ├── gpio.h                   # GPIO driver header
            ├── main.c                   
            ├── include/
            │   └── gpio_ctrl.h          
            ├── Makefile                 
            └── gpio_ctrl.ko             

```

---

# Authors

Ivan Berenić and Aljoša Špika

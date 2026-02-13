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

---

# System Workflow

1. `stepper_node.c` rotates the stepper motor.
2. After each movement:

   * It sends a UDP request to `sensor_node.py`
   * Waits for the measured distance
3. `sensor_node.py`:

   * Performs 5 measurements
   * Averages valid (non-zero) values
   * Sends the result back via UDP
4. `stepper_node.c`:

   * Stores the measurement in a buffer
   * Checks if the change is ≥ `DELTA`
   * If true, publishes the full measurement array via ZeroMQ
5. `plot_node.py`:

   * Displays the initial scan (blue)
   * Displays points with significant change (red)

---

# Hardware Description

The system is designed as a rotating sensing platform.

## Components

* **Computing Unit:** Raspberry Pi 2 Model B
* **Sensor:** VL53L1X Time-of-Flight (ToF) Long Range Distance Sensor
* **Actuator:** 28BYJ-48 Stepper Motor (5V)
* **Driver:** ULN2003 Darlington Transistor Array
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

## UDP Communication

* `stepper_node.c` → UDP client
* `sensor_node.py` → UDP server
* Port: **32501**

Communication format:

```
Stepper → "REQ"
Sensor  → uint32_t distance (4 bytes)
```

---

## ZeroMQ Communication

* `stepper_node.c` → ZMQ Publisher (`tcp://*:5555`)
* `plot_node.py` → ZMQ Subscriber

Data format:

```
NUM_OF_ANGLES × uint32_t
```

---

# Visualization

`plot_node.py` generates:

* **Figure 1** → Initial scan
* **Figure 2** → Points where change > `DELTA`

Parameters:

```
NUM_OF_ANGLES = 64
DELTA = 50 mm
```

The plot uses polar coordinates with:

* 0° at North
* Counter-clockwise direction
* Maximum range: 2100 mm

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

# System Features

* 64 angular positions
* Bidirectional scanning (CW + CCW)
* Averaging of 5 measurements
* Filtering of zero readings
* Real-time change detection
* Dual polar visualization
* Thread-based safe exit
* Custom Linux GPIO driver

---

# Possible Improvements

* Full 360° continuous rotation
* Interpolation between points
* CSV export
* SLAM integration
* 3D mapping
* Adjustable DELTA parameter
* Performance optimization

---

# Authors

Ivan Berenić
Aljoša Špika
Faculty of Technical Sciences
Computer Science and Automation

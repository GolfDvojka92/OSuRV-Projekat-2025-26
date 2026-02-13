import zmq
import struct
import numpy as np
import matplotlib.pyplot as plt
import time

# ==============================
# ZeroMQ subscriber
# ==============================
context = zmq.Context()
subscriber = context.socket(zmq.SUB)
subscriber.connect("tcp://10.42.0.84:5555")
subscriber.setsockopt_string(zmq.SUBSCRIBE, "")

time.sleep(0.5)
print("Waiting for messages...")

# ==============================
# Plot setup
# ==============================
plt.ion()
fig1 = plt.figure(figsize=(8, 8))
ax1 = fig1.add_subplot(111, polar=True)

ax1.set_theta_zero_location('N')   # 0° gore
ax1.set_theta_direction(1)         # CCW
ax1.set_rmax(2100)
fig2 = plt.figure(figsize=(8, 8))
ax2 = fig2.add_subplot(111, polar=True)

ax2.set_theta_zero_location('N')   # 0° gore
ax2.set_theta_direction(1)         # CCW
ax2.set_rmax(2100)

scat_initial = ax1.scatter([], [], s=25, c='blue', label='Inicijalno stanje')
scat_delta = ax2.scatter([], [], s=25, c='red', label='Pomeraj')
delta  = 50

plt.show()

# ==============================
# Angle definition
# ==============================
NUM_OF_ANGLES = 64
angle_step = 360 / NUM_OF_ANGLES

angles_deg = np.arange(0, 360, angle_step)
angles_rad = np.deg2rad(angles_deg)

# ==============================
# Calibration loop
# ==============================
for i in range (0,NUM_OF_ANGLES):
    msg = subscriber.recv()

    if len(msg) % 4 == 0:

        num_values = len(msg) // 4
        initial_measurements = list(struct.unpack(f'<{num_values}I', msg))

        if len(initial_measurements) != NUM_OF_ANGLES:
            print("⚠ Pogrešan broj merenja")
            continue

        print("Received measurements")

        scat_initial.set_offsets(np.c_[angles_rad, initial_measurements])

        fig1.canvas.draw_idle()
        fig1.canvas.flush_events()

    else:
        try:
            text = msg.decode('utf-8')
            print(f"Received (string): {text}")
        except UnicodeDecodeError:
            print("Received unknown binary data")

print("Initial measurements saved")

# ==============================
# Main loop
# ==============================
while True:
    msg = subscriber.recv()

    if len(msg) % 4 == 0:

        num_values = len(msg) // 4
        measurements = list(struct.unpack(f'<{num_values}I', msg))

        if len(measurements) != NUM_OF_ANGLES:
            print("⚠ Pogrešan broj merenja")
            continue

        print("Received measurements")

        current = np.array(measurements)
        diff = np.abs(current - initial_measurements)

        mask = diff > delta

        # Ako nema promena, obriši sve tačke
        if np.any(mask):
            scat_delta.set_offsets(np.c_[angles_rad[mask], current[mask]])
        else:
            scat_delta.set_offsets(np.empty((0, 2)))

        fig2.canvas.draw_idle()
        fig2.canvas.flush_events()


    else:
        try:
            text = msg.decode('utf-8')
            print(f"Received (string): {text}")
        except UnicodeDecodeError:
            print("Received unknown binary data")

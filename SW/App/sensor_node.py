import VL53L1X
import socket
import struct
import time

SERVER_IP = "0.0.0.0"  
SERVER_PORT = 32501
NUM_SAMPLES = 5  # Number of meas per req

# Server socket creation 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((SERVER_IP, SERVER_PORT))
print(f"[INFO] Sensor UDP server working on  {SERVER_IP}:{SERVER_PORT}")

# Sensor init
tof = VL53L1X.VL53L1X(i2c_bus=1, i2c_address=0x29)
tof.open()
tof.set_timing(35000, 40)
tof.start_ranging(1)  

# Memorize last valid distance
last_valid_distance = 0

try:
    while True:
        # Wait stepper request
        data, addr = sock.recvfrom(1024)
        if not data:
            continue

        # Measure NUM_SAMPLES times and average only valid (non-zero) readings
        distances = []
        for _ in range(NUM_SAMPLES):
            distance_mm = tof.get_distance()
            if distance_mm != 0:
                distances.append(distance_mm)
           
        # If no valid measuements, use the last measurement
        if len(distances) == 0:
            avg_distance = last_valid_distance
        else:
            avg_distance = int(sum(distances) / len(distances))
            last_valid_distance = avg_distance  # Update last valid value
            
        print(f"[DEBUG] Req from {addr}, distances = {distances}, avg = {avg_distance} mm")

        # Pack average distance to 4B unsigned int and send
        packet = struct.pack("I", avg_distance)
        sock.sendto(packet, addr)

except KeyboardInterrupt:
    print("[INFO] Sensor server stopped by user")

finally:
    tof.stop_ranging()
    tof.close()
    sock.close()
    print("[INFO] Socket closed")


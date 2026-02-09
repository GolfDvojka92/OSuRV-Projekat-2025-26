import VL53L1X
import socket
import struct

SERVER_IP = "0.0.0.0"  
SERVER_PORT = 32501

# Server socket creation 
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((SERVER_IP, SERVER_PORT))
print(f"[INFO] Sensor UDP server working on  {SERVER_IP}:{SERVER_PORT}")

# Sensor init
tof = VL53L1X.VL53L1X(i2c_bus=1, i2c_address=0x29)
tof.open()
tof.start_ranging(1)  

try:
    while True:
        # Wait stepper req
        data, addr = sock.recvfrom(1024)  # buffer size 1024B
        if not data:
            continue

        # Get distance
        distance_mm = tof.get_distance()
            
        print(f"[DEBUG] Req from {addr}, distance = {distance_mm} mm")

        # Pack distance to 4B unsigned int and send it to stepper
        packet = struct.pack("I", distance_mm)
        sock.sendto(packet, addr)

except KeyboardInterrupt:
    print("[INFO] Sensor server stopped by user")

finally:
    tof.stop_ranging()
    tof.close()
    sock.close()
    print("[INFO] Socket closed")


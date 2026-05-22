import serial
import csv
import struct
from datetime import datetime

PORT = '/dev/rfcomm0'
BAUD = 115200

label = "running"

time_str = datetime.now().strftime("%Y%m%d_%H%M%S")
SAVE_PATH = f"../data/{label}_{time_str}.csv"

print("RECORDING...")


try:
    with serial.Serial(PORT, BAUD, timeout=0.1) as ser:
        ser.reset_input_buffer()

        with open(SAVE_PATH, 'w', newline= '') as file:
            writer = csv.writer(file)
            writer.writerow(["timestamp", "ax", "ay", "az", "gx", "gy", "gz", "label"])

            count = 0
            while True:
                while ser.in_waiting >= 16:
                    try:
                        data = ser.read(16)
                        if len(data) == 16:
                            timestamp, ax, ay, az, gx, gy, gz = struct.unpack('<Ihhhhhh', data)

                            writer.writerow([timestamp,
                                            round(ax / 4096.0, 6),
                                            round(ay / 4096.0, 6),
                                            round(az / 4096.0, 6),
                                            round(gx / 32.8, 6),
                                            round(gy / 32.8, 6),
                                            round(gz / 32.8, 6),
                                            label])
                            count += 1

                        if count % 100 == 0:
                            print(f"Number of samples: {count}, time: {count/50:.2f} s")
                    except:
                        ser.read(1)
                        continue
except KeyboardInterrupt:
    print(f"Recording has finished, samples: {count}, time: {count/50:.2f} s")
except Exception as e:
    print(f"\nError: {e}")
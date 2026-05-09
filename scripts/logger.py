import serial
import csv
import os
from datetime import datetime

PORT = '/dev/ttyUSB0'
BAUD = 115200

label = "running"

time_str = datetime.now().strftime("%Y%m%d_%H%M%S")
SAVE_PATH = f"../data/{label}_{time_str}.csv"

print("RECORDING...")

try:
    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        ser.reset_input_buffer()

        with open(SAVE_PATH, 'w', newline= '') as file:
            writer = csv.writer(file)
            writer.writerow(["timestamp", "ax", "ay", "az", "gx", "gy", "gz", "label"])

            count = 0
            while True:
                sample = ser.readline().decode('utf-8', errors='ignore').strip()
                frame = sample.split(',')

                if len(frame) == 7:
                    writer.writerow(frame + [label])
                    count += 1

                    if count % 100 == 0:
                        print(f"Number of samples: {count}, time: {count/50:.2f} s")
except KeyboardInterrupt:
    print(f"Recording has finished, samples: {count}, time: {count/50:.2f} s")
except Exception as e:
    print(f"\nError: {e}")
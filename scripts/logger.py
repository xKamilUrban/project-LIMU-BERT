import csv
import struct
import paho.mqtt.client as mqtt
from datetime import datetime
import time

PACKET_SIZE = 16
label = "running"

time_str = datetime.now().strftime("%Y%m%d_%H%M%S")
SAVE_PATH = f"../data/{label}_{time_str}.csv"

count = 0
writer = None
running = True

def process_data(data):
    global count
    if len(data) == PACKET_SIZE:
        try:
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
                print(f"Samples: {count}, time: {count/50:.2f} s")
        except struct.error:
            pass

def on_message(client, userdata, msg):
    global running
    if msg.topic == "/imu/control":
        command = msg.payload.decode('utf-8')
        if command == "STOP":
            print("\n[!] STOP")
            running = False

    elif msg.topic == "/imu/data":
        for i in range(0, len(msg.payload), PACKET_SIZE):
            chunk = msg.payload[i:i+PACKET_SIZE]
            if len(chunk) == PACKET_SIZE:
                process_data(chunk)

def on_connect(client, userdata, flags, rc):
    client.subscribe("/imu/data", qos=1)
    client.subscribe("/imu/control", qos=1)

try:
    with open(SAVE_PATH, 'w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(["timestamp", "ax", "ay", "az", "gx", "gy", "gz", "label"])

        mqtt_client = mqtt.Client()
        mqtt_client.on_connect = on_connect
        mqtt_client.on_message = on_message

        mqtt_client.connect("broker.emqx.io", 1883, keepalive=60)

        print("RECORDING...")
        mqtt_client.loop_start()
        while running:
            time.sleep(0.1)

        mqtt_client.loop_stop()
        mqtt_client.disconnect()

except KeyboardInterrupt:
    print(f"Recording finished, samples: {count}, time: {count/50:.2f} s")
except Exception as e:
    print(f"Error: {e}")
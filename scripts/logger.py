import csv
import struct
import paho.mqtt.client as mqtt
from datetime import datetime
import time

PACKET_SIZE = 16
label = "jumping_jacks"

count = 0
writer = None
file_handle = None
is_recording = False
running = True

def process_data(data):
    global count, writer
    if len(data) == PACKET_SIZE and writer is not None:
        try:
            timestamp, ax, ay, az, gx, gy, gz = struct.unpack('<Ihhhhhh', data)
            writer.writerow([round(ax / 4096.0, 6),
                            round(ay / 4096.0, 6),
                            round(az / 4096.0, 6),
                            round(gx / 32.8, 6),
                            round(gy / 32.8, 6),
                            round(gz / 32.8, 6),
                            label])
            count += 1
            if count % 50 == 0:
                print(f"Samples: {count}, time: {count/50:.2f} s")
        except struct.error:
            pass

def on_message(client, userdata, msg):
    global is_recording, writer, file_handle, count
    
    if msg.topic == "/imu/control":
        command = msg.payload.decode('utf-8')
        
        if command in ["STOP", "TOGGLE"]: 
            if not is_recording:
                count = 0
                time_str = datetime.now().strftime("%Y%m%d_%H%M%S")
                save_path = f"../data/{label}_{time_str}.csv"
                
                file_handle = open(save_path, 'w', newline='')
                writer = csv.writer(file_handle)
                writer.writerow(["acc_x", "acc_y", "acc_z", "gyro_x", "gyro_y", "gyro_z", "label"])
                
                is_recording = True
                print(f"\nSaving to: {save_path}")
            else:
                is_recording = False
                if file_handle:
                    file_handle.close()
                    file_handle = None
                    writer = None
                print(f"Recording finished, samples: {count}, time: {count/50:.2f} s")
                print("Press the button to record")

    elif msg.topic == "/imu/data":
        if is_recording:
            for i in range(0, len(msg.payload), PACKET_SIZE):
                chunk = msg.payload[i:i+PACKET_SIZE]
                if len(chunk) == PACKET_SIZE:
                    process_data(chunk)

def on_connect(client, userdata, flags, rc):
    client.subscribe("/imu/data", qos=1)
    client.subscribe("/imu/control", qos=1)

try:
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.connect("broker.emqx.io", 1883, keepalive=60)

    print("Press the button to record")
    mqtt_client.loop_start()
    
    while running:
        time.sleep(0.1)

except KeyboardInterrupt:
    print("\nFinished")
finally:
    if file_handle:
        file_handle.close()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
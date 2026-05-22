import serial
import struct
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

PORT = '/dev/rfcomm0'
BAUD = 115200
WINDOW_SIZE = 100

ax_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
ay_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
az_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
gx_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
gy_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
gz_d = deque([0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    print(f"connected, PORT: {PORT}")
except Exception as e:
    print(f"Error: {e}")
    exit()

fig, (ax_top, ax_bot) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

line_ax, = ax_top.plot(ax_d, label='AX', color='red')
line_ay, = ax_top.plot(ay_d, label='AY', color='green')
line_az, = ax_top.plot(az_d, label='AZ', color='blue')
ax_top.set_ylim(-8, 8)
ax_top.set_ylabel("accel [g]")
ax_top.legend(loc='upper right')
ax_top.grid(True, alpha=0.3)

line_gx, = ax_bot.plot(gx_d, label='GX', color='orange')
line_gy, = ax_bot.plot(gy_d, label='GY', color='purple')
line_gz, = ax_bot.plot(gz_d, label='GZ', color='brown')
ax_bot.set_ylim(-1000, 1000)
ax_bot.set_ylabel("gyro [deg/s]")
ax_bot.legend(loc='upper right')
ax_bot.grid(True, alpha=0.3)

def update(frame):
    while ser.in_waiting >= 16:
        try:
            data = ser.read(16)
            if len(data) == 16:
                timestamp, ax, ay, az, gx, gy, gz = struct.unpack('<Ihhhhhh', data)

                ax_d.append(ax / 4096.0)
                ay_d.append(ay / 4096.0)
                az_d.append(az / 4096.0)
                gx_d.append(gx / 32.8)
                gy_d.append(gy / 32.8)
                gz_d.append(gz / 32.8)
        except:
            continue

    line_ax.set_ydata(ax_d); line_ay.set_ydata(ay_d); line_az.set_ydata(az_d)
    line_gx.set_ydata(gx_d); line_gy.set_ydata(gy_d); line_gz.set_ydata(gz_d)

    return line_ax, line_ay, line_az, line_gx, line_gy, line_gz

ani = FuncAnimation(fig, update, interval=10, blit=True, cache_frame_data=False)

plt.tight_layout()
plt.show()

ser.close()
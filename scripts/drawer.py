import serial
import struct
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

PORT = '/dev/rfcomm0'
BAUD = 115200
ser = serial.Serial(PORT, BAUD, timeout=0.1)

pen_x, pen_y = 0.0, 0.0
trace_x = deque([0.0], maxlen=200)
trace_y = deque([0.0], maxlen=200)
prev_timestamp = None

fig, ax = plt.subplots(figsize=(8, 8))
line, = ax.plot([], [], 'b-', lw=2)
ax.set_xlim(-500, 500)
ax.set_ylim(-500, 500)

def update(frame):
    global pen_x, pen_y, prev_timestamp

    while ser.in_waiting >= 16:
        try:
            data = ser.read(16)
            if len(data) == 16:
                timestamp, ax, ay, az, gx, gy, gz = struct.unpack('<Ihhhhhh', data)

                if prev_timestamp is not None:
                    dt = (timestamp - prev_timestamp) / 1000.0  # ms -> s
                    pen_x -= gz * dt
                    pen_y += gx * dt
                    trace_x.append(pen_x)
                    trace_y.append(pen_y)

                prev_timestamp = timestamp
        except struct.error:
            # Utrata synchronizacji — przesuń o 1 bajt i szukaj dalej
            ser.read(1)
            continue

    line.set_data(list(trace_x), list(trace_y))
    return line,

ani = FuncAnimation(fig, update, interval=10, blit=True, cache_frame_data=False)
plt.show()
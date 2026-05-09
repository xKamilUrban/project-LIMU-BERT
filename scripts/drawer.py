import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


PORT = '/dev/ttyUSB0'
BAUD = 115200
ser = serial.Serial(PORT, BAUD, timeout=0.1)

pen_x, pen_y = 0, 0
trace_x, trace_y = [0], [0]


fig, ax = plt.subplots(figsize=(8, 8))
line, = ax.plot([], [], 'b-', lw=2)
ax.set_xlim(-500, 500)
ax.set_ylim(-500, 500)

def update(frame):
    global pen_x, pen_y
    
    while ser.in_waiting > 0:
        try:
            line_raw = ser.readline().decode('utf-8', errors='ignore').strip()
            parts = line_raw.split(',')
            if len(parts) == 7:

                gx = float(parts[4]) 
                gy = float(parts[5])
                gz = float(parts[6])

                pen_x -= gz * 0.5
                pen_y += gx * 0.5

                trace_x.append(pen_x)
                trace_y.append(pen_y)
                
                if len(trace_x) > 200:
                    trace_x.pop(0)
                    trace_y.pop(0)
        except:
            continue

    line.set_data(trace_x, trace_y)
    return line,

ani = FuncAnimation(fig, update, interval=10, blit=True, cache_frame_data=False)
plt.show()
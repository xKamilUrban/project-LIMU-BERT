import pandas as pd
import matplotlib.pyplot as plt

file_path = "../data/running_20260530_170441.csv"
df = pd.read_csv(file_path)

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

ax1.plot(df['ax'], label='Accel X')
ax1.plot(df['ay'], label='Accel Y')
ax1.plot(df['az'], label='Accel Z')
ax1.set_title(f"Accelerometer data - {df['label'][0]}")
ax1.legend()
ax1.grid(True)

ax2.plot(df['gx'], label='Gyro X', color='orange')
ax2.plot(df['gy'], label='Gyro Y', color='green')
ax2.plot(df['gz'], label='Gyro Z', color='red')
ax2.set_title("Gyroscope data")
ax2.legend()
ax2.grid(True)

plt.xlabel("Number of sample")
plt.tight_layout()
plt.show()
import pandas as pd
import matplotlib.pyplot as plt

file_path = "../data/jumping_jacks_20260607_215611.csv"

df = pd.read_csv(file_path)

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

ax1.plot(df['acc_x'], label='Accel X')
ax1.plot(df['acc_y'], label='Accel Y')
ax1.plot(df['acc_z'], label='Accel Z')
ax1.set_title(f"Accelerometer data - {df['label'].iloc[0]}")
ax1.legend()
ax1.grid(True)

ax2.plot(df['gyro_x'], label='Gyro X', color='orange')
ax2.plot(df['gyro_y'], label='Gyro Y', color='green')
ax2.plot(df['gyro_z'], label='Gyro Z', color='red')
ax2.set_title("Gyroscope data")
ax2.legend()
ax2.grid(True)

plt.xlabel("Number of sample")
plt.tight_layout()
plt.show()
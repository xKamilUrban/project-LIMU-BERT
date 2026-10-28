import os
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.widgets import TextBox, Button

file_path = "../data/walking_20260608_171650.csv"

if not os.path.exists(file_path):
    print(f"Error: File {file_path} does not exist.")
    exit()

df = pd.read_csv(file_path)
df_clean = df.copy()

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
plt.subplots_adjust(bottom=0.2)

line_ax, = ax1.plot(df['acc_x'], label='Accel X')
line_ay, = ax1.plot(df['acc_y'], label='Accel Y')
line_az, = ax1.plot(df['acc_z'], label='Accel Z')
label_name = df['label'].iloc[0] if 'label' in df.columns else "Unknown"
ax1.set_title(f"Accelerometer Data - {label_name}")
ax1.legend()
ax1.grid(True)

line_gx, = ax2.plot(df['gyro_x'], label='Gyro X', color='orange')
line_gy, = ax2.plot(df['gyro_y'], label='Gyro Y', color='green')
line_gz, = ax2.plot(df['gyro_z'], label='Gyro Z', color='red')
ax2.set_title("Gyroscope Data")
ax2.legend()
ax2.grid(True)
ax2.set_xlabel("Sample Index")

ax_start = plt.axes([0.1, 0.05, 0.1, 0.04])
ax_end = plt.axes([0.25, 0.05, 0.1, 0.04])
ax_btn_trim = plt.axes([0.4, 0.05, 0.1, 0.04])
ax_btn_save = plt.axes([0.55, 0.05, 0.1, 0.04])

txt_start = TextBox(ax_start, 'Start: ', initial='0')
txt_end = TextBox(ax_end, 'End: ', initial=str(len(df)-1))
btn_trim = Button(ax_btn_trim, 'Preview Trim', color='lightblue')
btn_save = Button(ax_btn_save, 'Save File', color='lightgreen')

def update_plots(start, end):
    global df_clean
    try:
        s, e = int(start), int(end)
        if 0 <= s < e < len(df):
            df_clean = df.iloc[s:e].reset_index(drop=True)
            
            ax1.set_xlim(s, e)
            ax2.set_xlim(s, e)
            
            y1_min = df_clean[['acc_x', 'acc_y', 'acc_z']].min().min()
            y1_max = df_clean[['acc_x', 'acc_y', 'acc_z']].max().max()
            ax1.set_ylim(y1_min - 0.5, y1_max + 0.5)
            
            y2_min = df_clean[['gyro_x', 'gyro_y', 'gyro_z']].min().min()
            y2_max = df_clean[['gyro_x', 'gyro_y', 'gyro_z']].max().max()
            ax2.set_ylim(y2_min - 10, y2_max + 10)
            
            fig.canvas.draw_idle()
        else:
            print("Invalid range values.")
    except ValueError:
        print("Please enter valid integers.")

def on_trim_clicked(event):
    update_plots(txt_start.text, txt_end.text)

def on_save_clicked(event):
    backup_path = file_path + ".bak"
    if not os.path.exists(backup_path):
        os.rename(file_path, backup_path)
        print(f"Backup preserved at: {backup_path}")
    else:
        os.remove(file_path)
        
    df_clean.to_csv(file_path, index=False)
    print(f"File successfully overwritten: {file_path}")
    plt.close()

btn_trim.on_clicked(on_trim_clicked)
btn_save.on_clicked(on_save_clicked)

plt.show()
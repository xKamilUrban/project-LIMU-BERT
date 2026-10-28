import os
import sys
import glob
import json
import time
import struct
from collections import deque

import numpy as np
import serial
import torch
from scipy import signal
import matplotlib.pyplot as plt
from sklearn.decomposition import PCA


PROJECT_DIR = '/home/kamil/Dokumenty/PlatformIO/Projects/project-LIMU-BERT'
MODEL_DIR = f'{PROJECT_DIR}/limu_model'


BERT_WEIGHTS = f'{MODEL_DIR}/pretrain_base_motion_20_120/limu_v1.pt'
CLF_WEIGHTS = f'{MODEL_DIR}/classifier_base_gru_motion_20_120/model.pt'


CLASSES = ['walking', 'sitting', 'standing', 'squats', 'pushups', 'jumping_jacks']

MODEL_VERSION = 'v1'

PORT = '/dev/rfcomm0'
BAUD = 115200
PACKET_SIZE = 16


ACC_SCALE = 1.0 / 4096.0
GYRO_SCALE = 1.0 / 32.8

SOURCE_HZ = 50
TARGET_HZ = 20
RAW_LEN = 350 
EDGE = 10
PRED_INTERVAL_S = 1.0    
PLOT = True
PLOT_DATA = f'{MODEL_DIR}/plot_data.npz'
DEBUG = True
SMOOTH_N = 3


models_py = glob.glob(f'{PROJECT_DIR}/**/models.py', recursive=True)
if not models_py:
    raise FileNotFoundError('Nie znaleziono models.py repozytorium LIMU-BERT')
repo_path = os.path.dirname(models_py[0])
os.chdir(repo_path)
sys.path.append(repo_path)

from models import LIMUBertModel4Pretrain, fetch_classifier
from config import PretrainModelConfig, ClassifierModelConfig

with open(os.path.join(repo_path, 'config', 'limu_bert.json')) as f:
    bert_cfg = PretrainModelConfig.from_json(json.load(f)['base_' + MODEL_VERSION])
with open(os.path.join(repo_path, 'config', 'classifier.json')) as f:
    clf_cfg = ClassifierModelConfig.from_json(json.load(f)['gru_' + MODEL_VERSION])

WINDOW = bert_cfg.seq_len          # 120
CHUNK = clf_cfg.seq_len            # 20
N_CHUNKS = WINDOW // CHUNK


def load_state(path):
    sd = torch.load(path, map_location='cpu')
    if isinstance(sd, dict) and 'model_state_dict' in sd:
        sd = sd['model_state_dict']
    return sd


bert = LIMUBertModel4Pretrain(bert_cfg)
sd = load_state(BERT_WEIGHTS)
try:
    bert.load_state_dict(sd, strict=True)
except RuntimeError:
    bert.transformer.load_state_dict(sd, strict=True)
bert.eval()

clf = fetch_classifier('gru', clf_cfg, input=bert_cfg.hidden, output=len(CLASSES))
clf.load_state_dict(load_state(CLF_WEIGHTS), strict=True)
clf.eval()
print('Modele załadowane.')

if PLOT:
    d = np.load(PLOT_DATA)
    bg = d['emb'].reshape(len(d['emb']), -1)
    pca = PCA(n_components=2).fit(bg)
    bg2 = pca.transform(bg)
    plt.ion()
    fig, ax = plt.subplots(figsize=(7, 6))
    for k, name in enumerate(CLASSES):
        m = d['y'] == k
        ax.scatter(bg2[m, 0], bg2[m, 1], s=8, alpha=0.5, label=name)
    trail = deque(maxlen=10)
    trail_line, = ax.plot([], [], '-', color='black', alpha=0.4)
    now_pt, = ax.plot([], [], 'o', color='black', markersize=14,
                      markerfacecolor='none', markeredgewidth=2, label='teraz')
    ax.legend(loc='best')
    ax.set_title('PCA embeddingów: tło = dane treningowe, kółko = bieżące okno')
    plt.show(block=False)


def update_plot(emb_flat):
    p2 = pca.transform(emb_flat)[0]
    trail.append(p2)
    t = np.array(trail)
    trail_line.set_data(t[:, 0], t[:, 1])
    now_pt.set_data([p2[0]], [p2[1]])
    plt.pause(0.001)


raw_buf = deque(maxlen=RAW_LEN)
recent_probs = deque(maxlen=SMOOTH_N)


def make_window():
    raw = np.array(raw_buf, dtype=np.float64)
    res = signal.resample_poly(raw, up=TARGET_HZ, down=SOURCE_HZ, axis=0)
    w = res[EDGE:EDGE + WINDOW].astype(np.float32)
    w[:, :3] /= 9.8
    return w


def predict(window):
    x = torch.from_numpy(window).unsqueeze(0)
    with torch.no_grad():
        emb = bert.transformer(x)
        probs = []
        for i in range(N_CHUNKS):
            chunk = emb[:, i * CHUNK:(i + 1) * CHUNK, :]
            logits = clf(chunk, False).squeeze(0)
            probs.append(torch.softmax(logits, dim=-1))
        return torch.stack(probs).mean(dim=0).numpy(), emb.reshape(1, -1).numpy()


def parse_packet(data):
    try:
        _, ax, ay, az, gx, gy, gz = struct.unpack('<Ihhhhhh', data)
    except struct.error:
        return None
    return np.array([ax * ACC_SCALE, ay * ACC_SCALE, az * ACC_SCALE,
                     gx * GYRO_SCALE, gy * GYRO_SCALE, gz * GYRO_SCALE],
                    dtype=np.float32)


def connect():
    while True:
        try:
            print(f'Łączenie z {PORT}...')
            return serial.Serial(PORT, BAUD, timeout=0.1)
        except serial.SerialException as e:
            print(f'Błąd: {e}. ...')
            time.sleep(3)


ser = connect()
buf = bytearray()
last_pred = time.time()
n_packets = 0
t_rate = time.time()

try:
    while True:
        try:
            n = ser.in_waiting
            if n:
                buf.extend(ser.read(n))

            while len(buf) >= PACKET_SIZE:
                sample = parse_packet(bytes(buf[:PACKET_SIZE]))
                buf = buf[PACKET_SIZE:]
                if sample is not None:
                    raw_buf.append(sample)
                    n_packets += 1

            now = time.time()
            if len(raw_buf) == RAW_LEN and now - last_pred >= PRED_INTERVAL_S:
                last_pred = now
                if n_packets < 0.5 * SOURCE_HZ * PRED_INTERVAL_S:
                    print(f'Brak nowych danych z czujnika ({n_packets} probek w ostatnim interwale) - pomijam predykcje')
                    n_packets, t_rate = 0, now
                    time.sleep(0.005)
                    continue
                p_now, emb_flat = predict(make_window())
                recent_probs.append(p_now)
                if PLOT:
                    update_plot(emb_flat)
                probs = np.mean(recent_probs, axis=0)
                best = int(np.argmax(probs))
                print(f'\n>>> {CLASSES[best].upper()} ({probs[best] * 100:.1f}%)')
                for name, p in zip(CLASSES, probs):
                    print(f'  {name:15s} {"█" * int(p * 20):<20s} {p * 100:5.1f}%')
                if DEBUG:
                    hz = n_packets / (now - t_rate)
                    n_packets, t_rate = 0, now
                    r = np.array(raw_buf)
                    print(f'  [debug] {hz:.1f} Hz | |acc| srednio {np.linalg.norm(r[:, :3], axis=1).mean():.2f}'
                          f' | std acc {r[:, :3].std(0).round(3)} | std gyr {r[:, 3:].std(0).round(1)}')

            time.sleep(0.005)

        except OSError:
            print('Utracono połączenie. Ponawiam...')
            ser.close()
            raw_buf.clear()
            recent_probs.clear()
            buf = bytearray()
            time.sleep(2)
            ser = connect()

except KeyboardInterrupt:
    print('Zatrzymano.')
finally:
    ser.close()
import struct
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import serial

# --- 設定參數 ---
PORT = "/dev/ttyACM1"  # 請改為您的虛擬 COM 埠名稱 (Windows: 'COM3', Mac/Linux: '/dev/ttyUSB0' 等)
BAUDRATE = 115200
MAX_POINTS = 5000  # 畫面上最多顯示幾個取樣點
SAMPLE_SIZE_BYTES = 2
PACKET_START = b"SERR"
PACKET_END = b"IALL"

# 初始化序列埠
ser = serial.Serial(PORT, BAUDRATE, timeout=0.01)

# 初始化畫布
fig, ax = plt.subplots(figsize=(10, 5))
lines = []
x_data = np.arange(MAX_POINTS)
y_data = []  # 動態儲存各通道資料
rx_buffer = bytearray()


def parse_packet(ser_port):
  """解析序列埠封包

  封包格式:
    - Magic Start: 'SERR' (4 bytes)
    - Channel Count: 1 byte
    - Payload Length: 2 bytes (unsigned short, little-endian sample count)
    - Payload: sample count 個 uint16 little-endian ADC samples, interleaved by channel
    - Magic End: 'IALL' (4 bytes)
  """
  # Keep partial reads between animation frames so a serial timeout cannot
  # discard part of a packet.
  bytes_to_read = min(max(ser_port.in_waiting, 1), 4096)
  rx_buffer.extend(ser_port.read(bytes_to_read))

  while True:
    start = rx_buffer.find(PACKET_START)
    if start < 0:
      # Retain enough bytes to match a start marker split across reads.
      del rx_buffer[:-len(PACKET_START) + 1]
      return None
    if start:
      del rx_buffer[:start]

    if len(rx_buffer) < 7:
      return None

    num_channels = rx_buffer[4]
    (sample_count,) = struct.unpack_from("<H", rx_buffer, 5)
    if num_channels == 0 or sample_count == 0 or sample_count % num_channels:
      del rx_buffer[0]
      continue

    payload_bytes = sample_count * SAMPLE_SIZE_BYTES
    packet_size = 7 + payload_bytes + len(PACKET_END)
    if len(rx_buffer) < packet_size:
      return None

    end_offset = 7 + payload_bytes
    if rx_buffer[end_offset:packet_size] != PACKET_END:
      del rx_buffer[0]
      continue

    samples = np.frombuffer(rx_buffer[7:end_offset], dtype="<u2")
    channels = [
      samples[ch::num_channels].tolist()
      for ch in range(num_channels)
    ]
    del rx_buffer[:packet_size]
    return channels


def init_plot():
  """初始化圖表"""
  ax.set_xlim(0, MAX_POINTS)
  ax.set_ylim(0, 4095)  # STM32 ADC is configured for 12-bit unsigned samples.
  ax.set_title("Virtual COM Port Oscilloscope")
  ax.set_xlabel("Sample Index")
  ax.set_ylabel("Amplitude")
  ax.grid(True)
  return lines


def update(frame):
  """動畫更新函數：讀取資料並更新繪圖"""
  global y_data, lines

  # 嘗試讀取一個封包
  pkt = parse_packet(ser)
  if pkt is not None:
    num_ch = len(pkt)

    # 首次收到資料時，初始化通道線條與緩衝區
    if not y_data:
      y_data = [np.zeros(MAX_POINTS) for _ in range(num_ch)]
      for i in range(num_ch):
        (line,) = ax.plot(x_data, y_data[i], label=f"Channel {i+1}")
        lines.append(line)
      ax.legend(loc="upper right")

    # 更新資料緩衝區 (FIFO)
    for ch in range(num_ch):
      new_vals = pkt[ch]
      if new_vals:
        new_vals = np.asarray(new_vals[-MAX_POINTS:])
        shift = len(new_vals)
        y_data[ch] = np.roll(y_data[ch], -shift)
        y_data[ch][-shift:] = new_vals
        lines[ch].set_ydata(y_data[ch])

  return lines


# 啟動動畫 (interval=10 代表每 10ms 更新一次畫面)
ani = FuncAnimation(
    fig, update, init_func=init_plot, interval=100, blit=False, save_count=50
)

try:
  plt.show()
except KeyboardInterrupt:
  pass
finally:
  ser.close()
  print("序列埠已關閉")

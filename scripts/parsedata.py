import argparse
import threading
import queue
import serial
import struct
import time
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
from dataclasses import dataclass, field


MESSAGE_SIZE = 214
PAYLOAD_PIXEL_COUNT = 96

# Struct format (big-endian):
# B  = module_id      (1 byte)
# 6s = timestamp raw  (6 bytes)
# H  = gas_sensor     (2 bytes uint16)
# f  = temperature    (4 bytes float)
# f  = humidity       (4 bytes float)
# f  = pack_voltage   (4 bytes float)
# B  = row_sequence   (1 byte)
# 96H = payload       (96 x uint16 = 192 bytes)
_STRUCT_FORMAT = f"<BBHHHHfff{PAYLOAD_PIXEL_COUNT}H"
_STRUCT = struct.Struct(_STRUCT_FORMAT)

total_frames = [[[0] * 32 for _ in range(24)], [[0] * 32 for _ in range(24)], [[0] * 32 for _ in range(24)]]
full_msg = False

@dataclass
class SensorMessage:
    module_id: int
    sequence_num: int
    row_sequence: int
    timestamp: str
    gas_sensor: int
    temperature: float
    humidity: float
    pack_voltage: float
    payload: list[int]

    def __str__(self) -> str:
        return (
            f"SensorMessage(\n"
            f"  module_id    = {self.module_id}\n"
            f"  sequence_num = {self.sequence_num}\n"
            f"  row_sequence = {self.row_sequence}\n"
            f"  timestamp    = {self.timestamp} UTC\n"
            f"  gas_sensor   = {self.gas_sensor}\n"
            f"  temperature  = {self.temperature:.4f}\n"
            f"  humidity     = {self.humidity:.4f}\n"
            f"  pack_voltage = {self.pack_voltage:.4f}\n"
            f"  payload      = [{len(self.payload)} uint16 values] {self.payload[:8]}...\n"
            f")"
        )
    
last_message = None

def parse_timestamp(raw: bytes) -> str:
    """Convert 6 timestamp bytes to HH:MM:SS string.

    Assumes the 6 bytes encode hours, minutes, seconds each as a uint16
    (big-endian), i.e. [HH_hi, HH_lo, MM_hi, MM_lo, SS_hi, SS_lo].
    Adjust if your device uses a different encoding (e.g. 6 x uint8).
    """
    hh, mm, ss = struct.unpack(">HHH", raw)
    return f"{hh:02d}:{mm:02d}:{ss:02d}"


def parse_message(data: bytes) -> SensorMessage:
    """Parse a 214-byte sensor message into a SensorMessage dataclass.

    Args:
        data: 214 bytes of raw message data.

    Returns:
        A populated SensorMessage instance.

    Raises:
        ValueError: If data is not exactly 214 bytes.
        struct.error: If unpacking fails due to malformed data.
    """
    global last_message, total_frames, full_msg

    if len(data) != MESSAGE_SIZE:
        raise ValueError(f"Expected {MESSAGE_SIZE} bytes, got {len(data)}")

    unpacked = _STRUCT.unpack(data)
    module_id = data[0] & 0x0F
    sequence_num = (data[0] & 0xF0) >> 4
    # module_id    = unpacked[0] & 0x0F
    # sequence_num = (unpacked[0] & 0xF0) >> 4
    row_sequence = unpacked[1]
    hh = unpacked[2]
    mm = unpacked[3]
    ss = unpacked[4]
    timestamp    = f"{hh:02d}:{mm:02d}:{ss:02d}"
    gas_sensor   = unpacked[5]
    temperature  = unpacked[6]
    humidity     = unpacked[7]
    pack_voltage = unpacked[8]
    payload      = list(unpacked[9:])  # 96 uint16 pixel values

    if last_message != None and sequence_num != last_message.sequence_num:
        total_frames[sequence_num] = [[0] * 32 for _ in range(24)]

    for i in range(3):
        total_frames[sequence_num][row_sequence * 3 + i] = payload[i*32 : i*32 + 32]

    msg = SensorMessage(
        module_id=module_id,
        sequence_num=sequence_num,
        timestamp=timestamp,
        gas_sensor=gas_sensor,
        temperature=temperature,
        humidity=humidity,
        pack_voltage=pack_voltage,
        row_sequence=row_sequence,
        payload=payload,
    )

    last_message = msg
    return msg

def int_list_to_bytes(int_list: list[int]) -> bytes:
    """Convert a list of integers (each 0-255) to a bytes object.

    Args:
        int_list: A list of integers, each in range [0, 255].

    Returns:
        A bytes object of length len(int_list).

    Raises:
        ValueError: If any value is outside [0, 255] or the list length
                    does not match MESSAGE_SIZE.
    """
    if len(int_list) != MESSAGE_SIZE:
        raise ValueError(
            f"Expected {MESSAGE_SIZE} integers, got {len(int_list)}"
        )
    for i, v in enumerate(int_list):
        if not (0 <= v <= 255):
            raise ValueError(
                f"Value {v} at index {i} is out of byte range [0, 255]"
            )
    return bytes(int_list)


def uint16_to_float16(values: list[int]):
    raw_array = np.array(values, dtype=np.uint16)
    # raw_array = raw_array.byteswap() # TODO test - did nto look good
    float_array = raw_array.view(np.float16).astype(np.float32)
    # print(float_array)
    return float_array


def display_frame_data(frame_data: list[list[int]]):
    # flatten
    frame_data_two: list[int] = [data for data_row in frame_data for data in data_row]
    plt.close()
    plt.ion()
    fig, ax = plt.subplots(figsize=(12, 7))
    therm1 = ax.imshow(np.zeros((24, 32)), vmin=0, vmax=60)
    cbar = fig.colorbar(therm1)
    cbar.set_label('Temperature [$^{\circ}$C]', fontsize=14)
    # convert from raw values to 16-bit floats, to 32-bit floats
    float_values = uint16_to_float16(frame_data_two)
    # print colors!
    data_array = np.reshape(float_values, (24, 32)) # only 3 rows at a time
    therm1.set_data(np.fliplr(data_array))
    therm1.set_clim(vmin=np.min(-10), vmax=np.max(50))
    fig.canvas.draw()  # Redraw the figure to update the plot and colorbar
    fig.canvas.flush_events()
    # timestamp = time.strftime("%Y%m%d_%H%M%S")
    # plt.imsave(f"frame_{timestamp}.jpg", np.fliplr(data_array), vmin=np.min(data_array), vmax=np.max(data_array))
    time.sleep(1)
    


def serial_reader(port: str, baud_rate: int, data_queue: queue.Queue, stop_event: threading.Event) -> None:
    """Reads data from a COM port in chunks and places them into a queue."""
    try:
        with serial.Serial(port, baudrate=baud_rate, timeout=1) as ser:
            print(f"[Reader] Opened {port} at {baud_rate} baud.")
            ser.flush()

            while not stop_event.is_set():
                try:
                    line = ser.readline().decode('ascii', errors='replace').strip()
                    print("READ:")
                    print(line)
                    if not line.startswith("Data:"):
                        continue

                    hex_str = line[len("Data:"):].strip()
                    chunk = bytes.fromhex(hex_str)
                    # chunk += b'\x00'

                    if len(chunk) < MESSAGE_SIZE:
                        print(f"[Reader] Dropping short message ({len(chunk)} bytes, need {MESSAGE_SIZE}).")
                        continue

                    data_queue.put(chunk[:MESSAGE_SIZE])

                except ValueError as e:
                    print(f"[Reader] Dropping malformed line: {e}")
                except serial.SerialException as e:
                    print(f"[Reader] Serial error: {e}")
                    stop_event.set()
                    break

    except serial.SerialException as e:
        print(f"[Reader] Failed to open port {port}: {e}")
        stop_event.set()


def data_processor(data_queue: queue.Queue, stop_event: threading.Event) -> None:
    """Processes chunks of data from the queue in a separate thread."""
    print("[Processor] Started.")
    while not stop_event.is_set() or not data_queue.empty():
        try:
            chunk = data_queue.get(timeout=1)
            process_chunk(chunk)
            data_queue.task_done()
        except queue.Empty:
            continue

    print("[Processor] Stopped.")


def process_chunk(chunk: bytes) -> None:
    global full_msg
    print(f"[Processor] Processing {len(chunk)} bytes: {chunk[:16].hex()}...")
    print()
    try:
        sensor_msg = parse_message(chunk)
        print(sensor_msg)
        display_frame_data(total_frames[sensor_msg.sequence_num])
        for frame in total_frames:
            print(frame)
    except ValueError:
        print("Wrong number of bytes")



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read 214-byte chunks from a serial port."
    )

    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument(
        "port",
        type=str,
        nargs="?",
        help="port to read from",
    )

    parser.add_argument(
        "--baud", type=int, default=115200,
        help="Baud rate (default: 115200)"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    data_queue: queue.Queue = queue.Queue()
    stop_event = threading.Event()

    reader_thread = threading.Thread(
        target=serial_reader,
        args=(args.port, args.baud, data_queue, stop_event),
        daemon=True,
        name="SerialReader",
    )

    processor_thread = threading.Thread(
        target=data_processor,
        args=(data_queue, stop_event),
        daemon=True,
        name="DataProcessor"
    )

    reader_thread.start()
    processor_thread.start()

    try:
        reader_thread.join()
    except KeyboardInterrupt:
        print("\n[Main] Interrupted by user. Shutting down...")
        stop_event.set()
    
    
    processor_thread.join(timeout=3)
    print("[Main] Shutdown complete.")


if __name__ == "__main__":
    main()
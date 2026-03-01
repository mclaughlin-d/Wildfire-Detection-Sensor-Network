import argparse
import threading
import queue
import serial
import struct
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
_STRUCT_FORMAT = f">B6sHfffB{PAYLOAD_PIXEL_COUNT}H"
_STRUCT = struct.Struct(_STRUCT_FORMAT)

@dataclass
class SensorMessage:
    module_id: int          # 4 bits
    sequence_num: int       # 4 bits 
    row_sequence: int       # 1 byte  - uint8 (0-7)
    timestamp: str          # 6 bytes - "HH:MM:SS" (UTC), stored as XX:XX:XX
    gas_sensor: int         # 2 bytes - uint16 (0-4096)
    temperature: float      # 4 bytes - float
    humidity: float         # 4 bytes - float
    pack_voltage: float     # 4 bytes - float
    payload: list[int]      # 192 bytes - 96 x uint16 pixel values

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
        data: Exactly 214 bytes of raw message data.

    Returns:
        A populated SensorMessage instance.

    Raises:
        ValueError: If data is not exactly 214 bytes.
        struct.error: If unpacking fails due to malformed data.
    """
    if len(data) != MESSAGE_SIZE:
        raise ValueError(f"Expected {MESSAGE_SIZE} bytes, got {len(data)}")

    unpacked = _STRUCT.unpack(data)

    module_id    = unpacked[0] & 0x0F
    sequence_num = (unpacked[0] & 0xF0) >> 4
    row_sequence = unpacked[1]
    timestamp    = parse_timestamp(unpacked[2])
    gas_sensor   = unpacked[3]
    temperature  = unpacked[4]
    humidity     = unpacked[5]
    pack_voltage = unpacked[6]
    payload      = list(unpacked[7:])  # 96 uint16 pixel values

    return SensorMessage(
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

def uint16_to_float16(values: list[int]) -> list[float]:
    raw_array = np.array(values, dtype=np.uint16)
    float_array = raw_array.view(np.float16).astype(np.float32)
    return float_array


plt.ion()
fig, ax = plt.subplots(figsize=(12, 7))
therm1 = ax.imshow(np.zeros((24, 32)), vmin=0, vmax=60)
cbar = fig.colorbar(therm1)
cbar.set_label('Temperature [$^{\circ}$C]', fontsize=14)

def display_frame_data(frame_data: list[int]):
    # convert from raw values to 16-bit floats, to 32-bit floats
    float_values = uint16_to_float16(frame_data)
    # print colors!
    data_array = np.reshape(float_values, (24, 32))
    therm1.set_data(np.fliplr(data_array))
    therm1.set_clim(vmin=np.min(data_array), vmax=np.max(data_array))
    fig.canvas.draw()  # Redraw the figure to update the plot and colorbar
    fig.canvas.flush_events()


def serial_reader(port: str, baud_rate: int, data_queue: queue.Queue, stop_event: threading.Event) -> None:
    """Reads data from a COM port in chunks and places them into a queue."""
    try:
        with serial.Serial(port, baudrate=baud_rate, timeout=1) as ser:
            print(f"[Reader] Opened {port} at {baud_rate} baud.")
            buffer = bytearray()

            while not stop_event.is_set():
                try:
                    incoming = ser.read(MESSAGE_SIZE - len(buffer))
                    if incoming:
                        buffer.extend(incoming)

                    while len(buffer) >= MESSAGE_SIZE:
                        chunk = bytes(buffer[:MESSAGE_SIZE])
                        buffer = buffer[MESSAGE_SIZE:]
                        data_queue.put(chunk)
                        print(f"[Reader] Queued chunk of {MESSAGE_SIZE} bytes.")

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
    """Placeholder for actual data processing logic."""
    print(f"[Processor] Processing {len(chunk)} bytes: {chunk[:16].hex()}...")
    print()
    sensor_msg = parse_message(chunk)
    print(sensor_msg)

    display_frame_data(sensor_msg.payload)

    # TODO print thermal caemra reading


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Read 214-byte chunks from a Windows COM port.")
    parser.add_argument("port", type=str, help="COM port to read from (e.g. COM3)")
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
        name="SerialReader"
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

    reader_thread.join(timeout=3)
    processor_thread.join(timeout=3)
    print("[Main] Shutdown complete.")


if __name__ == "__main__":
    main()
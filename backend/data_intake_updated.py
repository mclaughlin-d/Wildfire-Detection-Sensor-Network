import argparse
import threading
import queue
import requests
import serial
import struct
import time
<<<<<<< HEAD
import numpy as np
import datetime
import matplotlib.pyplot as plt
=======
>>>>>>> d7e0e1f (frontend updates)
from dataclasses import dataclass
from fire_confidence import compute_fire_confidence
 

MESSAGE_SIZE = 214
PAYLOAD_PIXEL_COUNT = 96

_STRUCT_FORMAT = f"<BBHHHHfff{PAYLOAD_PIXEL_COUNT}H"
_STRUCT = struct.Struct(_STRUCT_FORMAT)

total_frames = [[[0] * 32 for _ in range(24)] for _ in range(4)]  # 4 thermal camera angles (0-3)
last_message = None

#added
API_BASE_URL = "http://localhost:5001/api"
_MODULE_ID_MAP: dict[int, str] = {i: f"mod-{i:03d}" for i in range(1, 10)}

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


def parse_message(data: bytes) -> SensorMessage:
    """Parse a 214-byte sensor message into a SensorMessage."""
    global last_message, total_frames
    if len(data) != MESSAGE_SIZE:
        raise ValueError(f"Expected {MESSAGE_SIZE} bytes, got {len(data)}")
    
    unpacked = _STRUCT.unpack(data)
    module_id = data[0] & 0x0F
    sequence_num = (data[0] & 0xF0) >> 4
    row_sequence = unpacked[1]
    timestamp = f"{unpacked[2]:02d}:{unpacked[3]:02d}:{unpacked[4]:02d}"
    gas_sensor = unpacked[5]
    temperature = unpacked[6]
    humidity = unpacked[7]
    pack_voltage = unpacked[8]
    payload = list(unpacked[9:])
    
    if last_message is not None and sequence_num != last_message.sequence_num:
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


def serial_reader(port: str, baud_rate: int, data_queue: queue.Queue, stop_event: threading.Event) -> None:
    """Reads data from a COM port in chunks and places them into a queue."""
    try:
        with serial.Serial(port, baudrate=baud_rate, timeout=1) as ser:
            print(f"[Reader] Opened {port} at {baud_rate} baud.")
            ser.flush()

            while not stop_event.is_set():
                try:
                    line = ser.readline().decode('ascii', errors='replace').strip()
                    if not line.startswith("Data:"):
                        continue

                    hex_str = line[len("Data:"):].strip()
                    chunk = bytes.fromhex(hex_str)

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


def _post_reading(sensor_msg: SensorMessage, fire_confidence: float) -> None:
    module_id = _MODULE_ID_MAP.get(sensor_msg.module_id, f"mod-{sensor_msg.module_id:03d}")
    reading = {
        "module_id":      module_id,
        "sequence_num":   sensor_msg.sequence_num,
        "row_sequence":   sensor_msg.row_sequence,
        "timestamp":      sensor_msg.timestamp,
        "temperature":    round(float(sensor_msg.temperature),  4),
        "humidity":       round(float(sensor_msg.humidity),     4),
        "gas_sensor":     int(sensor_msg.gas_sensor),
        "pack_voltage":   round(float(sensor_msg.pack_voltage), 4),
        "fire_confidence": fire_confidence,
        "payload":        sensor_msg.payload,

    }
    try:
        resp = requests.post(f"{API_BASE_URL}/readings", json=reading, timeout=5)
        if resp.status_code == 201:
            print(f"[API] Posted reading for {module_id}")
        else:
            print(f"[API] POST returned {resp.status_code}")
    except Exception as e:
        print(f"[API] POST error: {e}")
        

def process_chunk(chunk: bytes) -> None:
    print(f"[Processor] Processing {len(chunk)} bytes: {chunk[:16].hex()}...")
    try:
        sensor_msg = parse_message(chunk)
        fire_confidence = compute_fire_confidence(
            sensor_msg.temperature,
            sensor_msg.humidity,
            sensor_msg.gas_sensor,
        )
        print(sensor_msg)
        print(f"fire_confidence={fire_confidence:.4f}")
        _post_reading(sensor_msg, fire_confidence) 
    except ValueError:
        print("Wrong number of bytes")



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Read 214-byte chunks from a serial port.")
    parser.add_argument("port", type=str, help="port to read from")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    return parser.parse_args()

def main() -> None:
    args = parse_args()
    data_queue = queue.Queue()
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
        name="DataProcessor",
    )

    reader_thread.start()
    processor_thread.start()

    # Main thread owns the plot update loop
    try:
        while reader_thread.is_alive():
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n[Main] Interrupted by user. Shutting down.")
    finally:
        stop_event.set()

if __name__ == "__main__":
    main()
import argparse
import threading
import queue
import requests
import serial
import struct
import time
import numpy as np
import matplotlib.pyplot as plt
from dataclasses import dataclass
from fire_confidence import compute_fire_confidence
 

MESSAGE_SIZE = 214
PAYLOAD_PIXEL_COUNT = 96

MESSAGE_STRUCT_FMT = f"<BBHHHHfff{PAYLOAD_PIXEL_COUNT}H"
MESSAGE_STRUCT = struct.Struct(MESSAGE_STRUCT_FMT)

# stores last 4 fragments for a given reading sequence
total_frames = [[[0] * 32 for _ in range(24)] for _ in range(4)]
# stores the last full message (SensorMessage instance)
last_message = None

# for posting to the database
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


"""Parse a 214-byte sensor message into a SensorMessage."""
def parse_message(data: bytes) -> SensorMessage:
    global last_message, total_frames
    if len(data) != MESSAGE_SIZE:
        raise ValueError(f"Expected {MESSAGE_SIZE} bytes, got {len(data)}")
    
    unpacked = MESSAGE_STRUCT.unpack(data)
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


"""Helper function to convert from 16-bit float (in int form) to actual float"""
def uint16_to_float16(values: list[int]):
    raw_array = np.array(values, dtype=np.uint16)
    return raw_array.view(np.float16).astype(np.float32)


"""Displays thermal camera data"""
def display_frame_data(therm1, fig):
    if last_message is None:
        return

    # must flip and rotate frames for display
    frame0 = np.rot90(np.flip(np.array(total_frames[0])))
    frame1 = np.rot90(np.flip(np.array(total_frames[1])))
    frame2 = np.rot90(np.flip(np.array(total_frames[2])))
    frame3 = np.rot90(np.flip(np.array(total_frames[3])))

    overall_data = np.hstack((frame3, frame2, frame1, frame0))
    overall_data = overall_data.flatten()

    float_values = uint16_to_float16(overall_data)
    data_array = np.reshape(float_values, (32, 24 * 4))
    therm1.set_data(np.fliplr(data_array))
    therm1.set_clim(vmin=np.min(data_array), vmax=np.max(data_array))

    fig.canvas.draw()
    fig.canvas.flush_events()


"""
Serial reader thread, which continuously reads data from serial port
    Assumes each message starts with 'Data: ' and ends in a newline '\n'
"""
def serial_reader(port: str, baud_rate: int, data_queue: queue.Queue, stop_event: threading.Event) -> None:
    """Reads data from a serial port in chunks and places them into a queue."""
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


"""For the processing thread, which takes in bytes objects from the data queue"""
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


"""Posts a given reading to the database"""
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
        

"""Takes in a bytes object (assumes 214-byte message from sensor module) and parses the information"""
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


def main() -> None:
    parser = argparse.ArgumentParser(description="Read 214-byte chunks from a serial port.")
    parser.add_argument("port", type=str, help="port to read from")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    
    data_queue = queue.Queue()
    stop_event = threading.Event()

    # This has to be created here (in the main thread) so tk does not error
    fig, ax = plt.subplots(figsize=(12, 7))
    therm1 = ax.imshow(np.zeros((32, 24 * 4)), vmin=0, vmax=40)
    cbar = fig.colorbar(therm1)
    cbar.set_label('Temperature [$^{\circ}$C]', fontsize=14)
    plt.ion()
    plt.show()
    
    # this thread reads data from the serial port continuously
    # it puts bytes into the data queue created at the top of this function
    reader_thread = threading.Thread(
        target=serial_reader,
        args=(args.port, args.baud, data_queue, stop_event),
        daemon=True,
        name="SerialReader",
    )
    # this thread parses messages from the bytes put in data_queue, posts the corresponding readings,
    # and displays the thermal camera data
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
            display_frame_data(therm1, fig)
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n[Main] Interrupted by user. Shutting down.")
    finally:
        stop_event.set()

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""Generate mock sensor readings and post to API"""

import random
import struct
import sys
import time
from datetime import datetime

import requests
from fire_confidence import compute_fire_confidence


API_BASE_URL = "http://localhost:5001/api"
MOCK_INTERVAL = 10  # seconds between readings
MODULES = [
    "mod-001", "mod-005", "mod-009",
]

TEMP_MIN,      TEMP_MAX      = -2.0, 12.0
HUMIDITY_MIN,  HUMIDITY_MAX  = 35.0,  55.0
GAS_MIN,       GAS_MAX       = 600,   1400
VOLTAGE_MIN,   VOLTAGE_MAX   = 10.95,   15.0
SEQUENCE_MIN,  SEQUENCE_MAX  = 0,     3   # 4 thermal camera angles (0-3)
ROW_SEQ_MIN,   ROW_SEQ_MAX   = 0,     7   # 8 row groups per frame (3 rows each = 24 rows)
PAYLOAD_COUNT = 96                        # uint16 pixel values per message

FRAME_HEIGHT = 24
FRAME_WIDTH = 32

_module_state: dict[str, dict] = {
    module_id: {
        "sequence_num": SEQUENCE_MIN,
        "row_sequence": ROW_SEQ_MIN,
        "cycle_base_temp": random.uniform(2.0, 8.0),
        "frames": {},
    }
    for module_id in MODULES
}


def _float_to_uint16(value: float) -> int:
    return struct.unpack("<H", struct.pack("<e", value))[0]


def _generate_thermal_frame(base_temp: float) -> list[list[float]]:
    frame: list[list[float]] = []
    for _ in range(FRAME_HEIGHT):
        row: list[float] = []
        for _ in range(FRAME_WIDTH):
            temp_c = base_temp + random.uniform(-0.6, 0.6)
            row.append(max(TEMP_MIN, min(TEMP_MAX, temp_c)))
        frame.append(row)
    return frame


def _next_sequence_row(module_id: str) -> tuple[int, int]:
    state = _module_state[module_id]
    sequence_num = state["sequence_num"]
    row_sequence = state["row_sequence"]

    if row_sequence < ROW_SEQ_MAX:
        state["row_sequence"] += 1
    else:
        state["row_sequence"] = ROW_SEQ_MIN
        if sequence_num < SEQUENCE_MAX:
            state["sequence_num"] += 1
        else:
            state["sequence_num"] = SEQUENCE_MIN

    return sequence_num, row_sequence


def _payload_from_frame(frame: list[list[float]], row_sequence: int) -> list[int]:
    start_row = row_sequence * 3
    payload: list[int] = []
    for row in range(start_row, start_row + 3):
        for col in range(FRAME_WIDTH):
            payload.append(_float_to_uint16(frame[row][col]))
    return payload


def _generate_mock_reading(module_id: str) -> dict:
    state = _module_state[module_id]
    sequence_num, row_sequence = _next_sequence_row(module_id)

    if sequence_num == SEQUENCE_MIN and row_sequence == ROW_SEQ_MIN:
        state["cycle_base_temp"] = random.uniform(2.0, 8.0)

    if row_sequence == ROW_SEQ_MIN or sequence_num not in state["frames"]:
        sequence_offset = (sequence_num - 1.5) * 0.12
        state["frames"][sequence_num] = _generate_thermal_frame(
            state["cycle_base_temp"] + sequence_offset
        )

    frame = state["frames"][sequence_num]
    payload = _payload_from_frame(frame, row_sequence)

    start_row = row_sequence * 3
    packet_temps = frame[start_row] + frame[start_row + 1] + frame[start_row + 2]

    now = datetime.utcnow()
    temperature = round(sum(packet_temps) / len(packet_temps), 4)
    humidity = round(random.uniform(HUMIDITY_MIN, HUMIDITY_MAX), 4)
    gas_sensor = random.randint(GAS_MIN, GAS_MAX)
    pack_voltage = round(random.uniform(VOLTAGE_MIN, VOLTAGE_MAX), 4)
    fire_confidence = compute_fire_confidence(temperature, humidity, gas_sensor)

    return {
        "module_id":    module_id,
        "sequence_num": sequence_num,
        "row_sequence": row_sequence,
        "timestamp":    now.strftime("%H:%M:%S"),
        "temperature":  temperature,
        "humidity":     humidity,
        "gas_sensor":   gas_sensor,
        "pack_voltage": pack_voltage,
        "fire_confidence": fire_confidence,
        "payload":      payload,
    }


def run_mock() -> None:
    print(f"[mock] API: {API_BASE_URL}")
    print(f"[mock] Interval: {MOCK_INTERVAL}s  |  Modules: {', '.join(MODULES)}\n")
    total = 0
    try:
        while True:
            for module_id in MODULES:
                reading = _generate_mock_reading(module_id)
                ok = _post_reading(reading)
                ts = datetime.now().strftime("%H:%M:%S")
                if ok:
                    total += 1
                    print(
                        f"[{ts}] ✓ {module_id}: "
                        f"T={reading['temperature']}°C  "
                        f"H={reading['humidity']}%  "
                        f"G={reading['gas_sensor']}  "
                        f"V={reading['pack_voltage']}V  "
                        f"F={reading['fire_confidence']}  "
                        f"seq={reading['sequence_num']}  row={reading['row_sequence']}"
                    )
                else:
                    print(f"[{ts}] ✗ Failed to post for {module_id}")
            print(f"   (total sent: {total})\n")
            time.sleep(MOCK_INTERVAL)
    except KeyboardInterrupt:
        print(f"\n[mock] Stopped. Readings sent: {total}")
        sys.exit(0)


def _post_reading(reading: dict) -> bool:
    try:
        resp = requests.post(
            f"{API_BASE_URL}/readings",
            json=reading,
            timeout=5,
        )
        return resp.status_code == 201
    except Exception as exc:
        print(f"[api] POST error: {exc}")
        return False

def main() -> None:
    run_mock()


if __name__ == "__main__":
    main()

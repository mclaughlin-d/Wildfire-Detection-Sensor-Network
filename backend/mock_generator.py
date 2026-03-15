#!/usr/bin/env python3
"""Generate mock sensor readings and post to API"""

import random
import sys
import time
from datetime import datetime

import requests


API_BASE_URL = "http://localhost:5001/api"
MOCK_INTERVAL = 10  # seconds between readings
MODULES = [
    "mod-001", "mod-002", "mod-003", "mod-004", "mod-005",
    "mod-006", "mod-007", "mod-008", "mod-009",
]

TEMP_MIN,    TEMP_MAX    = 15.0,  35.0
HUMIDITY_MIN, HUMIDITY_MAX = 20.0, 80.0
GAS_MIN,     GAS_MAX     = 0,    4096
VOLTAGE_MIN, VOLTAGE_MAX = 1.0,   4.0


def _generate_mock_reading(module_id: str) -> dict:
    return {
        "module_id":   module_id,
        "temperature": round(random.uniform(TEMP_MIN,    TEMP_MAX),    2),
        "humidity":    round(random.uniform(HUMIDITY_MIN, HUMIDITY_MAX), 2),
        "gas_raw":     random.randint(GAS_MIN, GAS_MAX),
        "pack_voltage": round(random.uniform(VOLTAGE_MIN, VOLTAGE_MAX), 2),
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
                        f"V={reading['pack_voltage']}V"
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

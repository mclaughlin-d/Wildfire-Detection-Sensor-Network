#!/usr/bin/env python3

import time
import random
import requests
import sys
from datetime import datetime

API_BASE_URL = "http://localhost:5001/api"
READING_INTERVAL = 3 #length between readings
MODULES = [
    "mod-001", "mod-002", "mod-003", "mod-004", "mod-005",
    "mod-006", "mod-007", "mod-008", "mod-009"
]

#ranges for sensor values
TEMP_MIN, TEMP_MAX = 15.0, 35.0
HUMIDITY_MIN, HUMIDITY_MAX = 20.0, 80.0
GAS_MIN, GAS_MAX = 0, 4096
VOLTAGE_MIN, VOLTAGE_MAX = 1.0, 4.0

def generate_reading(module_id):
    return {
        "module_id": module_id,
        "temperature": round(random.uniform(TEMP_MIN, TEMP_MAX), 2),
        "humidity": round(random.uniform(HUMIDITY_MIN, HUMIDITY_MAX), 2),
        "gas_raw": random.randint(GAS_MIN, GAS_MAX),
        "pack_voltage": round(random.uniform(VOLTAGE_MIN, VOLTAGE_MAX), 2),
    }

def post_reading(reading):
    try:
        response = requests.post(
            f"{API_BASE_URL}/readings",
            json=reading,
            timeout=5
        )
        return response.status_code == 201
    except Exception as e:
        print(f"✗ Error posting reading: {e}")
        return False

def main():
    print(f"starting...")
    print(f"API: {API_BASE_URL}")
    print(f"Interval: {READING_INTERVAL}s")
    print(f"Modules: {', '.join(MODULES)}")
    
    reading_count = 0
    
    try:
        while True:
            for module_id in MODULES:
                reading = generate_reading(module_id)
                if post_reading(reading):
                    reading_count += 1
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    print(f"[{timestamp}] ✓ {module_id}: T={reading['temperature']}°C, H={reading['humidity']}%, V={reading['pack_voltage']}V")
                else:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    print(f"[{timestamp}] ✗ Failed to post reading for {module_id}")
            
            print(f"   (Total sent: {reading_count})\n")
            time.sleep(READING_INTERVAL)
    
    except KeyboardInterrupt:
        print(f"\n\nreadings sent: {reading_count}")
        sys.exit(0)

if __name__ == "__main__":
    main()

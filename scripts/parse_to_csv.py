import re
import csv
import sys

def parse_fuzzy_output(text: str) -> list[dict]:
    records = []

    pattern = re.compile(
        r"Calling with temp\s*([\d.]+)\s*C,\s*hum\s*([\d.]+)%,\s*gas\s*([\d.]+)\s*ppm\s*\n"
        r"Temp:\s*(\d+)\s*([\d.]+)\s*\n"
        r"Hum:\s*(\d+)\s*([\d.]+)\s*\n"
        r"Gas:\s*(\d+)\s*([\d.]+)\s*\n"
        r"Z\s*([\d.]+)"
    )

    for m in pattern.finditer(text):
        records.append({
            "temp_c":         float(m.group(1)),
            "hum_pct":        float(m.group(2)),
            "gas_ppm":        int(m.group(3)),
            "temp_class":     int(m.group(4)),
            "temp_membership":float(m.group(5)),
            "hum_class":      int(m.group(6)),
            "hum_membership": float(m.group(7)),
            "gas_class":      int(m.group(8)),
            "gas_membership": float(m.group(9)),
            "confidence":     float(m.group(10)),
        })

    return records


def write_csv(records: list[dict], output_path: str):
    if not records:
        print("No records found.")
        return

    with open(output_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=records[0].keys())
        writer.writeheader()
        writer.writerows(records)

    print(f"Wrote {len(records)} records to {output_path}")


if __name__ == "__main__":
    input_file  = sys.argv[1] if len(sys.argv) > 1 else "output.txt"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "results.csv"

    with open(input_file, "r") as f:
        text = f.read()

    records = parse_fuzzy_output(text)
    write_csv(records, output_file)
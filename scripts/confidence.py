
from parsedata import SensorMessage
import numpy as np


"""
Intro to fuzzification:
https://www.geeksforgeeks.org/artificial-intelligence/fuzzy-logic-introduction/

define triangles for each classification label, for each reading basically
"""

# temperature triangles
TEMP_VERY_LOW = [-float('inf'), 0, 30]
TEMP_LOW = [20.5, 22, 40]
TEMP_MEDIUM = [25, 40, 55]
TEMP_HIGH = [40, 55, 70]
TEMP_VERY_HIGH = [55, 70, float('inf')]
temp_memberships = [TEMP_VERY_LOW, TEMP_LOW, TEMP_MEDIUM, TEMP_HIGH, TEMP_VERY_HIGH]

# gas triangles
GAS_VERY_LOW = [-float('inf'), 0, 50]
GAS_LOW = [25, 60, 100]
GAS_MEDIUM = [51, 130, 200]
GAS_HIGH = [101, 200, 300]
GAS_VERY_HIGH = [290, 300, float('inf')]
gas_memberships = [GAS_VERY_LOW, GAS_LOW, GAS_MEDIUM, GAS_HIGH, GAS_VERY_HIGH]

# humidity triangles - percent
HUMIDITY_VERY_LOW = [80, 90, float('inf')]
HUMIDITY_LOW = [60, 70, 80]
HUMIDITY_MEDIUM = [50, 60, 65]
HUMIDITY_HIGH = [22, 40, 50]
HUMIDITY_VERY_HIGH = [float('-inf'), 10, 25]
hum_memberships = [HUMIDITY_VERY_LOW, HUMIDITY_LOW, HUMIDITY_MEDIUM, HUMIDITY_HIGH, HUMIDITY_VERY_HIGH]

def calc_membership(triangle: list[float], value: float) -> float:
    """Return fuzzy membership of value in a triangular set [left, center, right].

    The center has membership 1.0; left and right have membership 0.0, with
    linear interpolation between. Infinite endpoints create open-ended shoulders:
      - left == -inf  → membership is 1.0 for all values <= center
      - right == inf  → membership is 1.0 for all values >= center
    """
    left, center, right = triangle

    if value <= left or value >= right:
        # Closed endpoints: outside the triangle entirely
        if left != -float('inf') and value <= left:
            return 0.0
        if right != float('inf') and value >= right:
            return 0.0

    if value <= center:
        if left == -float('inf'):
            return 1.0
        return (value - left) / (center - left)
    else:
        if right == float('inf'):
            return 1.0
        return (right - value) / (right - center)

def fuzzify_and_detuz(temp, hum, gas):
    temp_classes = [calc_membership(t, temp) for t in temp_memberships]
    hum_classes = [calc_membership(t, hum) for t in hum_memberships]
    gas_classes = [calc_membership(t, gas) for t in gas_memberships]

    temp_result = np.argmax(temp_classes)
    hum_result = np.argmax(hum_classes)
    gas_result = np.argmax(gas_classes)

    print(f"Temp: {temp_result} {temp_classes[temp_result]}")
    print(f"Hum: {hum_result} {hum_classes[hum_result]}")
    print(f"Gas: {gas_result} {gas_classes[gas_result]}")

    Z = (temp_result + 1) * temp_classes[temp_result] + (hum_result + 1) * hum_classes[hum_result] + (gas_result + 1) * gas_classes[gas_result]
    print(f"Z {Z/3}")

def run_random_tests():
    for _ in range(100):
        temp = np.random.normal(loc=20, scale=np.sqrt(5))
        hum = np.random.uniform(0, 100)
        gas = np.random.uniform(0, 4096)
        print(f"Calling with temp {temp} C, hum {hum}%, gas {gas} ppm")
        fuzzify_and_detuz(temp, hum, gas)
        print()

    # confirm range is 1-5
    print("Testing min")
    fuzzify_and_detuz(0, 100, 0)

    print()
    print("Testing max")
    fuzzify_and_detuz(float('inf'), 0, float('inf'))

run_random_tests()
"""
https://ieeexplore.ieee.org/document/6571347
Fuzzy logic paper (original)

https://ieeexplore.ieee.org/document/9166416
Indoor detection paper which uses this logic

idea: each measurement mapped to label: VL, L, M, H, VH (1-5)

"""
# def compute_confidence(message: SensorMessage):
#     pass
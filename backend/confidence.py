
import numpy as np


"""
Intro to fuzzification:
https://www.geeksforgeeks.org/artificial-intelligence/fuzzy-logic-introduction/

define triangles for each classification label, for each reading basically

https://ieeexplore.ieee.org/document/6571347
Fuzzy logic paper (original)

https://ieeexplore.ieee.org/document/9166416
Indoor detection paper which uses this logic

idea: each measurement mapped to label: VL, L, M, H, VH (1-5)

"""

# temperature triangles
TEMP_VERY_LOW = [-float('inf'), 0, 30]
TEMP_LOW = [20.5, 22, 40]
TEMP_MEDIUM = [25, 40, 55]
TEMP_HIGH = [40, 55, 70]
TEMP_VERY_HIGH = [55, 70, float('inf')]
temp_memberships = [TEMP_VERY_LOW, TEMP_LOW, TEMP_MEDIUM, TEMP_HIGH, TEMP_VERY_HIGH]

# gas triangles
GAS_VERY_LOW = [-float('inf'), 0, 400]
GAS_LOW = [300, 500, 590]
GAS_MEDIUM = [560, 575, 640]
GAS_HIGH = [620, 660, 700]
GAS_VERY_HIGH = [680, 770, float('inf')]
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

def get_confidence(temp, hum, gas):
    temp_classes = [calc_membership(t, temp) for t in temp_memberships]
    hum_classes = [calc_membership(t, hum) for t in hum_memberships]
    gas_classes = [calc_membership(t, gas) for t in gas_memberships]

    indices = np.array([1, 2, 3, 4, 5])
    temp_z = np.dot(indices, temp_classes) / np.sum(temp_classes)
    hum_z = np.dot(indices, hum_classes) / np.sum(hum_classes)
    gas_z = np.dot(indices, gas_classes) / np.sum(gas_classes)

    Z = (temp_z) * 0.2 + (hum_z) * 0.1 + (gas_z) * 0.7
    Z = np.clip(Z, 1, 5)

    return Z
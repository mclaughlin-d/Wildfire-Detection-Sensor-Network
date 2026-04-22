"""
"Membership functions" for each reading.
Note that these were tuned using ambient temperature for thermal data, not the thermal camera temperature.
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
GAS_LOW = [300, 500, 550]
GAS_MEDIUM = [530, 575, 650]
GAS_HIGH = [630, 670, 720]
GAS_VERY_HIGH = [700, 850, float('inf')]
gas_memberships = [GAS_VERY_LOW, GAS_LOW, GAS_MEDIUM, GAS_HIGH, GAS_VERY_HIGH]

# humidity triangles (Relative humidity: % value)
HUMIDITY_VERY_LOW = [80, 90, float('inf')]
HUMIDITY_LOW = [60, 70, 80]
HUMIDITY_MEDIUM = [50, 60, 65]
HUMIDITY_HIGH = [22, 40, 50]
HUMIDITY_VERY_HIGH = [float('-inf'), 10, 25]
hum_memberships = [HUMIDITY_VERY_LOW, HUMIDITY_LOW, HUMIDITY_MEDIUM, HUMIDITY_HIGH, HUMIDITY_VERY_HIGH]

"""
Return fuzzy membership of value in a triangular set [left, center, right].
    Essentially, this function calculates the value of a function of the form:
    f(x)
    1_| 
      |   /\ 
      ---/  \-------> x
    
    Where the first endpoint of the triangle is at x=left, the vertex is at x=center, 
    and the second endpoint is at x=right
"""
def calc_membership(triangle, value):
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


"""Calculates the confidence given the intervals defined in this file and the three measurements."""
def confidence_from_measurements(temp, hum, gas):
    temp_classes = [calc_membership(t, temp) for t in temp_memberships]
    hum_classes = [calc_membership(t, hum) for t in hum_memberships]
    gas_classes = [calc_membership(t, gas) for t in gas_memberships]

    temp_result = max(range(len(temp_classes)), key=lambda i: temp_classes[i])
    hum_result = max(range(len(hum_classes)), key=lambda i: hum_classes[i])
    gas_result = max(range(len(gas_classes)), key=lambda i: gas_classes[i])

    z = (
        (temp_result + 1) * temp_classes[temp_result] * 0.1
        + (hum_result + 1) * hum_classes[hum_result] * 0.1
        + (gas_result + 1) * gas_classes[gas_result] * 0.8
    )

    return z


def compute_fire_confidence(temperature, humidity, gas_sensor):
    return confidence_from_measurements(temperature, humidity, gas_sensor)

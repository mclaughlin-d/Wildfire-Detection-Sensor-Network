import skfuzzy as fuzz
import numpy as np


LABELS = {
    "very low": 1,
    "low": 2,
    "medium": 3,
    "high": 4,
    "very high": 5
}

class GaussMembership:
    def __init__(self, label: str, mean: float, sigma: float) -> None:
        self.label = label
        self.mean = mean
        self.sigma = sigma

TEMP_MEMBERSHIPS = [
    GaussMembership("very low", )
]

HUM_MEMBERSHIPS = [

]

GAS_MEMBERSHIPS = [

]
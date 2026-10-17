"""Built-in regression coefficients for the MQ sensor families.

The values are the published `a` / `b` pairs of the MQUnifiedsensor PPM model
(`PPM = a * ratio^b` with ``ratio = RS / R0``), from ``sensorConfigData.h`` of
the SolderedElectronics MQ library, cross-checked against the per-sensor tables
in the ``examples/`` of miguel5612/MQSensorsLib.

Everything here is resolved at code generation time, so a wrong ``sensor_type`` /
``gas`` combination fails the build instead of silently producing garbage at
runtime.
"""

from dataclasses import dataclass
import math

MQ_MAX_A = 1e30
MQ_MAX_B = 100.0


@dataclass(frozen=True)
class ResolvedSensor:
    """Fully resolved sensor configuration handed to the C++ component."""

    type_key: str
    label: str
    gas: str
    method: str
    a: float
    b: float
    ratio_in_clean_air: float
    rl: float
    vcc: float
    min_ppm: float
    max_ppm: float


def normalize_type(value: str) -> str:
    """``"mq-8"`` / ``"MQ_8"`` / ``"MQ8"`` -> ``"MQ8"``."""
    return str(value).strip().upper().replace("-", "").replace("_", "").replace(" ", "")


def normalize_gas(value: str) -> str:
    """``"h2"`` / ``"H-2"`` -> ``"H2"``; ``"ethyl alcohol"`` -> ``"ETHYL_ALCOHOL"``."""
    return str(value).strip().upper().replace("-", "_").replace(" ", "_")


def sensor_types() -> list[str]:
    """Canonical type keys, e.g. ``["MQ2", ..., "MQ138"]``."""
    return list(SENSOR_TYPES)


def label_for(type_key: str) -> str:
    return SENSOR_TYPES[type_key]["label"]


# ---------------------------------------------------------------------------
# type key -> label, method, RS/R0 in clean air, primary gas, max ppm, gases
# (gas -> (a, b) regression coefficients)
# ---------------------------------------------------------------------------
SENSOR_TYPES: dict[str, dict] = {
    "MQ2": {
        "label": "MQ-2",
        "method": "exponential",
        "ratio_in_clean_air": 9.83,
        "primary_gas": "LPG",
        "max_ppm": 10000.0,
        "gases": {  # combustible gas / smoke
            "LPG": (574.25, -2.222),
            "H2": (987.99, -2.162),
            "CH4": (36974.0, -3.109),
            "CO": (36974.0, -3.109),
            "ALCOHOL": (3616.1, -2.675),
            "PROPANE": (658.71, -2.168),
            "SMOKE": (1000.5, -2.186),
        },
    },
    "MQ3": {
        "label": "MQ-3",
        "method": "exponential",
        "ratio_in_clean_air": 60.0,
        "primary_gas": "ALCOHOL",
        "max_ppm": 10000.0,
        "gases": {  # alcohol
            "LPG": (44771.0, -3.245),
            "CH4": (2e31, 19.01),
            "CO": (521853.0, -3.821),
            "ALCOHOL": (0.3934, -1.504),
            "BENZENE": (4.8387, -2.68),
            "HEXANE": (7585.3, -2.849),
        },
    },
    "MQ4": {
        "label": "MQ-4",
        "method": "exponential",
        "ratio_in_clean_air": 4.4,
        "primary_gas": "CH4",
        "max_ppm": 10000.0,
        "gases": {  # methane / natural gas
            "LPG": (3811.9, -3.113),
            "CH4": (1012.7, -2.786),
            "CO": (2e14, -19.05),
            "ALCOHOL": (6e10, -14.01),
            "SMOKE": (3e7, -8.308),
        },
    },
    "MQ5": {
        "label": "MQ-5",
        "method": "exponential",
        "ratio_in_clean_air": 6.5,
        "primary_gas": "LPG",
        "max_ppm": 10000.0,
        "gases": {  # LPG / natural gas
            "LPG": (80.897, -2.431),
            "CH4": (177.65, -2.56),
            "CO": (491204.0, -5.826),
            "H2": (1163.8, -3.874),
            "ALCOHOL": (97124.0, -4.918),
        },
    },
    "MQ6": {
        "label": "MQ-6",
        "method": "exponential",
        "ratio_in_clean_air": 10.0,
        "primary_gas": "LPG",
        "max_ppm": 10000.0,
        "gases": {  # LPG / iso-butane
            "LPG": (1009.2, -2.35),
            "H2": (88158.0, -3.597),
            "CH4": (2127.2, -2.526),
            "CO": (1e15, -13.5),
            "ALCOHOL": (5e7, -6.017),
        },
    },
    "MQ7": {
        "label": "MQ-7",
        "method": "exponential",
        "ratio_in_clean_air": 27.5,
        "primary_gas": "CO",
        "max_ppm": 2000.0,  # datasheet: 20 - 2000 ppm CO
        "gases": {  # carbon monoxide
            "CO": (99.042, -1.518),
            "H2": (69.014, -1.374),
            "LPG": (7e8, -7.703),
            "CH4": (6e13, -10.54),
            "ALCOHOL": (4e16, -12.35),
        },
    },
    "MQ8": {
        "label": "MQ-8",
        "method": "exponential",
        "ratio_in_clean_air": 70.0,  # RatioMQ8CleanAir of the reference examples
        "primary_gas": "H2",
        "max_ppm": 10000.0,  # datasheet: 100 - 10000 ppm H2
        "gases": {  # hydrogen
            "H2": (976.97, -0.688),
            "LPG": (1e7, -3.123),
            "CH4": (8e13, -6.666),
            "CO": (2e18, -8.074),
            "ALCOHOL": (76101.0, -1.86),
        },
    },
    "MQ9": {
        "label": "MQ-9",
        "method": "exponential",
        "ratio_in_clean_air": 9.6,
        "primary_gas": "LPG",
        "max_ppm": 10000.0,
        "gases": {  # CO / combustible gas
            "LPG": (1000.5, -2.186),
            "CH4": (4269.6, -2.648),
            "CO": (599.65, -2.244),
        },
    },
    "MQ131": {
        "label": "MQ-131",
        "method": "linear",
        "ratio_in_clean_air": 1.0,
        "primary_gas": "O3",
        "max_ppm": 10.0,  # datasheet range is given in ppb (10 - 1000 ppb)
        "gases": {  # ozone
            "O3": (0.41195, -0.4708),
        },
    },
    "MQ135": {
        "label": "MQ-135",
        "method": "linear",
        "ratio_in_clean_air": 1.0,
        "primary_gas": "NH3",
        "max_ppm": 10000.0,
        "gases": {  # air quality
            "NH3": (-0.47712, 0.4491),
            "H2": (-0.5101, 0.31988),
            "TOLUENE": (-0.21779, -0.23269),
        },
    },
    "MQ137": {
        "label": "MQ-137",
        "method": "linear",
        "ratio_in_clean_air": 1.0,
        "primary_gas": "NH3",
        "max_ppm": 500.0,  # datasheet: 5 - 500 ppm NH3
        "gases": {  # ammonia
            "NH3": (-0.26406, -0.24143),
        },
    },
    "MQ138": {
        "label": "MQ-138",
        "method": "linear",
        "ratio_in_clean_air": 1.0,
        "primary_gas": "TOLUENE",
        "max_ppm": 1000.0,  # datasheet: 5 - 1000 ppm VOC
        "gases": {  # VOC / organic solvents
            "TOLUENE": (-0.4434, 0.15397),
            "ALCOHOL": (-0.46099, 0.0681),
            "ACETONE": (-0.52356, 0.49225),
        },
    },
}


def _clamp_coefficient(value: float, limit: float) -> float:
    """Clamp a coefficient to the range the firmware can evaluate (see `MQ_MAX_A`)."""
    if not math.isfinite(value):
        return 0.0
    return max(-limit, min(limit, value))


def resolve_sensor(
    type_key: str,
    gas: str | None = None,
    *,
    ratio_in_clean_air: float | None = None,
    rl: float | None = None,
    vcc: float | None = None,
) -> ResolvedSensor:
    """Merge the built-in curve of `type_key` / `gas` with the hardware facts.

    Raises :class:`ValueError` for an unknown type/gas combination.
    """
    if type_key not in SENSOR_TYPES:
        raise ValueError(
            f"unknown sensor_type '{type_key}', expected one of: {', '.join(sensor_types())}"
        )

    definition = SENSOR_TYPES[type_key]
    gases: dict = definition["gases"]

    gas_key = normalize_gas(gas) if gas else definition["primary_gas"]
    if gas_key not in gases:
        raise ValueError(
            f"{definition['label']} has no curve for gas '{gas_key}', "
            f"available gases: {', '.join(sorted(gases))}"
        )

    return ResolvedSensor(
        type_key=type_key,
        label=definition["label"],
        gas=gas_key,
        method=definition["method"],
        a=_clamp_coefficient(float(gases[gas_key][0]), MQ_MAX_A),
        b=_clamp_coefficient(float(gases[gas_key][1]), MQ_MAX_B),
        ratio_in_clean_air=float(
            definition["ratio_in_clean_air"]
            if ratio_in_clean_air is None
            else ratio_in_clean_air
        ),
        rl=float(10.0 if rl is None else rl),
        vcc=float(5.0 if vcc is None else vcc),
        min_ppm=0.0,
        max_ppm=float(definition["max_ppm"]),
    )

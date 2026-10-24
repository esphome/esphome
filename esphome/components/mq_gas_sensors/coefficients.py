"""Built-in regression coefficients for the MQ sensor families.

The values are the published `a` / `b` pairs of the MQUnifiedsensor PPM model::

    PPM = a * ratio^b                          (method: exponential)
    log10(PPM) = (log10(ratio) - b) / a        (method: linear)

with ``ratio = RS / R0``.

Provenance
----------
* ``a`` / ``b`` / regression method / ``ratio_in_clean_air`` (RS/R0 in clean air):
  ``sensorConfigData.h`` of the SolderedElectronics MQ library, which carries the
  MQUnifiedsensor curves, cross-checked against the per-sensor tables printed in
  the ``examples/`` of miguel5612/MQSensorsLib (e.g. MQ-3 Alcohol 0.3934 / -1.504,
  MQ-4 CH4 1012.7 / -2.786, MQ-8 H2 976.97 / -0.688).
* MQ-136, MQ-214, MQ-303A and MQ-309A have no published coefficients in either
  library (they need a two-phase heater drive), so ``a`` and ``b`` must be given
  explicitly in the YAML configuration.

Everything here is resolved at code generation time, so a wrong ``sensor_type`` /
``gas`` combination fails the build instead of silently producing garbage at
runtime.

Alternative curves: ``curve: mqdatascience`` selects the ``a`` / ``b`` /
regression method published by https://github.com/abcdaaaaaaaaa/MQDataScience
instead of the SolderedElectronics/MQUnifiedsensor values.

Temperature/humidity correction: ``correction_mode: mqdatascience`` needs the
``a`` / ``b`` / ``c`` constants of MQDataScience's ``Correction.cpp``. Only the
sensor types using the one-segment 33 %/85 % variant are listed.
"""

from dataclasses import dataclass
import math

#: Upper clamp of the magnitude of ``a`` / ``b`` in the firmware, mirroring
#: MQUnifiedsensor's ``MQ_MAX_A`` / ``MQ_MAX_B`` (``mqmath::clamp_a()`` /
#: ``mqmath::clamp_b()``). A coefficient outside this range is clamped here as
#: well, so that the firmware evaluates exactly what :func:`resolve_sensor`
#: returns; a user supplied value is checked against it in ``sensor.py`` before
#: it gets this far.
MQ_MAX_A = 1e30
MQ_MAX_B = 100.0

CUSTOM_TYPE = "CUSTOM"

#: Sensors that need user supplied coefficients (also see `heater_note`).
TYPES_REQUIRING_COEFFICIENTS = ("MQ136", "MQ214", "MQ303A", "MQ309A")

# ---------------------------------------------------------------------------
# Curve selection
# ---------------------------------------------------------------------------
CURVE_STANDARD = "standard"
CURVE_MQDATASCIENCE = "mqdatascience"

#: Accepted `curve:` values, `standard` (SolderedElectronics/MQUnifiedsensor) first.
CURVES = (CURVE_STANDARD, CURVE_MQDATASCIENCE)

#: Deviating curve datasets: (type key, gas key) -> curve -> (a, b, method).
#: Only the entries that differ from `SENSOR_TYPES` are listed here, so the
#: standard table stays the single source of truth.
CURVE_VARIANTS: dict[tuple[str, str], dict[str, tuple[float, float, str]]] = {
    # MQDataScience `GasModel MQ8` / "H2": a = 18391.5667, b = -1.4494, ppm =
    # (ratio / a)^(1 / b). Re-parameterised to the exponential form this is
    # a' = 875.4, b' = -0.68994 - the same slope, ~10.5 % lower than the
    # standard 976.97 / -0.688 (their clean-air anchor is ~47 instead of ~52 ppm).
    ("MQ8", "H2"): {
        CURVE_MQDATASCIENCE: (18391.5667, -1.4494, "inverse"),
    },
}


def curves_for(type_key: str, gas: str) -> list[str]:
    """Curve names available for a type/gas pair (`standard` always works)."""
    return [CURVE_STANDARD, *CURVE_VARIANTS.get((type_key, gas), {})]


# ---------------------------------------------------------------------------
# Temperature/humidity correction (MQDataScience Correction.cpp)
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class TcCoefficients:
    """Constants of the MQDataScience correction model ``a + c * exp(b * T)``.

    ``33`` / ``85`` are the relative humidities (in %) the constants were fitted
    at; the model interpolates linearly between them.
    """

    a33: float
    b33: float
    c33: float
    a85: float
    b85: float
    c85: float


#: The MQ-2/MQ-135/MQ-136/MQ-137/MQ-138/MQ-214/MQ-216 group shares one fit.
_TC_GROUP_STANDARD = TcCoefficients(0.8579, -0.0543, 0.4912, 0.7818, -0.0554, 0.4378)

#: type key -> correction constants (only the one-segment 33 %/85 % variant).
CORRECTION_COEFFICIENTS: dict[str, TcCoefficients] = {
    "MQ2": _TC_GROUP_STANDARD,
    "MQ3": TcCoefficients(0.7897, -0.0423, 0.5355, 0.7319, -0.0446, 0.4069),
    "MQ4": TcCoefficients(0.8597, -0.0381, 0.2861, 0.5838, -0.0218, 0.4064),
    "MQ5": TcCoefficients(0.8098, -0.0413, 0.3686, 0.6066, -0.0283, 0.3891),
    "MQ6": TcCoefficients(0.8714, -0.0440, 0.2883, 0.7287, -0.0412, 0.2648),
    "MQ7": TcCoefficients(0.8315, -0.0462, 0.3813, 0.6708, -0.0330, 0.3580),
    "MQ8": TcCoefficients(0.8559, -0.0611, 0.1673, 0.8201, -0.0606, 0.1492),
    "MQ135": _TC_GROUP_STANDARD,
    "MQ136": _TC_GROUP_STANDARD,
    "MQ137": _TC_GROUP_STANDARD,
    "MQ138": _TC_GROUP_STANDARD,
    "MQ214": _TC_GROUP_STANDARD,
    # MQ-9 and MQ-131 use the two-segment variant (RH 30/60/85 %) in
    # Correction.cpp and are intentionally not listed.
}


def correction_for(type_key: str) -> TcCoefficients | None:
    """Correction constants of ``type_key``, ``None`` if there is no model."""
    return CORRECTION_COEFFICIENTS.get(type_key)


def _type_sort_key(type_key: str) -> tuple[int, str]:
    """``MQ2`` < ``MQ3`` < ... < ``MQ8`` < ``MQ135`` < ... (numeric, not lexicographic)."""
    digits = "".join(char for char in type_key if char.isdigit())
    return (int(digits) if digits else 0, type_key)


def corrected_types() -> list[str]:
    """Datasheet names of the types that support the correction model, in order."""
    return [
        label_for(key) for key in sorted(CORRECTION_COEFFICIENTS, key=_type_sort_key)
    ]


@dataclass(frozen=True)
class ResolvedSensor:
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
    heater_note: str = ""
    curve: str = CURVE_STANDARD
    tc: TcCoefficients | None = None


def normalize_type(value: str) -> str:
    return str(value).strip().upper().replace("-", "").replace("_", "").replace(" ", "")


def normalize_gas(value: str) -> str:
    return str(value).strip().upper().replace("-", "_").replace(" ", "_")


def sensor_types() -> list[str]:
    """Canonical type keys, e.g. ``["MQ2", ..., "MQ309A", "CUSTOM"]``."""
    return list(SENSOR_TYPES)


def label_for(type_key: str) -> str:
    return SENSOR_TYPES[type_key]["label"]


def requires_coefficients(type_key: str) -> bool:
    return type_key == CUSTOM_TYPE or type_key in TYPES_REQUIRING_COEFFICIENTS


# ---------------------------------------------------------------------------
# type key -> sensor definition
#   label                 canonical datasheet name ("MQ-8")
#   method                "exponential" (1) or "linear" (2)
#   ratio_in_clean_air    RS/R0 in clean air (datasheet), used to compute R0
#   primary_gas           gas assumed when the YAML does not select one
#   gases                 gas -> (a, b) regression coefficients
#   rl / vcc              load resistor (kOhm) / supply (V) defaults
#   min_ppm / max_ppm     datasheet range used to clamp the published value
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
        "heater_note": "MQ-7 needs a 2-phase heater (5V for 60s, then 1.4V for 90s)",
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
        "gases": {  # hydrogen - the H2 curve of this project
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
        "ratio_in_clean_air": 15.0,  # RatioMQ131CleanAir of the reference examples
        "primary_gas": "O3",
        "max_ppm": 10.0,  # datasheet range is given in ppb (10 - 1000 ppb)
        "gases": {  # ozone
            "O3": (0.41195, -0.4708),
        },
    },
    "MQ135": {
        "label": "MQ-135",
        "method": "linear",
        "ratio_in_clean_air": 3.6,  # RatioMQ135CleanAir of the reference examples
        "primary_gas": "NH3",
        "max_ppm": 10000.0,
        "gases": {  # air quality: NH3 / H2 / toluene (verified linear fits)
            "NH3": (-0.47712, 0.4491),
            "H2": (-0.5101, 0.31988),
            "TOLUENE": (-0.21779, -0.23269),
        },
    },
    "MQ136": {
        "label": "MQ-136",
        "method": "exponential",
        "ratio_in_clean_air": None,
        "primary_gas": "",
        "max_ppm": 200.0,  # datasheet: 1 - 200 ppm H2S
        "heater_note": "MQ-136 needs a 2-phase heater (5V pre-heat, then 1.5V)",
        "gases": {},  # no published coefficients - set a: / b: in the YAML
    },
    "MQ137": {
        "label": "MQ-137",
        "method": "linear",
        "ratio_in_clean_air": None,
        "primary_gas": "NH3",
        "max_ppm": 500.0,  # datasheet: 5 - 500 ppm NH3
        "gases": {  # ammonia
            "NH3": (-0.26406, -0.24143),
        },
    },
    "MQ138": {
        "label": "MQ-138",
        "method": "linear",
        "ratio_in_clean_air": None,
        "primary_gas": "TOLUENE",
        "max_ppm": 1000.0,  # datasheet: 5 - 1000 ppm VOC
        "gases": {  # VOC / organic solvents
            "TOLUENE": (-0.4434, 0.15397),
            "ALCOHOL": (-0.46099, 0.0681),
            "ACETONE": (-0.52356, 0.49225),
        },
    },
    "MQ214": {
        "label": "MQ-214",
        "method": "exponential",
        "ratio_in_clean_air": None,
        "primary_gas": "",
        "max_ppm": 10000.0,  # natural gas / methane
        "gases": {},  # no published coefficients - set a: / b: in the YAML
    },
    "MQ303A": {
        "label": "MQ-303A",
        "method": "exponential",
        "ratio_in_clean_air": None,
        "primary_gas": "",
        "max_ppm": 1000.0,  # datasheet: 20 - 1000 ppm alcohol
        "heater_note": "MQ-303A drives an internal heater and needs 5V +/- 0.1V",
        "gases": {},  # no published coefficients - set a: / b: in the YAML
    },
    "MQ309A": {
        "label": "MQ-309A",
        "method": "exponential",
        "ratio_in_clean_air": None,
        "primary_gas": "",
        "max_ppm": 1000.0,  # datasheet: 20 - 1000 ppm CO
        "heater_note": "MQ-309A needs a 2-phase heater (1.5V low / 5V high cycle)",
        "gases": {},  # no published coefficients - set a: / b: in the YAML
    },
    CUSTOM_TYPE: {
        "label": "CUSTOM",
        "method": "exponential",
        "ratio_in_clean_air": None,
        "primary_gas": "",
        "max_ppm": 10000.0,
        "gases": {},  # set a: / b: (and optionally ratio_in_clean_air:) in the YAML
    },
}


def _clamp_coefficient(value: float, limit: float, key: str) -> float:
    """Clamp a user supplied coefficient to the range the firmware evaluates.

    ``_validate_config()`` in ``sensor.py`` warns when the returned value differs
    from ``value``. A non-finite value is rejected instead of being turned into 0,
    which would silently drop the curve.
    """
    if not math.isfinite(value):
        raise ValueError(f"'{key}' must be a finite number, got {value}")
    return max(-limit, min(limit, value))


def resolve_sensor(
    type_key: str,
    gas: str | None = None,
    *,
    a: float | None = None,
    b: float | None = None,
    method: str | None = None,
    ratio_in_clean_air: float | None = None,
    rl: float | None = None,
    vcc: float | None = None,
    min_ppm: float | None = None,
    max_ppm: float | None = None,
    curve: str = CURVE_STANDARD,
) -> ResolvedSensor:
    """Merge the built-in curve of `type_key` / `gas` with explicit YAML overrides.

    `curve` selects a different coefficient dataset (see `CURVE_VARIANTS`); the
    alternative curves define `a` / `b` / the regression method themselves, so
    combining them with explicit `a:` / `b:` is rejected.

    Raises :class:`ValueError` for an unknown type/gas, a missing coefficient, a
    one-sided coefficient override, a regression method without its own pair, or an
    unavailable curve variant.
    """
    if type_key not in SENSOR_TYPES:
        raise ValueError(
            f"unknown sensor_type '{type_key}', expected one of: {', '.join(sensor_types())}"
        )

    definition = SENSOR_TYPES[type_key]
    gases: dict = definition["gases"]

    gas_key = normalize_gas(gas) if gas is not None else definition["primary_gas"]
    # A user supplied pair defines its own curve; the built-in pair only applies
    # when neither coefficient is written.
    supplied_curve = a is not None and b is not None

    explicit_settings = a is not None or b is not None or method is not None

    if gases:
        if gas_key not in gases:
            raise ValueError(
                f"{definition['label']} has no curve for gas '{gas_key}', "
                f"available gases: {', '.join(sorted(gases))}"
            )
        if (a is None) != (b is None):
            raise ValueError(
                f"set both 'coefficient_a:' and 'coefficient_b:' for "
                f"{definition['label']} or neither - one of them combined with the "
                f"built-in pair is a curve nobody fitted"
            )
        if not supplied_curve:
            a, b = gases[gas_key]
    else:
        gas_key = gas_key or "CUSTOM"
        if not supplied_curve:
            raise ValueError(
                f"{definition['label']} has no built-in coefficients, "
                f"set both 'coefficient_a:' and 'coefficient_b:' in the configuration"
            )

    resolved_method = method or definition["method"]

    if (
        curve == CURVE_STANDARD
        and method is not None
        and method != definition["method"]
        and not supplied_curve
    ):
        raise ValueError(
            f"the coefficients of {definition['label']} are fitted for the "
            f"{definition['method']} curve - set both 'coefficient_a:' and "
            f"'coefficient_b:' to use 'regression_method: {method}'"
        )

    if curve != CURVE_STANDARD:
        variants = CURVE_VARIANTS.get((type_key, gas_key), {})
        if curve not in variants:
            raise ValueError(
                f"{definition['label']} {gas_key} has no '{curve}' curve, "
                f"available: {', '.join(curves_for(type_key, gas_key))}"
            )
        if explicit_settings:
            raise ValueError(
                f"'{curve}' defines the coefficients ('a', 'b') and the regression "
                f"method of {definition['label']} {gas_key} itself, remove the explicit "
                f"'a:'/'b:'/'regression_method:' keys"
            )
        a, b, resolved_method = variants[curve]

    # 0.0 means the type has no published clean-air ratio: only a boot
    # calibration needs one, and `sensor.py` rejects that without an override.
    ratio = (
        definition["ratio_in_clean_air"]
        if ratio_in_clean_air is None
        else ratio_in_clean_air
    )

    return ResolvedSensor(
        type_key=type_key,
        label=definition["label"],
        gas=gas_key,
        method=resolved_method,
        a=_clamp_coefficient(float(a), MQ_MAX_A, "coefficient_a"),
        b=_clamp_coefficient(float(b), MQ_MAX_B, "coefficient_b"),
        ratio_in_clean_air=0.0 if ratio is None else float(ratio),
        rl=float(10.0 if rl is None else rl),
        vcc=float(5.0 if vcc is None else vcc),
        min_ppm=float(0.0 if min_ppm is None else min_ppm),
        max_ppm=float(definition["max_ppm"] if max_ppm is None else max_ppm),
        heater_note=definition.get("heater_note", ""),
        curve=curve,
        tc=correction_for(type_key),
    )

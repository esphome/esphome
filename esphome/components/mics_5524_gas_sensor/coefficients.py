"""Data tables for the MiCS-5524 gas sensor platform.

Two conversion models are supported because the published references use two
different ones - they are *not* interchangeable:

``dfrobot`` (default, used by the makerguides tutorial)
    The vendor model of the `DFRobot_MICS` library (MIT), which is what the
    DFRobot "Fermion" analog breakout and its clones are sold for::

        x     = VCC - V_AO                      (VCC = module supply, 5 V)
        ratio = x / x_air                       (~1.0 in clean air, drops with gas)
        ppm   = (threshold - ratio) / gain      clamped to the gas range

    ``x_air`` is sampled during the clean-air calibration, so the model needs
    neither the board's load resistor nor its divider ratio - handy for the
    common unlabeled breakout.  Source: ``DFRobot_MICS.cpp``, ``getGasData()``
    and the per-gas ``getXXX()`` helpers (threshold/gain pairs), cross-checked
    against the Python port ``python/raspberrypi/DFRobot_MICS.py``.

``datasheet``
    The log-log fit of the datasheet curve, the same chain the sibling
    ``mq_gas_sensors`` component uses::

        RS    = (VCC * RL) / V_AO - RL
        ratio = RS / R0                         (R0 = RS in clean air)
        ppm   = a * ratio^b

    Only CO ships with coefficients: ``a = 6.3``, ``b = -1.1``, derived from two
    points of the datasheet curve (10 ppm at RS/R0 = 0.5 and 1000 ppm at
    RS/R0 = 0.01) in the Home Assistant community thread "CO sensor - MICS-5524
    or MICS-6814" (post 8).  All other gases need explicit ``a:``/``b:``.

Both tables are resolved at code generation time, so a wrong ``gas:`` /
``conversion:`` combination fails the build instead of producing nonsense.
"""

from dataclasses import dataclass

CONVERSION_DFROBOT = "dfrobot"
CONVERSION_DATASHEET = "datasheet"

#: Accepted ``conversion:`` values, the vendor model first.
CONVERSIONS = (CONVERSION_DFROBOT, CONVERSION_DATASHEET)


@dataclass(frozen=True)
class VendorCurve:
    """Constants of the DFRobot vendor model for one gas."""

    threshold: float  #: ratio at/below which the gas counts as detected
    gain: float  #: ppm per unit ratio (the vendor's step size)
    min_ppm: float  #: below this the vendor reports 0 ppm
    max_ppm: float  #: upper end of the measuring range


@dataclass(frozen=True)
class DatasheetCurve:
    """Coefficients of the datasheet power law ``ppm = a * ratio^b``."""

    a: float
    b: float
    max_ppm: float


@dataclass(frozen=True)
class GasDefinition:
    """One gas of the MiCS-5524, with the constants of both models."""

    key: str
    label: str
    vendor: VendorCurve | None
    datasheet: DatasheetCurve | None
    vendor_range: str
    note: str = ""


#: Per-gas constants.  The vendor values are the DFRobot_MICS `getGasData()`
#: thresholds/gains; the ranges are the vendor's documented measuring ranges (the
#: Fermion module reaches 1000 ppm for CO and H2, which is exactly the trace /
#: early-warning band this project uses the sensor for).
GASES: dict[str, GasDefinition] = {
    "CO": GasDefinition(
        key="CO",
        label="CO",
        vendor=VendorCurve(0.425, 0.000405, 1.0, 1000.0),
        datasheet=DatasheetCurve(6.3, -1.1, 1000.0),
        vendor_range="1 - 1000 ppm",
        note="2-point datasheet fit (10 ppm @ RS/R0 0.5, 1000 ppm @ RS/R0 0.01)",
    ),
    "H2": GasDefinition(
        key="H2",
        label="H2",
        vendor=VendorCurve(0.279, 0.00026, 1.0, 1000.0),
        datasheet=None,  # no published fit - set a:/b: explicitly
        vendor_range="1 - 1000 ppm",
        note="target gas of this project (trace / early warning band)",
    ),
    "NH3": GasDefinition(
        key="NH3",
        label="NH3",
        vendor=VendorCurve(0.8, 0.0015, 10.0, 500.0),
        datasheet=None,
        vendor_range="1 - 500 ppm (vendor math reports 0 below 10 ppm)",
    ),
    "C2H5OH": GasDefinition(
        key="C2H5OH",
        label="C2H5OH",
        vendor=VendorCurve(0.306, 0.00057, 10.0, 500.0),
        datasheet=None,
        vendor_range="10 - 500 ppm",
    ),
    "CH4": GasDefinition(
        key="CH4",
        label="CH4",
        vendor=VendorCurve(0.786, 0.000023, 1000.0, 25000.0),
        datasheet=None,
        vendor_range="1000 - 25000 ppm",
    ),
}

#: Every MQ/MiCS gas sensor responds to several reducing gases at once - the
#: MiCS-5524 has a single analog output, so a reading is always a sum of CO, H2,
#: ethanol, ammonia and methane.  Stated once here, quoted by the docs.
SELECTIVITY_NOTE = (
    "single analog output: the reading is a mixture of CO, H2, C2H5OH, NH3 and "
    "CH4, it cannot identify which gas is present"
)

#: Load resistor of the common analog breakouts (10 kOhm), the value the Home
#: Assistant community thread derives `RS = (5 V * 10 kOhm) / V_AO - 10 kOhm`
#: with.  Only the datasheet model uses it: measure your own board if in doubt.
DEFAULT_RL_KOHM = 10.0

#: Friendly aliases accepted by ``gas:``, because the vendor and the datasheet
#: name the gases differently (Hydrogen, Methane, Ethanol, Ammonia, Carbon
#: Monoxide).
GAS_ALIASES: dict[str, str] = {
    "CARBONMONOXIDE": "CO",
    "CO": "CO",
    "HYDROGEN": "H2",
    "H2": "H2",
    "AMMONIA": "NH3",
    "NH3": "NH3",
    "ETHANOL": "C2H5OH",
    "ALCOHOL": "C2H5OH",
    "C2H5OH": "C2H5OH",
    "METHANE": "CH4",
    "CH4": "CH4",
}


def normalize_gas(value: str) -> str:
    """``"h2"`` / ``"h-2"`` -> ``"H2"``; ``"ethanol"`` stays upper case."""
    return str(value).strip().upper().replace("-", "").replace(" ", "")


def resolve_gas_key(value: str) -> str:
    """Map a user supplied gas name to a key of :data:`GASES`.

    Accepts the vendor keys (``H2``, ``CH4``, ``C2H5OH``, ``NH3``, ``CO``) and the
    datasheet names (``hydrogen``, ``methane``, ``ethanol``/``alcohol``,
    ``ammonia``, ``carbon monoxide``).
    """
    normalized = normalize_gas(value)
    return GAS_ALIASES.get(normalized, normalized)


def gas_keys() -> list[str]:
    """Canonical gas keys, ordered as in the vendor documentation."""
    return list(GASES)


def label_for(gas_key: str) -> str:
    """Datasheet label of a gas key (``H2`` -> ``H2``)."""
    return GASES[gas_key].label


def datasheet_gases() -> list[str]:
    """Gases that ship with datasheet coefficients (the rest need ``a:``/``b:``)."""
    return [
        key for key, definition in GASES.items() if definition.datasheet is not None
    ]


def vendor_gases() -> list[str]:
    """Gases the vendor model supports."""
    return [key for key, definition in GASES.items() if definition.vendor is not None]


@dataclass(frozen=True)
class ResolvedGas:
    """Fully resolved gas configuration handed to the C++ component."""

    gas: str
    label: str
    conversion: str
    threshold: float  # vendor model: detection threshold of the ratio
    gain: float  # vendor model: ppm per unit ratio
    vendor_min_ppm: float  # vendor model: below this the vendor reports 0
    vendor_max_ppm: float  # vendor model: upper end of the vendor's own range
    a: float  # datasheet model
    b: float  # datasheet model
    min_ppm: float
    max_ppm: float
    note: str = ""


def resolve_gas(
    gas: str,
    conversion: str,
    *,
    a: float | None = None,
    b: float | None = None,
    min_ppm: float | None = None,
    max_ppm: float | None = None,
) -> ResolvedGas:
    """Merge the built-in table of `gas`/`conversion` with explicit YAML overrides.

    Raises :class:`ValueError` for an unknown gas/conversion, coefficients that
    belong to the other model, or a missing coefficient.
    """
    if conversion not in CONVERSIONS:
        raise ValueError(
            f"unknown conversion '{conversion}', expected one of: {', '.join(CONVERSIONS)}"
        )
    if gas not in GASES:
        raise ValueError(
            f"unknown gas '{gas}', expected one of: {', '.join(gas_keys())}"
        )

    definition = GASES[gas]
    explicit_coefficients = a is not None or b is not None

    if conversion == CONVERSION_DFROBOT:
        if explicit_coefficients:
            raise ValueError(
                f"'a:'/'b:' are the coefficients of conversion: {CONVERSION_DATASHEET} - "
                f"remove them or switch the conversion"
            )
        curve = definition.vendor
        if curve is None:  # pragma: no cover - every shipped gas has a vendor curve
            raise ValueError(f"{definition.label} has no built-in vendor constants")
        return ResolvedGas(
            gas=gas,
            label=definition.label,
            conversion=conversion,
            threshold=curve.threshold,
            gain=curve.gain,
            vendor_min_ppm=curve.min_ppm,
            vendor_max_ppm=curve.max_ppm,
            a=0.0,
            b=0.0,
            min_ppm=float(0.0 if min_ppm is None else min_ppm),
            max_ppm=float(curve.max_ppm if max_ppm is None else max_ppm),
            note=definition.note,
        )

    curve = definition.datasheet
    resolved_a = float(a) if a is not None else (curve.a if curve is not None else None)
    resolved_b = float(b) if b is not None else (curve.b if curve is not None else None)
    if resolved_a is None or resolved_b is None:
        shipped = ", ".join(datasheet_gases())
        raise ValueError(
            f"{definition.label} has no built-in 'conversion: {CONVERSION_DATASHEET}' "
            f"coefficients (only {shipped}) - set both 'a:' and 'b:' in the configuration"
        )
    return ResolvedGas(
        gas=gas,
        label=definition.label,
        conversion=conversion,
        threshold=0.0,
        gain=0.0,
        vendor_min_ppm=0.0,
        vendor_max_ppm=0.0,
        a=resolved_a,
        b=resolved_b,
        min_ppm=float(0.0 if min_ppm is None else min_ppm),
        max_ppm=float(
            (curve.max_ppm if curve is not None else 1000.0)
            if max_ppm is None
            else max_ppm
        ),
        note=definition.note,
    )

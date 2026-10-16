"""Data tables for the ``mics_5524_gas_sensor`` platform.

Two conversion models, which are *not* interchangeable:

``dfrobot`` (default) - vendor model of the `DFRobot_MICS` library (MIT), which is
what the "Fermion" analog breakout and its clones are sold for:
``x = VCC - V_AO``, ``ratio = x / x_air`` (~1.0 in clean air, drops with gas),
``ppm = (threshold - ratio) / gain`` clamped to the gas range.  ``x_air`` is sampled
during the clean-air calibration, so the board's load resistor and its divider ratio
do not matter.  Source: ``DFRobot_MICS.cpp`` (``getGasData()`` and the per-gas
helpers), cross-checked against ``python/raspberrypi/DFRobot_MICS.py``.

``datasheet`` - ``RS = (VCC * RL) / V_AO - RL``, ``ratio = RS / R0`` (R0 = RS in clean
air), ``ppm = a * ratio^b``.  Only CO ships with coefficients (``a = 6.3``,
``b = -1.1``, from 10 ppm at RS/R0 = 0.5 and 1000 ppm at RS/R0 = 0.01 in the Home
Assistant community thread "CO sensor - MICS-5524 or MICS-6814"); every other gas
needs explicit ``coefficient_a:``/``coefficient_b:``.

Every MQ/MiCS gas sensor responds to several reducing gases at once and the MiCS-5524
has a single analog output, so a reading is a mixture of CO, H2, ethanol, ammonia and
methane: it cannot say which of them is present.  A configuration therefore lists the
gases to report, one sub-sensor each, and the same reading is converted for every one
of them by the tables below.

Both tables are resolved at code generation time, so a wrong
gas/conversion/coefficient combination fails the build instead of producing nonsense.
"""

from dataclasses import dataclass

# The sub-sensor keys of the platform schema, listed as the datasheet does.
from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
)

CONVERSION_DFROBOT = "dfrobot"
CONVERSION_DATASHEET = "datasheet"

#: Accepted ``conversion:`` values, the vendor model first.
CONVERSIONS = (CONVERSION_DFROBOT, CONVERSION_DATASHEET)

#: Load resistor of the breakout (kOhm), the default of the datasheet model.
DEFAULT_RL_KOHM = 10.0


@dataclass(frozen=True)
class VendorCurve:
    """Constants of the DFRobot vendor model for one gas."""

    label: str  #: gas name as it appears in the log lines (`'C2H5OH'`)
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

    vendor: VendorCurve
    datasheet: DatasheetCurve | None


#: The gas table: the DFRobot_MICS `getGasData()` thresholds and gains, plus the
#: measuring ranges the vendor documents for them (the Fermion module reaches
#: 1000 ppm for CO and H2, the trace / early-warning band this project uses) and the
#: datasheet fit where one is published.  `max_ppm:` may narrow a range but never
#: widen it, and only CO has a built-in datasheet fit.
GASES: dict[str, GasDefinition] = {
    CONF_CARBON_MONOXIDE: GasDefinition(
        vendor=VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
        datasheet=DatasheetCurve(6.3, -1.1, 1000.0),
    ),
    CONF_HYDROGEN: GasDefinition(
        vendor=VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0),
        datasheet=None,  # no published fit: set coefficient_a:/-b: explicitly
    ),
    CONF_ETHANOL: GasDefinition(
        vendor=VendorCurve("C2H5OH", 0.306, 0.00057, 10.0, 500.0),
        datasheet=None,
    ),
    CONF_AMMONIA: GasDefinition(
        vendor=VendorCurve("NH3", 0.8, 0.0015, 10.0, 500.0),
        datasheet=None,
    ),
    CONF_METHANE: GasDefinition(
        vendor=VendorCurve("CH4", 0.786, 0.000023, 1000.0, 25000.0),
        datasheet=None,
    ),
}


def datasheet_gases() -> list[str]:
    """Labels of the gases that ship with datasheet coefficients."""
    return [
        definition.vendor.label
        for definition in GASES.values()
        if definition.datasheet is not None
    ]


@dataclass(frozen=True)
class ResolvedGas:
    """Fully resolved gas configuration handed to the C++ component."""

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


def resolve_gas(
    gas_key: str,
    conversion: str,
    *,
    a: float | None = None,
    b: float | None = None,
    min_ppm: float | None = None,
    max_ppm: float | None = None,
) -> ResolvedGas:
    """Merge the built-in table of `gas_key`/`conversion` with explicit YAML overrides.

    Raises :class:`ValueError` for an unknown gas or conversion, coefficients of the
    other model, or a missing coefficient.
    """
    if conversion not in CONVERSIONS:
        raise ValueError(
            f"unknown conversion '{conversion}', expected one of: {', '.join(CONVERSIONS)}"
        )
    if gas_key not in GASES:
        raise ValueError(
            f"unknown gas '{gas_key}', expected one of: {', '.join(GASES)}"
        )

    definition = GASES[gas_key]
    curve = definition.vendor

    if conversion == CONVERSION_DFROBOT:
        if a is not None or b is not None:
            raise ValueError(
                "'coefficient_a:'/'coefficient_b:' are the coefficients of "
                f"conversion: {CONVERSION_DATASHEET} - remove them or switch the "
                "conversion"
            )
        return ResolvedGas(
            label=curve.label,
            conversion=conversion,
            threshold=curve.threshold,
            gain=curve.gain,
            vendor_min_ppm=curve.min_ppm,
            vendor_max_ppm=curve.max_ppm,
            a=0.0,
            b=0.0,
            # The vendor's own floor is also where it stops reporting gas, so an
            # unconfigured `min_ppm:` keeps the documented behaviour of the table.
            min_ppm=float(curve.min_ppm if min_ppm is None else min_ppm),
            max_ppm=float(curve.max_ppm if max_ppm is None else max_ppm),
        )

    fitted = definition.datasheet
    # An explicit fit is all or nothing: pairing one coefficient with the shipped
    # value of the other mixes two different curve fits into a plausible but wrong
    # reading.
    if (a is None) != (b is None):
        raise ValueError(
            "set both 'coefficient_a:' and 'coefficient_b:' together, or neither "
            f"(only {', '.join(datasheet_gases())} ship with built-in "
            f"'conversion: {CONVERSION_DATASHEET}' coefficients)"
        )
    resolved_a = (
        float(a) if a is not None else (fitted.a if fitted is not None else None)
    )
    resolved_b = (
        float(b) if b is not None else (fitted.b if fitted is not None else None)
    )
    if resolved_a is None or resolved_b is None:
        shipped = ", ".join(datasheet_gases())
        raise ValueError(
            f"{curve.label} has no built-in 'conversion: {CONVERSION_DATASHEET}' "
            f"coefficients (only {shipped}) - set both 'coefficient_a:' and "
            "'coefficient_b:' in the configuration"
        )
    if resolved_a <= 0.0:
        raise ValueError(
            f"'coefficient_a:' must be positive, got {resolved_a:g} - "
            "a non-positive fit cannot produce a reading"
        )
    return ResolvedGas(
        label=curve.label,
        conversion=conversion,
        threshold=0.0,
        gain=0.0,
        vendor_min_ppm=0.0,
        vendor_max_ppm=0.0,
        a=resolved_a,
        b=resolved_b,
        min_ppm=float(0.0 if min_ppm is None else min_ppm),
        max_ppm=float(
            (fitted.max_ppm if fitted is not None else 1000.0)
            if max_ppm is None
            else max_ppm
        ),
    )

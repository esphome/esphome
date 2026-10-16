"""Gas tables of both MiCS-5524 conversion models; the formulas live in mics_math.h."""

from dataclasses import dataclass

from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
)

CONVERSION_DFROBOT = "dfrobot"
CONVERSION_DATASHEET = "datasheet"


@dataclass(frozen=True)
class VendorCurve:
    """Constants of the vendor model for one gas."""

    label: str  # vendor name of the gas
    threshold: float  # ratio at or below which the gas counts as detected
    gain: float  # ppm per unit ratio
    min_ppm: float  # the vendor reports 0 ppm below this
    max_ppm: float  # upper end of the measuring range


@dataclass(frozen=True)
class DatasheetCurve:
    """Coefficients of the datasheet fit ppm = a * ratio^b."""

    a: float
    b: float
    max_ppm: float


@dataclass(frozen=True)
class GasDefinition:
    """One gas with the constants of both models."""

    vendor: VendorCurve
    datasheet: DatasheetCurve | None


# DFRobot_MICS getGasData() thresholds and gains with the vendor measuring ranges; the CO
# datasheet fit is the community one (1000 ppm at RS/R0 = 0.01, about 10 ppm at RS/R0 = 0.65;
# clean air, RS/R0 = 1, reads the 6.3 ppm of `a`).
GASES: dict[str, GasDefinition] = {
    CONF_CARBON_MONOXIDE: GasDefinition(
        vendor=VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
        datasheet=DatasheetCurve(6.3, -1.1, 1000.0),
    ),
    CONF_HYDROGEN: GasDefinition(
        vendor=VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0),
        datasheet=None,  # no published fit
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
    """Resolved gas configuration handed to the C++ component."""

    label: str
    conversion: str
    p0: float  # threshold (vendor model) or a (datasheet model)
    p1: float  # gain (vendor model) or b (datasheet model)
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
    """Merge the built-in curve with the YAML overrides; ValueError on a bad combination."""
    if conversion not in (CONVERSION_DFROBOT, CONVERSION_DATASHEET):
        raise ValueError(f"unknown conversion '{conversion}'")
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
            p0=curve.threshold,
            p1=curve.gain,
            # The vendor floor is where it stops reporting gas, so it is the default min_ppm.
            min_ppm=float(curve.min_ppm if min_ppm is None else min_ppm),
            max_ppm=float(curve.max_ppm if max_ppm is None else max_ppm),
        )

    fitted = definition.datasheet
    # One coefficient with the shipped other one would mix two fits.
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
        p0=resolved_a,
        p1=resolved_b,
        min_ppm=float(0.0 if min_ppm is None else min_ppm),
        # A user fit without a published range falls back to the vendor range of the gas.
        max_ppm=float(
            (fitted.max_ppm if fitted is not None else curve.max_ppm)
            if max_ppm is None
            else max_ppm
        ),
    )

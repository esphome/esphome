"""Gas table of the DFRobot_MICS vendor model; the formula lives in mics_math.h."""

from dataclasses import dataclass

from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
)


@dataclass(frozen=True)
class VendorCurve:
    """Constants of the vendor model for one gas."""

    label: str  # vendor name of the gas
    threshold: float  # ratio at or below which the gas counts as detected
    gain: float  # ppm per unit ratio
    min_ppm: float  # the vendor reports 0 ppm below this
    max_ppm: float  # upper end of the measuring range


# DFRobot_MICS getGasData() thresholds and gains with the vendor measuring ranges.
GASES: dict[str, VendorCurve] = {
    CONF_CARBON_MONOXIDE: VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
    CONF_HYDROGEN: VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0),
    CONF_ETHANOL: VendorCurve("C2H5OH", 0.306, 0.00057, 10.0, 500.0),
    CONF_AMMONIA: VendorCurve("NH3", 0.8, 0.0015, 10.0, 500.0),
    CONF_METHANE: VendorCurve("CH4", 0.786, 0.000023, 1000.0, 25000.0),
}

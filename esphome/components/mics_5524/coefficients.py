"""Data tables for the ``mics_5524`` platform.

The conversion is the vendor model of the `DFRobot_MICS` library (MIT), which is
what the "Fermion" analog breakout and its clones are sold for:
``x = VCC - V_AO``, ``ratio = x / x_air`` (~1.0 in clean air, drops with gas),
``ppm = (threshold - ratio) / gain`` clamped to the gas range.  ``x_air`` is
sampled during the clean-air calibration, so the board's load resistor and its
divider ratio do not matter.  Source: ``DFRobot_MICS.cpp`` (``getGasData()`` and
the per-gas helpers), cross-checked against ``python/raspberrypi/DFRobot_MICS.py``.

Every MQ/MiCS gas sensor responds to several reducing gases at once and the
MiCS-5524 has a single analog output, so a reading is a mixture of CO, H2,
ethanol, ammonia and methane: it cannot say which of them is present.  A
configuration therefore lists the gases to report, one sub-sensor each, and the
same reading is converted for every one of them by the table below.
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


@dataclass(frozen=True)
class VendorCurve:
    """Constants of the DFRobot vendor model for one gas."""

    label: str  #: gas name as it appears in the log lines (`'C2H5OH'`)
    threshold: float  #: ratio at/below which the gas counts as detected
    gain: float  #: ppm per unit ratio (the vendor's step size)
    min_ppm: float  #: below this the vendor reports 0 ppm
    max_ppm: float  #: upper end of the measuring range


#: The gas table: the DFRobot_MICS `getGasData()` thresholds and gains, plus the
#: measuring ranges the vendor documents for them (the Fermion module reaches
#: 1000 ppm for CO and H2, the trace / early-warning band this project uses).
#: `max_ppm:` may narrow a range but never widen it.
GASES: dict[str, VendorCurve] = {
    CONF_CARBON_MONOXIDE: VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
    CONF_HYDROGEN: VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0),
    CONF_ETHANOL: VendorCurve("C2H5OH", 0.306, 0.00057, 10.0, 500.0),
    CONF_AMMONIA: VendorCurve("NH3", 0.8, 0.0015, 10.0, 500.0),
    CONF_METHANE: VendorCurve("CH4", 0.786, 0.000023, 1000.0, 25000.0),
}

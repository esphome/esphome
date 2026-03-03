from esphome.components.mipi import DriverChip
from esphome.config_validation import UNDEFINED

# A driver chip for Raspberry Pi MIPI RGB displays. These require no init sequence
DriverChip(
    "RPI",
    swap_xy=UNDEFINED,
    initsequence=(),
)

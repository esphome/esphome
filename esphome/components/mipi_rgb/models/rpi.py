from . import RgbDriverChip

# A driver chip for Raspberry Pi MIPI RGB displays. These require no init sequence
RgbDriverChip(
    "RPI",
    initsequence=(),
)

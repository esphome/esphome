from esphome.components.mipi import DriverChip
from esphome.const import CONF_SWAP_XY


class RgbDriverChip(DriverChip):
    """A driver chip for MIPI RGB displays."""

    @property
    def transforms(self) -> set[str]:
        """
        Return the set of transformations supported by this driver chip.
        RGB displays do not support axis swapping, so this method removes CONF_SWAP_XY
        """
        return super().transforms - {CONF_SWAP_XY}

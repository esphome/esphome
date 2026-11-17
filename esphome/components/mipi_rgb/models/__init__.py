from esphome.components.mipi import DriverChip
from esphome.const import CONF_SWAP_XY


class RgbDriverChip(DriverChip):
    """A driver chip for MIPI RGB displays."""

    @property
    def transforms(self) -> set[str]:
        """
        Return the set of transformations supported by this driver chip.
        RGB displays do not support axis swapping, so this method removes CONF_SWAP_XY.
        A model with an empty init sequence has no command interface at all, so it
        cannot apply any transform in hardware.
        """
        if self.initsequence is not None and len(self.initsequence) == 0:
            return set()
        return super().transforms - {CONF_SWAP_XY}

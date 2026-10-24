from esphome.components.mipi import MODE_BGR
from esphome.const import CONF_COLOR_ORDER, CONF_MIRROR_X, CONF_MIRROR_Y

from . import RgbDriverChip

# The GC9503V does not use the standard MADCTL (0x36) register. Its memory access
# control register is the vendor specific command 0xB1.
GC9503V_MADCTL = 0xB1
GC9503V_MADCTL_DEFAULT = 0x10  # Power-on default; this bit is preserved
GC9503V_MADCTL_SS = 0x01  # Source scan direction, used for mirror_y
GC9503V_MADCTL_GS = 0x02  # Gate scan direction, used for mirror_x
GC9503V_MADCTL_BGR = 0x20  # RGB/BGR colour order, 0 = RGB, 1 = BGR


class GC9503V(RgbDriverChip):
    def add_madctl(self, sequence: list, config: dict) -> None:
        transform = self.get_transform(config)
        madctl = GC9503V_MADCTL_DEFAULT
        if config[CONF_COLOR_ORDER] == MODE_BGR:
            madctl |= GC9503V_MADCTL_BGR
        if transform.get(CONF_MIRROR_X):
            madctl |= GC9503V_MADCTL_GS
        if transform.get(CONF_MIRROR_Y):
            madctl |= GC9503V_MADCTL_SS
        sequence.append((GC9503V_MADCTL, madctl))

    @property
    def transforms(self) -> set[str]:
        """The GC9503V can mirror both axes, but never swap them."""
        return {CONF_MIRROR_X, CONF_MIRROR_Y}


# The GC9503V init sequence is panel specific (gamma, gate driver timing), so the chip
# itself only selects the vendor command page. Boards supply their own sequence.
# fmt: off
gc9503v = GC9503V(
    "GC9503V",
    hsync_pulse_width=10,
    hsync_back_porch=10,
    hsync_front_porch=20,
    vsync_pulse_width=10,
    vsync_back_porch=10,
    vsync_front_porch=10,
    pclk_frequency="15MHz",
    pclk_inverted=True,
    # The panel does not initialise reliably at the 1MHz default; the vendor code uses 10MHz
    data_rate="10MHz",
    # Espressif's reference driver waits 120ms after reset
    reset_delay=120,
    # Both the vendor init and Espressif's driver wait 120ms between sleep out and display on;
    # sending display on earlier leaves the panel blank.
    slpout_delay=120,
    initsequence=(
        (0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00),  # Select command page 0
    ),
)

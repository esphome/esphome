import esphome.codegen as cg
from esphome.components.mipi import flatten_sequence
import esphome.config_validation as cv
from esphome.const import CONF_BUSY_PIN, CONF_RESET_PIN
from esphome.core import ID

from ..display import CONF_INIT_SEQUENCE_ID
from . import EpaperModel


class JD79660(EpaperModel):
    def __init__(self, name, class_name="EPaperJD79660", fast_update=None, **kwargs):
        super().__init__(name, class_name, **kwargs)
        self.fast_update = fast_update

    def option(self, name, fallback=cv.UNDEFINED) -> cv.Optional | cv.Required:
        # Validate required pins, as C++ code will assume existence
        if name in (CONF_RESET_PIN, CONF_BUSY_PIN):
            return cv.Required(name)

        # Delegate to parent
        return super().option(name, fallback)

    def get_constructor_args(self, config) -> tuple:
        # Resembles init_sequence handling for fast_update config
        if self.fast_update is None:
            fast_update = cg.nullptr, 0
        else:
            flat_fast_update = flatten_sequence(self.fast_update)
            fast_update = (
                cg.static_const_array(
                    ID(
                        config[CONF_INIT_SEQUENCE_ID].id + "_fast_update", type=cg.uint8
                    ),
                    flat_fast_update,
                ),
                len(flat_fast_update),
            )
        return (*fast_update,)


jd79660 = JD79660(
    "jd79660",
    # Specified refresh times are ~20s (full) or ~15s (fast) due to BWRY.
    # So disallow low update intervals (with safety margin), to avoid e.g. FSM update loops.
    # Even less frequent intervals (min/h) highly recommended to optimize lifetime!
    minimum_update_interval="30s",
    # SPI rate: From spec comparisons, IC should allow SCL write cycles up to 10MHz rate.
    # Existing code samples also prefer 10MHz. So justifies as default.
    # Decrease value further in user config if needed (e.g. poor cabling).
    data_rate="10MHz",
    # No need to set optional reset_duration:
    # Code requires multistep reset sequence with precise timings
    # according to data sheet or samples.
)

# Waveshare 1.54-G
#
# Device may have specific factory provisioned MTP content to facilitate vendor register features like fast init.
# Vendor specific init derived from vendor sample code
# <https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/1in54_e-Paper_G/ESP32/EPD_1in54g.cpp>
# Compatible MIT license, see esphome/LICENSE file.
#
# fmt: off
jd79660.extend(
    "Waveshare-1.54in-G",
    width=200,
    height=200,

    initsequence=(
        (0x4D, 0x78,),
        (0x00, 0x0F, 0x29,),
        (0x06, 0x0d, 0x12, 0x30, 0x20, 0x19, 0x2a, 0x22,),
        (0x50, 0x37,),
        (0x61, 200 // 256, 200 % 256, 200 // 256, 200 % 256,),  # RES: 200x200 fixed
        (0xE9, 0x01,),
        (0x30, 0x08,),
        # Power On (0x04): Must be early part of init seq = Disabled later!
        (0x04,),
    ),
    fast_update=(
        (0xE0, 0x02,),
        (0xE6, 0x5D,),
        (0xA5, 0x00,),
    ),
)

from esphome.const import (
    CONF_DATA_RATE,
    CONF_HEIGHT,
    CONF_MIRROR_X,
    CONF_SWAP_XY,
    CONF_WIDTH,
)

from . import EpaperModel

CONF_TEMPERATURE_C = "temperature_c"


class E2271KS0C1(EpaperModel):
    """
    Pervasive Displays E2271KS0C1 2.7" e-paper (264x176, fast partial updates).
    """

    def __init__(self, name, class_name="EPaperE2271KS0C1", **kwargs):
        kwargs.setdefault(CONF_DATA_RATE, "4MHz")
        # Panel internal layout is 176 columns x 264 rows
        # SWAP_XY presents user with 264x176 (width x height)
        kwargs.setdefault(CONF_WIDTH, 176)
        kwargs.setdefault(CONF_HEIGHT, 264)
        kwargs.setdefault(CONF_SWAP_XY, True)
        kwargs.setdefault(CONF_MIRROR_X, True)
        kwargs.setdefault(CONF_TEMPERATURE_C, 25.0)
        super().__init__(name, class_name, **kwargs)

    def get_init_sequence(self, config: dict):
        # Configuration happens dynamically in transfer_data()
        return ()


e2271ks0c1 = E2271KS0C1("E2271KS0C1")

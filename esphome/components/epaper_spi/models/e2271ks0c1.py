from esphome.const import (
    CONF_DATA_RATE,
    CONF_MIRROR_X,
    CONF_SWAP_XY,
    CONF_TEMPERATURE,
)

from . import EpaperModel


class E2271KS0C1(EpaperModel):
    """
    Pervasive Displays E2271KS0C1 2.7" e-paper (264x176, fast partial updates).
    """

    def __init__(self, name, class_name="EPaperE2271KS0C1", **kwargs):
        kwargs.setdefault(CONF_DATA_RATE, "4MHz")
        # Panel internal layout is 176 columns x 264 rows
        # SWAP_XY presents user with 264x176 (width x height)
        kwargs.setdefault(CONF_SWAP_XY, True)
        kwargs.setdefault(CONF_MIRROR_X, True)
        kwargs.setdefault(CONF_TEMPERATURE, 25.0)
        super().__init__(name, class_name, width=176, height=264, **kwargs)

    def get_init_sequence(self, config: dict):
        # Configuration happens dynamically in transfer_data()
        return ()


e2271ks0c1 = E2271KS0C1("E2271KS0C1")

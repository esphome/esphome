from esphome.const import CONF_DATA_RATE, CONF_HEIGHT, CONF_WIDTH

from . import EpaperModel

# Local constant - also defined in display.py for schema/codegen
CONF_TEMPERATURE_C = "temperature_c"


class E2271KS0C1(EpaperModel):
    """
    Pervasive Displays E2271KS0C1 2.7" e-paper display (264x176 pixels).
    Features fast partial update mode with temperature-compensated waveforms.
    """

    def __init__(self, name, class_name="EPaperE2271KS0C1", **kwargs):
        kwargs.setdefault(CONF_DATA_RATE, "4MHz")
        kwargs.setdefault(CONF_WIDTH, 264)
        kwargs.setdefault(CONF_HEIGHT, 176)
        kwargs.setdefault(CONF_TEMPERATURE_C, 25.0)
        # Busy pin is active-low on this display
        super().__init__(name, class_name, **kwargs)


e2271ks0c1 = E2271KS0C1("E2271KS0C1")

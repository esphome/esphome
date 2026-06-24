from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate6flick",
    cpp_class="Inkplate6FLICK",
    width=1024,
    height=758,
    dark_phases=4,     # 4 LUTB passes (Inkplate6FLICKDriver.cpp display1b k < 4)
    partial_phases=5,  # 5 passes (Inkplate6FLICKDriver.cpp partialUpdate k < 5)
    grayscale_phases=9,  # 9 phases — waveform table has 9 columns
    direct_pins={
        "ckv": 32,
        "sph": 33,
        "le":  2,
    },
    expander_pins={
        "oe":           0,
        "gmod":         1,
        "spv":          2,
        "wakeup":       3,
        "pwrup":        4,
        "vcom":         5,
        "gpio0_enable": 8,
    },
    min_update_interval_ms=5000,
)

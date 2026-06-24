from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate5v1",
    cpp_class="Inkplate5V1",
    width=960,
    height=540,
    dark_phases=3,
    partial_phases=4,
    grayscale_phases=9,
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

InkplateParallelModel(
    name="inkplate5v2",
    cpp_class="Inkplate5",
    width=1280,
    height=720,
    dark_phases=3,       # 3 LUTB passes (Inkplate5V2Driver.cpp display1b)
    partial_phases=4,    # 4 passes (Inkplate5V2Driver.cpp partialUpdate)
    grayscale_phases=9,  # 9 phases from WAVEFORM3BIT table
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

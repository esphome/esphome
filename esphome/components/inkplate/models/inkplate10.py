from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate10",
    cpp_class="Inkplate10",
    width=1200,
    height=825,
    dark_phases=5,  # 5 LUTB passes (Inkplate10Driver.cpp display1b _repeat=5)
    partial_phases=5,  # 5 passes (Inkplate10Driver.cpp partialUpdate _repeat=5)
    grayscale_phases=9,  # 9 phases from WAVEFORM3BIT table
    direct_pins={
        "ckv": 32,
        "sph": 33,
        "le": 2,
    },
    expander_pins={
        "oe": 0,
        "gmod": 1,
        "spv": 2,
        "wakeup": 3,
        "pwrup": 4,
        "vcom": 5,
        "gpio0_enable": 8,
    },
    min_update_interval_ms=5000,
)

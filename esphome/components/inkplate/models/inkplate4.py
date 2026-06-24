from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate4",
    cpp_class="Inkplate4",
    width=600,
    height=600,
    dark_phases=10,  # 10 LUTB passes (Inkplate4TEMPERADriver.cpp display1b k < 10)
    partial_phases=9,  # 9 passes (Inkplate4TEMPERADriver.cpp partialUpdate k < 9)
    grayscale_phases=9,  # 9 phases — matches waveform table columns
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

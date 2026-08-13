from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate6v2",
    cpp_class="Inkplate6",
    width=800,
    height=600,
    dark_phases=5,
    partial_phases=6,
    grayscale_phases=9,
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

InkplateParallelModel(
    name="inkplate6v1",
    cpp_class="Inkplate6V1",
    width=800,
    height=600,
    dark_phases=4,
    partial_phases=5,
    grayscale_phases=9,
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

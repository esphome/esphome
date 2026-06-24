from . import InkplateParallelModel

InkplateParallelModel(
    name="inkplate6plus",
    cpp_class="Inkplate6PLUS",
    width=1024,
    height=758,
    dark_phases=4,
    partial_phases=5,
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
        "gpio0_enable": 9,
    },
    gpio0_enable_low=True,
    min_update_interval_ms=5000,
)

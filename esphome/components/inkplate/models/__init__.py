class InkplateParallelModel:
    models = {}

    def __init__(
        self,
        name,
        cpp_class,
        width,
        height,
        dark_phases,
        partial_phases,
        grayscale_phases,
        direct_pins=None,
        expander_pins=None,
        gpio0_enable_low=False,
        min_update_interval_ms=5000,
    ):
        self.name = name
        self.cpp_class = cpp_class
        self.width = width
        self.height = height
        self.dark_phases = dark_phases
        self.partial_phases = partial_phases
        self.grayscale_phases = grayscale_phases
        # direct_pins: pin_name → ESP32 GPIO number (injected as defaults, user may override)
        self.direct_pins = direct_pins or {}
        # expander_pins: pin_name → PCAL6416A pin number (auto-wired from pca6416a_id)
        self.expander_pins = expander_pins or {}
        self.gpio0_enable_low = gpio0_enable_low
        self.min_update_interval_ms = min_update_interval_ms
        InkplateParallelModel.models[name] = self

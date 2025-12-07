"""Inkplate device model definitions."""

from typing import Any

import esphome.config_validation as cv
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_NUMBER,
    CONF_WIDTH,
)


class InkplateModel:
    """
    A class representing an Inkplate device model.
    The parameters define the model name, enum constant, default pin assignments, and waveforms.
    """

    models: dict[str, "InkplateModel"] = {}

    def __init__(self, name: str, **defaults):
        """
        Initialize an Inkplate model.

        :param name: Display name of the model (e.g., "inkplate_6")
        :param defaults Default options
        """
        self.name = name.upper()
        self.defaults = defaults

        # Automatically register the model
        InkplateModel.models[self.name] = self

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)

    def option(self, name, fallback=cv.UNDEFINED) -> cv.Optional | cv.Required:
        if fallback is None and self.get_default(name, None) is None:
            return cv.Required(name)
        return cv.Optional(name, default=self.get_default(name, fallback))

    def get_dimensions(self, config) -> tuple[int, int]:
        if CONF_DIMENSIONS in config:
            # Explicit dimensions, just use as is
            dimensions = config[CONF_DIMENSIONS]
            if isinstance(dimensions, dict):
                width = dimensions[CONF_WIDTH]
                height = dimensions[CONF_HEIGHT]
            else:
                (width, height) = dimensions

        else:
            # Default dimensions, use model defaults
            width = self.get_default(CONF_WIDTH)
            height = self.get_default(CONF_HEIGHT)
        return width, height

    def extend(self, name, **defaults) -> "InkplateModel":
        """
        Extend the current model with additional parameters or a modified init sequence.
        Parameters supplied here will override the defaults of the current model.
        :param name:
        :param defaults New defaults, will override the base model defaults
        :return:
        """
        basis = self.defaults.copy()
        basis.update(defaults)
        return self.__class__(name, **basis)

    @classmethod
    def get_models(cls):
        return cls.models

    @property
    def waveform(self):
        return self.get_default("waveform")


# Inkplate 6
inkplate_6 = InkplateModel(
    name="inkplate_6",
    width=800,
    height=600,
    display_data_pins=[
        4,
        {CONF_NUMBER: 5, CONF_IGNORE_STRAPPING_WARNING: True},
        18,
        19,
        23,
        25,
        26,
        27,
    ],
    cl_pin={CONF_NUMBER: 0, CONF_IGNORE_STRAPPING_WARNING: True},
    le_pin={CONF_NUMBER: 2, CONF_IGNORE_STRAPPING_WARNING: True},
    waveform=(
        (0, 1, 1, 0, 0, 1, 1, 0, 0),
        (0, 1, 2, 1, 1, 2, 1, 0, 0),
        (1, 1, 1, 2, 2, 1, 0, 0, 0),
        (0, 0, 0, 1, 1, 1, 2, 0, 0),
        (2, 1, 1, 1, 2, 1, 2, 0, 0),
        (2, 2, 1, 1, 2, 1, 2, 0, 0),
        (1, 1, 1, 2, 1, 2, 2, 0, 0),
        (0, 0, 0, 0, 0, 0, 2, 0, 0),
    ),
)

# Inkplate 10
inkplate_6.extend(
    name="inkplate_10",
    width=1200,
    height=825,
    waveform=(
        (0, 0, 0, 0, 0, 0, 0, 1, 0),
        (0, 0, 0, 2, 2, 2, 1, 1, 0),
        (0, 0, 2, 1, 1, 2, 2, 1, 0),
        (0, 1, 2, 2, 1, 2, 2, 1, 0),
        (0, 0, 2, 1, 2, 2, 2, 1, 0),
        (0, 2, 2, 2, 2, 2, 2, 1, 0),
        (0, 0, 0, 0, 0, 2, 1, 2, 0),
        (0, 0, 0, 2, 2, 2, 2, 2, 0),
    ),
    custom_waveforms=(
        (
            (0, 0, 0, 0, 0, 0, 0, 0, 0),
            (0, 0, 0, 2, 1, 2, 1, 1, 0),
            (0, 0, 0, 2, 2, 1, 2, 1, 0),
            (0, 0, 2, 2, 1, 2, 2, 1, 0),
            (0, 0, 0, 2, 1, 1, 1, 2, 0),
            (0, 0, 2, 2, 2, 1, 1, 2, 0),
            (0, 0, 0, 0, 0, 1, 2, 2, 0),
            (0, 0, 0, 0, 2, 2, 2, 2, 0),
        ),
        (
            (0, 3, 3, 3, 3, 3, 3, 3, 0),
            (0, 1, 2, 1, 1, 2, 2, 1, 0),
            (0, 2, 2, 2, 1, 2, 2, 1, 0),
            (0, 0, 2, 2, 2, 2, 2, 1, 0),
            (0, 3, 3, 2, 1, 1, 1, 2, 0),
            (0, 3, 3, 2, 2, 1, 1, 2, 0),
            (0, 2, 1, 2, 1, 2, 1, 2, 0),
            (0, 3, 3, 3, 2, 2, 2, 2, 0),
        ),
        (
            (0, 0, 0, 0, 0, 0, 0, 1, 0),
            (0, 0, 0, 2, 2, 2, 1, 1, 0),
            (0, 0, 2, 1, 1, 2, 2, 1, 0),
            (1, 1, 2, 2, 1, 2, 2, 1, 0),
            (0, 0, 2, 1, 2, 2, 2, 1, 0),
            (0, 1, 2, 2, 2, 2, 2, 1, 0),
            (0, 0, 0, 2, 2, 2, 1, 2, 0),
            (0, 0, 0, 2, 2, 2, 2, 2, 0),
        ),
        (
            (0, 0, 0, 0, 0, 0, 0, 1, 0),
            (0, 0, 0, 2, 2, 2, 1, 1, 0),
            (2, 2, 2, 1, 0, 2, 1, 0, 0),
            (2, 1, 1, 2, 1, 1, 1, 2, 0),
            (2, 2, 2, 1, 1, 1, 0, 2, 0),
            (2, 2, 2, 1, 1, 2, 1, 2, 0),
            (0, 0, 0, 0, 2, 1, 2, 2, 0),
            (0, 0, 0, 0, 2, 2, 2, 2, 0),
        ),
    ),
)

# Inkplate 6 Plus
inkplate_6.extend(
    name="inkplate_6_plus",
    width=1024,
    height=758,
    waveform=(
        (0, 0, 0, 0, 0, 2, 1, 1, 0),
        (0, 0, 2, 1, 1, 1, 2, 1, 0),
        (0, 2, 2, 2, 1, 1, 2, 1, 0),
        (0, 0, 2, 2, 2, 1, 2, 1, 0),
        (0, 0, 0, 0, 2, 2, 2, 1, 0),
        (0, 0, 2, 1, 2, 1, 1, 2, 0),
        (0, 0, 2, 2, 2, 1, 1, 2, 0),
        (0, 0, 0, 0, 2, 2, 2, 2, 0),
    ),
)

# Inkplate 6 V2
inkplate_6.extend(
    name="inkplate_6_v2",
    width=800,
    height=600,
    waveform=(
        (1, 0, 1, 0, 1, 1, 1, 0, 0),
        (0, 0, 0, 1, 1, 1, 1, 0, 0),
        (1, 1, 1, 1, 0, 2, 1, 0, 0),
        (1, 1, 1, 2, 2, 1, 1, 0, 0),
        (1, 1, 1, 1, 2, 2, 1, 0, 0),
        (0, 1, 1, 1, 2, 2, 1, 0, 0),
        (0, 0, 0, 0, 1, 1, 2, 0, 0),
        (0, 0, 0, 0, 0, 1, 2, 0, 0),
    ),
)

# Inkplate 5
inkplate_6.extend(
    name="inkplate_5",
    width=960,
    height=540,
    ckv_pin=32,
    sph_pin=33,
    gmod_pin={"pca6416a": None, "number": 1},
    gpio0_enable_pin={"pca6416a": None, "number": 8},
    oe_pin={"pca6416a": None, "number": 0},
    spv_pin={"pca6416a": None, "number": 2},
    powerup_pin={"pca6416a": None, "number": 4},
    wakeup_pin={"pca6416a": None, "number": 3},
    vcom_pin={"pca6416a": None, "number": 5},
    waveform=(
        (0, 0, 1, 1, 0, 1, 1, 1, 0),
        (0, 1, 1, 1, 1, 2, 0, 1, 0),
        (1, 2, 2, 0, 2, 1, 1, 1, 0),
        (1, 1, 1, 2, 0, 1, 1, 2, 0),
        (0, 1, 1, 1, 2, 0, 1, 2, 0),
        (0, 0, 0, 1, 1, 2, 1, 2, 0),
        (1, 1, 1, 2, 0, 2, 1, 2, 0),
        (0, 0, 0, 0, 0, 0, 0, 0, 0),
    ),
)

# Inkplate 5 V2
inkplate_6.extend(
    name="inkplate_5_v2",
    width=1280,
    height=720,
    waveform=(
        (0, 0, 1, 1, 2, 1, 1, 1, 0),
        (1, 1, 2, 2, 1, 2, 1, 1, 0),
        (0, 1, 2, 2, 1, 1, 2, 1, 0),
        (0, 0, 1, 1, 1, 1, 1, 2, 0),
        (1, 2, 1, 2, 1, 1, 1, 2, 0),
        (0, 1, 1, 1, 2, 0, 1, 2, 0),
        (1, 1, 1, 2, 2, 2, 1, 2, 0),
        (0, 0, 0, 0, 0, 0, 0, 0, 0),
    ),
)

from typing import Any, Self

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS, CONF_HEIGHT, CONF_WIDTH
from esphome.cpp_generator import MockObj


class EpaperModel:
    models: dict[str, Self] = {}

    # Whether the driver manages chip-select itself instead of via the SPI bus.
    manages_cs: bool = False

    def __init__(
        self,
        name: str,
        class_name: str,
        initsequence=None,
        **defaults,
    ):
        name = name.upper()
        self.name = name
        self.class_name = class_name
        self.initsequence = initsequence
        self.defaults = defaults
        EpaperModel.models[name] = self

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)

    def get_init_sequence(self, config: dict):
        return self.initsequence

    def option(self, name, fallback=cv.UNDEFINED) -> cv.Optional | cv.Required:
        if fallback is None and self.get_default(name, None) is None:
            return cv.Required(name)
        return cv.Optional(name, default=self.get_default(name, fallback))

    def get_constructor_args(self, config) -> tuple:
        return ()

    def get_config_options(self) -> dict:
        """
        Return model-specific configuration schema options.
        The base implementation adds nothing; specific models override this to
        declare extra options without cluttering the shared schema.
        :return: A mapping suitable for cv.Schema.extend()
        """
        return {}

    async def to_code(self, var: MockObj, config: dict) -> dict:
        """
        Generate model-specific code for the options added by add_options().
        The base implementation does nothing; specific models override this.
        The config can be updated in place to add or remove options.
        :param var: The component variable
        :param config: The validated configuration
        """
        return config

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

    def extend(self, name, **kwargs) -> "EpaperModel":
        """
        Extend the current model with additional parameters or a modified init sequence.
        Parameters supplied here will override the defaults of the current model.
        if the initsequence is not provided, the current model's initsequence will be used.
        If add_init_sequence is provided, it will be appended to the current initsequence.
        :param name:
        :param kwargs:
        :return:
        """
        initsequence = list(kwargs.pop("initsequence", self.initsequence) or ())
        initsequence.extend(kwargs.pop("add_init_sequence", ()))
        defaults = self.defaults.copy()
        defaults.update(kwargs)
        return self.__class__(name, initsequence=tuple(initsequence), **defaults)

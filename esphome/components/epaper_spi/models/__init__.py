from typing import Any, Self

from esphome.components.mipi import check_model_requirements
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_FULL_UPDATE_EVERY,
    CONF_HEIGHT,
    CONF_OFFSET_HEIGHT,
    CONF_WIDTH,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CONF_MONOCHROME_PARTIAL_UPDATES = "monochrome_partial_updates"

# partial_update value for models whose partial updates are black and white only
MONOCHROME = "monochrome"


def ultrachip_resolution(width: int, gates: int) -> tuple:
    """The UC81xx resolution command (0x61): source (width) and gate counts, each big-endian."""
    return (0x61, width >> 8, width & 0xFF, gates >> 8, gates & 0xFF)


class EpaperModel:
    models: dict[str, Self] = {}

    # Whether the driver manages chip-select itself instead of via the SPI bus.
    manages_cs: bool = False

    # The .cpp files this model's driver class needs: its own and those of every parent class
    # except epaper_spi.cpp, which is always built. Subclasses override this next to their class_name.
    source_files: tuple[str, ...] = ()

    def __init__(
        self,
        name: str,
        class_name: str,
        initsequence=(),
        source_files: tuple[str, ...] | None = None,
        **defaults,
    ):
        name = name.upper()
        self.name = name
        self.class_name = class_name
        self.initsequence = initsequence
        if source_files is not None:
            self.source_files = source_files
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
        The base implementation adds the option that a model with black and white only
        partial updates needs; specific models extend this, calling super(), to declare
        extra options without cluttering the shared schema.
        :return: A mapping suitable for cv.Schema.extend()
        """
        if self.get_default("partial_update") != MONOCHROME:
            return {}
        return {cv.Optional(CONF_MONOCHROME_PARTIAL_UPDATES, default=False): cv.boolean}

    def validate_config(self, config: ConfigType) -> ConfigType:
        """
        Validate the configuration as a whole, once the schema has been applied.
        The base implementation checks the option a model with black and white only
        partial updates needs; specific models extend this, calling super(), for rules
        that span several options.
        :param config: The validated configuration
        :return: The configuration, possibly updated
        """
        if (
            self.get_default("partial_update") == MONOCHROME
            and config[CONF_FULL_UPDATE_EVERY] > 1
            and not config.get(CONF_MONOCHROME_PARTIAL_UPDATES)
        ):
            raise cv.Invalid(
                f"{self.name} can only update partially in black and white, and a partial "
                "update reduces the whole panel to black and white until the next full "
                f"update. Set '{CONF_MONOCHROME_PARTIAL_UPDATES}: true' to accept this, "
                "or leave full_update_every at 1",
                path=[CONF_FULL_UPDATE_EVERY],
            )
        return config

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

    def get_offset_height(self, config) -> int:
        # The panel's first row within the controller's addressable area; from explicit dimensions or the model default
        dimensions = config.get(CONF_DIMENSIONS, {})
        return dimensions.get(
            CONF_OFFSET_HEIGHT, self.get_default(CONF_OFFSET_HEIGHT, 0)
        )

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
        model = self.__class__(name, initsequence=tuple(initsequence), **defaults)
        # Subclass constructors do not all take these, so carry them over unless overridden
        if "class_name" not in kwargs:
            model.class_name = self.class_name
        if "source_files" not in kwargs:
            model.source_files = self.source_files
        return model

    def check_requirements(self) -> None:
        """Check that the components this model needs are configured, see check_model_requirements()."""
        check_model_requirements(self.name, self.defaults)

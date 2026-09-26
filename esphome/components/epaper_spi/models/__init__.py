from typing import Any, Self

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS, CONF_HEIGHT, CONF_WIDTH
from esphome.core import CORE
from esphome.cpp_generator import MockObj

LOGGER = cv.logging.getLogger(__name__)


class EpaperModel:
    models: dict[str, Self] = {}

    # Whether the driver manages chip-select itself instead of via the SPI bus.
    manages_cs: bool = False

    # Whether the driver can do a lighter/quicker update in between full refreshes. Models that
    # can't (e.g. a waveform that only supports a full refresh) must reject full_update_every
    # values other than 1.
    supports_partial_update: bool = True

    def __init__(
        self,
        name: str,
        class_name: str,
        initsequence=(),
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
        class_name = kwargs.pop("class_name", self.class_name)
        defaults.update(kwargs)
        return self.__class__(
            name, class_name=class_name, initsequence=tuple(initsequence), **defaults
        )

    def check_requirements(self) -> None:
        """
        Raise a friendly error if any component this model requires is not configured.

        This runs during schema validation (before ID references are resolved) so that a
        model whose default pins live on a pin expander reports the missing expander clearly
        instead of a cryptic "Couldn't find ID" from the unresolved pin reference.

        Also logs a warning if the model is deprecated.
        """
        if deprecation_reason := self.get_default("deprecation_reason"):
            LOGGER.warning(
                "Display model %s is deprecated: %s", self.name, deprecation_reason
            )
        if requirements := self.get_default("requires", set()):
            # ``raw_config`` is populated before any component schema runs during a real
            # validation, so presence of a required component is simply a top-level key.
            # When it is absent (e.g. a unit test that invokes the schema directly) there
            # is no config to check against, so skip.
            global_config = CORE.raw_config
            if global_config is None:
                return
            missing = {x for x in requirements if x not in global_config}
            if missing:
                reqstr = ", ".join(f"'{x}'" for x in sorted(missing))
                raise cv.Invalid(
                    f"{self.name} requires component{'s' if len(missing) > 1 else ''} {reqstr} to be configured"
                )

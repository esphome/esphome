from abc import ABC, abstractmethod
from collections.abc import Callable
import contextvars
from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_PLATFORM
from esphome.types import ID, ConfigFragmentType, ConfigPathType, ConfigType


class FinalValidateConfig(ABC):
    @property
    @abstractmethod
    def data(self) -> dict[str, Any]:
        """A dictionary that can be used by post validation functions to store
        global data during the validation phase. Each component should store its
        data under a unique key
        """

    @abstractmethod
    def get_path_for_id(self, id: ID) -> ConfigPathType:
        """Get the config path a given ID has been declared in.

        This is the location under the _validated_ config (for example, with cv.ensure_list applied)
        Raises KeyError if the id was not declared in the configuration.
        """

    @abstractmethod
    def get_config_for_path(self, path: ConfigPathType) -> ConfigFragmentType:
        """Get the config fragment for the given global path.

        Raises KeyError if a key in the path does not exist.
        """


FinalValidateConfig.register(dict)

# Context variable tracking the full config for some final validation functions.
full_config: contextvars.ContextVar[FinalValidateConfig] = contextvars.ContextVar(
    "full_config"
)


def get_declaring_config(
    id_: ID, config: FinalValidateConfig | None = None
) -> ConfigType:
    """Return the config block that declares id_, such as a hub's own config.

    Uses the full config being validated unless config is given.
    Raises KeyError if the id was not declared, TypeError if its parent is not a dict.
    """
    fconf = full_config.get() if config is None else config
    declaring = fconf.get_config_for_path(fconf.get_path_for_id(id_)[:-1])
    if not isinstance(declaring, dict):
        raise TypeError(f"ID {id_} is not declared in a config block")
    return declaring


def platform_entries(
    *, domain: str, platform: str, match: dict[str, Any] | None = None
) -> list[ConfigType]:
    """Return the entries of domain set up by platform whose keys equal the match values.

    Reads the full config being validated, for example
    platform_entries(domain="switch", platform="my_hub", match={CONF_CHANNEL: 1}).
    """
    match = match or {}
    return [
        entry
        for entry in full_config.get().get(domain, [])
        if entry.get(CONF_PLATFORM) == platform
        and all(entry.get(key) == value for key, value in match.items())
    ]


def unique_platform_key(
    *, domain: str, platform: str, keys: tuple[str, ...] = (), message: str
) -> Callable[[ConfigType], ConfigType]:
    """A final-validation function that allows one platform entry per combination of keys.

    With no keys it allows one entry of the platform in total. message is formatted with
    the entry's key values, by position and by key name; the error points at the last key.
    """
    path = [keys[-1]] if keys else []

    def validator(config: ConfigType) -> ConfigType:
        values = {key: config.get(key) for key in keys}
        if len(platform_entries(domain=domain, platform=platform, match=values)) > 1:
            raise cv.Invalid(message.format(*values.values(), **values), path=path)
        return config

    return validator


def id_declaration_match_schema(schema):
    """A final-validation schema function that applies a schema to the outer config fragment of an
    ID declaration.

    This validator must be applied to ID values.
    """
    if not isinstance(schema, cv.Schema):
        schema = cv.Schema(schema, extra=cv.ALLOW_EXTRA)

    def validator(value):
        fconf = full_config.get()
        path = fconf.get_path_for_id(value)[:-1]
        declaration_config = fconf.get_config_for_path(path)
        with cv.prepend_path([cv.ROOT_CONFIG_PATH] + path):
            return schema(declaration_config)

    return validator

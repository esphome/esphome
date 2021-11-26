from abc import ABC, abstractmethod
from typing import Dict, Any
import contextvars

from esphome.types import ConfigFragmentType, ID, ConfigPathType
import esphome.config_validation as cv


class FinalValidateConfig(ABC):
    @property
    @abstractmethod
    def data(self) -> Dict[str, Any]:
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

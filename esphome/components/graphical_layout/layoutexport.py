import esphome.config_validation as cv
from typing import Awaitable, Any, Callable, Optional

class LayoutImport:
    def __init__(self, name : str, schema_builder_func: Callable[[cv.Schema, cv.Schema, cv.Schema], cv.Schema], builder_func : Awaitable[Any], parent_schema_builder_func : Optional[Callable[[], cv.Schema]] = None):
        self.name = name
        self.schema_builder_func = schema_builder_func
        self.builder_func = builder_func
        self.parent_schema_builder_func = parent_schema_builder_func
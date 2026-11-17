from ..dyp_base import (
    DYPModel,
    dyp_final_validate_schema,
    dyp_sensor_schema,
    to_code_base,
)
from . import DOMAIN

AUTO_LOAD = ["dyp_base"]
DEPENDENCIES = ["uart"]

MODELS = {
    "A02": DYPModel("3cm", "450cm"),
    "A02YYT": DYPModel("3cm", "450cm", mode="CONTROLLED"),
    "A02YYTW": DYPModel("3cm", "450cm", mode="CONTROLLED"),
    "A02YYU": DYPModel("3cm", "450cm", mode="AUTO"),
    "A02YYUW": DYPModel("3cm", "450cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

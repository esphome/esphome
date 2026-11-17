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
    "A10A": DYPModel("25cm", "450cm"),
    "A10ANYTW": DYPModel("25cm", "450cm", mode="CONTROLLED"),
    "A10ANYUW": DYPModel("25cm", "450cm", mode="AUTO"),
    "A10B": DYPModel("28cm", "450cm"),
    "A10BNYTW": DYPModel("28cm", "450cm", mode="CONTROLLED"),
    "A10BNYUW": DYPModel("28cm", "450cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

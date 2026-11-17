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
    "ME007YS": DYPModel("28cm", "450cm"),
    "ME007YS-TX": DYPModel("28cm", "450cm", mode="AUTO"),
    "ME007YS-TX1": DYPModel("28cm", "450cm", mode="CONTROLLED"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

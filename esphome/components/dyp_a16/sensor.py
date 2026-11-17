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
    "A16": DYPModel("50cm", "1500cm"),
    "A16NYTW": DYPModel("50cm", "1500cm", mode="CONTROLLED"),
    "A16NYUW": DYPModel("50cm", "1500cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

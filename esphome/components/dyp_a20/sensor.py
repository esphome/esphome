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
    "A20": DYPModel("3cm", "300cm"),
    "A20NYTW": DYPModel("3cm", "300cm", mode="CONTROLLED"),
    "A20NYUW": DYPModel("3cm", "300cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

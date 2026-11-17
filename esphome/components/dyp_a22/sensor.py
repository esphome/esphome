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
    "A22": DYPModel("2cm", "300cm", baud_rate=115200),
    "A22NYTW": DYPModel("2cm", "300cm", baud_rate=115200, mode="CONTROLLED"),
    "A22NYUW": DYPModel("2cm", "300cm", baud_rate=115200, mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

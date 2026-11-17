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
    "L08": DYPModel("5cm", "1000cm", baud_rate=115200, mode="CONTROLLED"),
    "L081MTW": DYPModel("5cm", "1000cm", baud_rate=115200, mode="CONTROLLED"),
    "L08B": DYPModel("8cm", "1000cm", baud_rate=115200, mode="CONTROLLED"),
    "L08B50TW": DYPModel("8cm", "1000cm", baud_rate=115200, mode="CONTROLLED"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

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
    "R01": DYPModel("2cm", "400cm", baud_rate=115200),
    "R01TW": DYPModel("2cm", "400cm", baud_rate=115200, mode="CONTROLLED"),
    "R01UW": DYPModel("2cm", "400cm", baud_rate=115200, mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

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
    "L02": DYPModel("2cm", "200cm", supports_rx_mode_select=False),
    "L023MTW": DYPModel("2cm", "200cm", mode="CONTROLLED"),
    "L023MUW": DYPModel("2cm", "200cm", mode="AUTO", supports_rx_mode_select=False),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

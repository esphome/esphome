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
    "DS1603DA-3U": DYPModel("4cm", "200cm", mode="AUTO", supports_rx_mode_select=False),
    "DS1603L": DYPModel("5cm", "200cm", mode="AUTO", supports_rx_mode_select=False),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

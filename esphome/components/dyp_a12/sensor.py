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
    "A12A": DYPModel("25cm", "500cm"),
    "A12ANYTW": DYPModel("25cm", "500cm", mode="CONTROLLED"),
    "A12ANYUW": DYPModel("25cm", "500cm", mode="AUTO"),
    "A12B": DYPModel("25cm", "500cm"),
    "A12BNYTW": DYPModel("25cm", "500cm", mode="CONTROLLED"),
    "A12BNYUW": DYPModel("25cm", "500cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

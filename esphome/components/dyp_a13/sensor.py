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
    "A13": DYPModel("25cm", "200cm"),
    "A13B": DYPModel("25cm", "200cm"),
    "A13BNYTW": DYPModel("25cm", "200cm", mode="CONTROLLED"),
    "A13BNYUW": DYPModel("25cm", "200cm", mode="AUTO"),
    "A13NYTW": DYPModel("25cm", "200cm", mode="CONTROLLED"),
    "A13NYUW": DYPModel("25cm", "200cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

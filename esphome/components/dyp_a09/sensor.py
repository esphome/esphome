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
    "A09A": DYPModel("20cm", "350cm"),
    "A09ANYTW": DYPModel("20cm", "350cm", mode="CONTROLLED"),
    "A09ANYUW": DYPModel("20cm", "350cm", mode="AUTO"),
    "A09B": DYPModel("28cm", "350cm"),
    "A09BNYTW": DYPModel("28cm", "350cm", mode="CONTROLLED"),
    "A09BNYUW": DYPModel("28cm", "350cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

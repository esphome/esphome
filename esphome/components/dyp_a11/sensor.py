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
    "A11A": DYPModel("21cm", "300cm"),
    "A11ANYTW": DYPModel("21cm", "300cm", mode="CONTROLLED"),
    "A11ANYUW": DYPModel("21cm", "300cm", mode="AUTO"),
    "A11B": DYPModel("23cm", "200cm"),
    "A11BNYTW": DYPModel("23cm", "200cm", mode="CONTROLLED"),
    "A11BNYUW": DYPModel("23cm", "200cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

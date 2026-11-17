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
    "A01A": DYPModel("28cm", "750cm"),
    "A01ANYTB": DYPModel("28cm", "750cm", mode="CONTROLLED"),
    "A01ANYUB": DYPModel("28cm", "750cm", mode="AUTO"),
    "A01B": DYPModel("28cm", "750cm"),
    "A01BNYTB": DYPModel("28cm", "750cm", mode="CONTROLLED"),
    "A01BNYUB": DYPModel("28cm", "750cm", mode="AUTO"),
    "A01BNYTW": DYPModel("28cm", "450cm", mode="CONTROLLED"),
    "A01BNYUW": DYPModel("28cm", "450cm", mode="AUTO"),
    "A01C": DYPModel("28cm", "250cm"),
    "A01CNYTB": DYPModel("28cm", "250cm", mode="CONTROLLED"),
    "A01CNYUB": DYPModel("28cm", "250cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

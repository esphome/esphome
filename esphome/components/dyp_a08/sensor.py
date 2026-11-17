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
    "A08A": DYPModel("25cm", "800cm"),
    "A08ANYTB": DYPModel("25cm", "800cm", mode="CONTROLLED"),
    "A08ANYUB": DYPModel("25cm", "800cm", mode="AUTO"),
    "A08B": DYPModel("25cm", "500cm"),
    "A08BNYTB": DYPModel("25cm", "500cm", mode="CONTROLLED"),
    "A08BNYUB": DYPModel("25cm", "500cm", mode="AUTO"),
    "A08C": DYPModel("25cm", "200cm", mode="AUTO"),
    "A08CNYUB": DYPModel("25cm", "200cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

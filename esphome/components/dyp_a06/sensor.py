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
    "A06B": DYPModel("30cm", "200cm"),
    "A06BLYT": DYPModel("30cm", "200cm", mode="CONTROLLED"),
    "A06BLYU": DYPModel("30cm", "200cm", mode="AUTO"),
    "A06BNYT": DYPModel("30cm", "200cm", mode="CONTROLLED"),
    "A06BNYU": DYPModel("30cm", "200cm", mode="AUTO"),
    "A06LYT": DYPModel("25cm", "600cm", mode="CONTROLLED"),
    "A06LYU": DYPModel("25cm", "600cm", mode="AUTO"),
    "A06NYT": DYPModel("25cm", "600cm", mode="CONTROLLED"),
    "A06NYU": DYPModel("25cm", "600cm", mode="AUTO"),
}

CONFIG_SCHEMA = dyp_sensor_schema(MODELS)
FINAL_VALIDATE_SCHEMA = dyp_final_validate_schema(DOMAIN, MODELS)
to_code = to_code_base

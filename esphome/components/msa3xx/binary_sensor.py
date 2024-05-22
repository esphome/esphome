# import esphome.codegen as cg
import esphome.config_validation as cv
from . import MSA3xxComponent, CONF_MSA3XX_ID

DEPENDENCIES = ["msa3xx"]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_MSA3XX_ID): cv.use_id(MSA3xxComponent),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


# async def to_code(config):

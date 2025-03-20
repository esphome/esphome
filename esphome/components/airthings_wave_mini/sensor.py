import esphome.codegen as cg
from esphome.components import airthings_wave_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = airthings_wave_base.DEPENDENCIES

AUTO_LOAD = ["airthings_wave_base"]

airthings_wave_mini_ns = cg.esphome_ns.namespace("airthings_wave_mini")
AirthingsWaveMini = airthings_wave_mini_ns.class_(
    "AirthingsWaveMini", airthings_wave_base.AirthingsWaveBase
)


CONFIG_SCHEMA = airthings_wave_base.BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AirthingsWaveMini),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await airthings_wave_base.wave_base_to_code(var, config)

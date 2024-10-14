import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_BATTERY_LEVEL, CONF_DEVICE, CONF_ID, DEVICE_CLASS_BATTERY

from .. import AcuRiteComponent, acurite_ns

DEPENDENCIES = ["acurite"]

CONF_ACURITE_ID = "acurite_id"
CONF_DEVICES = "devices"

AcuRiteBinarySensor = acurite_ns.class_("AcuRiteBinarySensor", cg.Component)

DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AcuRiteBinarySensor),
        cv.Required(CONF_DEVICE): cv.hex_int_range(max=0x3FFF),
        cv.Required(CONF_BATTERY_LEVEL): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ACURITE_ID): cv.use_id(AcuRiteComponent),
        cv.Required(CONF_DEVICES): cv.ensure_list(DEVICE_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ACURITE_ID])
    if devices_cfg := config.get(CONF_DEVICES):
        for device_cfg in devices_cfg:
            var = cg.new_Pvariable(device_cfg[CONF_ID])
            await cg.register_component(var, device_cfg)
            cg.add(var.set_id(device_cfg[CONF_DEVICE]))
            if CONF_BATTERY_LEVEL in device_cfg:
                sens = await binary_sensor.new_binary_sensor(
                    device_cfg[CONF_BATTERY_LEVEL]
                )
                cg.add(var.set_battery_level_binary_sensor(sens))
            cg.add(parent.add_device(var, device_cfg[CONF_DEVICE]))

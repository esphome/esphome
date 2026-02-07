import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.cpp_generator import MockObjClass

from .. import CONF_PARENT_ID, CaravanDeviceComponent, FendtCaravan, fendt_caravan_ns

ControlUnitDeviceSensor = fendt_caravan_ns.class_(
    "ControlUnitDeviceSensor",
    CaravanDeviceComponent,
    sensor.Sensor,
    cg.Parented.template(FendtCaravan),
)


def _device_schema(class_: MockObjClass) -> cv.Schema:
    return (
        sensor.sensor_schema(
            class_,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend({cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravan)})
    ).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.typed_schema({"mcu": _device_schema(ControlUnitDeviceSensor)})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_device")(var))

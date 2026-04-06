from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_POWER,
    CONF_POWER_MODE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

DEPENDENCIES = ["i2c"]

hdc302x_ns = cg.esphome_ns.namespace("hdc302x")
HDC302XComponent = hdc302x_ns.class_(
    "HDC302XComponent", cg.PollingComponent, i2c.I2CDevice
)

HDC302XPowerMode = hdc302x_ns.enum("HDC302XPowerMode")
POWER_MODE_OPTIONS = {
    "HIGH_ACCURACY": HDC302XPowerMode.HIGH_ACCURACY,
    "BALANCED": HDC302XPowerMode.BALANCED,
    "LOW_POWER": HDC302XPowerMode.LOW_POWER,
    "ULTRA_LOW_POWER": HDC302XPowerMode.ULTRA_LOW_POWER,
}

# Actions
HeaterOnAction = hdc302x_ns.class_("HeaterOnAction", automation.Action)
HeaterOffAction = hdc302x_ns.class_("HeaterOffAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HDC302XComponent),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_MODE, default="HIGH_ACCURACY"): cv.enum(
                POWER_MODE_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x44))  # Default address per datasheet, Table 7-2.
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if temp_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temp_config)
        cg.add(var.set_temp_sensor(sens))

    if humidity_config := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(humidity_config)
        cg.add(var.set_humidity_sensor(sens))

    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))


# HDC302x heater power configs, per datasheet Table 7-15.
HDC302X_HEATER_POWER_MAP = {
    "QUARTER": 0x009F,
    "HALF": 0x03FF,
    "FULL": 0x3FFF,
}


def heater_power_value(value):
    """Accept enum names or raw uint16 values"""
    if isinstance(value, cv.Lambda):
        return value
    if isinstance(value, str):
        upper = value.upper()
        if upper in HDC302X_HEATER_POWER_MAP:
            return HDC302X_HEATER_POWER_MAP[upper]
        raise cv.Invalid(
            f"Unknown heater power preset: {value}. Use QUARTER, HALF, FULL, or a raw value 0-16383"
        )
    return cv.int_range(min=0, max=0x3FFF)(value)


HDC302X_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(HDC302XComponent)})

HDC302X_HEATER_ON_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(HDC302XComponent),
        cv.Optional(CONF_POWER, default="QUARTER"): cv.templatable(heater_power_value),
        cv.Optional(CONF_DURATION, default="5s"): cv.templatable(
            cv.positive_time_period_milliseconds
        ),
    }
)


@automation.register_action(
    "hdc302x.heater_on",
    HeaterOnAction,
    HDC302X_HEATER_ON_ACTION_SCHEMA,
    synchronous=True,
)
async def hdc302x_heater_on_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_POWER], args, cg.uint16)
    cg.add(var.set_power(template_))
    template_ = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(template_))
    return var


@automation.register_action(
    "hdc302x.heater_off",
    HeaterOffAction,
    HDC302X_ACTION_SCHEMA,
    synchronous=True,
)
async def hdc302x_heater_off_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var

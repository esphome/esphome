from esphome.components import sensor, i2c
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_ENERGY,
    STATE_CLASS_MEASUREMENT,
    UNIT_WATT,
)

CODEOWNERS = ["@flaviut", "@Maelstrom96", "@krconv"]
ESP_PLATFORMS = ["esp-idf"]
DEPENDENCIES = ["i2c"]
AUTOLOAD = ["sensor"]

emporia_vue_ns = cg.esphome_ns.namespace("emporia_vue")
EmporiaVueComponent = emporia_vue_ns.class_(
    "EmporiaVueComponent", cg.PollingComponent, i2c.I2CDevice
)
PhaseConfig = emporia_vue_ns.class_("PhaseConfig")
PowerSensor = emporia_vue_ns.class_("PowerSensor", sensor.Sensor)

PhaseInputColor = emporia_vue_ns.enum("PhaseInputColor")
PHASE_INPUT = {
    "BLACK": PhaseInputColor.BLACK,
    "RED": PhaseInputColor.RED,
    "BLUE": PhaseInputColor.BLUE,
}

CTInputPort = emporia_vue_ns.enum("CTInputPort")
CT_INPUT = {
    "A": CTInputPort.A,
    "B": CTInputPort.B,
    "C": CTInputPort.C,
    "1": CTInputPort.ONE,
    "2": CTInputPort.TWO,
    "3": CTInputPort.THREE,
    "4": CTInputPort.FOUR,
    "5": CTInputPort.FIVE,
    "6": CTInputPort.SIX,
    "7": CTInputPort.SEVEN,
    "8": CTInputPort.EIGHT,
    "9": CTInputPort.NINE,
    "10": CTInputPort.TEN,
    "11": CTInputPort.ELEVEN,
    "12": CTInputPort.TWELVE,
    "13": CTInputPort.THIRTEEN,
    "14": CTInputPort.FOURTEEN,
    "15": CTInputPort.FIFTEEN,
    "16": CTInputPort.SIXTEEN,
}

CONF_PHASES = "phases"
CONF_PHASE_ID = "phase_id"
CONF_PHASE_INPUT = "phase_input"
CONF_CT_INPUT = "ct_input"
CONF_POWER = "power"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmporiaVueComponent),
            cv.Required(CONF_PHASES): cv.ensure_list(
                {
                    cv.Required(CONF_ID): cv.declare_id(PhaseConfig),
                    cv.Required(CONF_PHASE_INPUT): cv.enum(PHASE_INPUT),
                }
            ),
            cv.Required(CONF_POWER): cv.ensure_list(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_WATT,
                    device_class=DEVICE_CLASS_ENERGY,
                    state_class=STATE_CLASS_MEASUREMENT,
                ).extend(
                    {
                        cv.GenerateID(): cv.declare_id(PowerSensor),
                        cv.Required(CONF_PHASE_ID): cv.use_id(PhaseConfig),
                        cv.Required(CONF_CT_INPUT): cv.enum(CT_INPUT),
                    }
                )
            ),
        },
        # cv.only_with_esp_idf,
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x64))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    phases = []
    for phase_config in config[CONF_PHASES]:
        phase_var = cg.new_Pvariable(phase_config[CONF_ID], PhaseConfig())
        cg.add(phase_var.set_input_color(phase_config[CONF_PHASE_INPUT]))

        phases.append(phase_var)
    cg.add(var.set_phases(phases))

    power_sensors = []
    for power_config in config[CONF_POWER]:
        power_var = cg.new_Pvariable(power_config[CONF_ID], PowerSensor())
        phase_var = await cg.get_variable(power_config[CONF_PHASE_ID])
        cg.add(power_var.set_phase(phase_var))

        await sensor.register_sensor(power_var, power_config)

        power_sensors.append(power_var)
    cg.add(var.set_power_sensors(power_sensors))

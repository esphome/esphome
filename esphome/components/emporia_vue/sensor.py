from esphome.components import sensor, i2c
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_CALIBRATION,
    #    CONF_CT,
    CONF_ID,
    CONF_INPUT,
    #    CONF_PHASES,
    #    CONF_PHASE_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_WATT,
    UNIT_VOLT,
)

# TODO: Remove this - It's only added so we can test the component using the External_component configuration
CONF_CT = "ct"
CONF_PHASES = "phases"
CONF_PHASE_ID = "phase_id"

CODEOWNERS = ["@flaviut", "@Maelstrom96", "@krconv"]
ESP_PLATFORMS = ["esp-idf"]
DEPENDENCIES = ["i2c"]
AUTOLOAD = ["sensor"]

emporia_vue_ns = cg.esphome_ns.namespace("emporia_vue")
EmporiaVueComponent = emporia_vue_ns.class_(
    "EmporiaVueComponent", cg.Component, i2c.I2CDevice
)
PhaseConfig = emporia_vue_ns.class_("PhaseConfig")
CTSensor = emporia_vue_ns.class_("CTSensor", sensor.Sensor)

PhaseInputColor = emporia_vue_ns.enum("PhaseInputWire")
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

SCHEMA_CT = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(): cv.declare_id(CTSensor),
        cv.Required(CONF_PHASE_ID): cv.use_id(PhaseConfig),
        cv.Required(CONF_INPUT): cv.enum(CT_INPUT),
        cv.Optional(CONF_CALIBRATION, default=0.022): cv.zero_to_one_float,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmporiaVueComponent),
            cv.Required(CONF_PHASES): cv.ensure_list(
                {
                    cv.Required(CONF_ID): cv.declare_id(PhaseConfig),
                    cv.Required(CONF_INPUT): cv.enum(PHASE_INPUT),
                    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                        UNIT_VOLT,
                        ICON_EMPTY,
                        1,
                        DEVICE_CLASS_VOLTAGE,
                        STATE_CLASS_MEASUREMENT,
                    ),
                }
            ),
            cv.Required(CONF_CT): cv.ensure_list(SCHEMA_CT),
        },
        # cv.only_with_esp_idf,
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x64))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    phases = []
    for phase_config in config[CONF_PHASES]:
        phase_var = cg.new_Pvariable(phase_config[CONF_ID], PhaseConfig())
        cg.add(phase_var.set_input_wire(phase_config[CONF_INPUT]))
        cg.add(phase_var.set_calibration(phase_config[CONF_CALIBRATION]))

        if CONF_VOLTAGE in phase_config:
            voltage_sensor = await sensor.new_sensor(phase_config[CONF_VOLTAGE])
            cg.add(var.set_voltage_sensor(voltage_sensor))

        phases.append(phase_var)
    cg.add(var.set_phases(phases))

    ct_sensors = []
    for ct_config in config[CONF_CT]:
        power_var = cg.new_Pvariable(ct_config[CONF_ID], CTSensor())
        phase_var = await cg.get_variable(ct_config[CONF_PHASE_ID])
        cg.add(power_var.set_phase(phase_var))
        cg.add(power_var.set_ct_input(ct_config[CONF_INPUT]))

        await sensor.register_sensor(power_var, ct_config)

        ct_sensors.append(power_var)
    cg.add(var.set_ct_sensors(ct_sensors))

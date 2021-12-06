from esphome.components import sensor, i2c
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components.ota import OTAComponent
from esphome.const import (
    CONF_CALIBRATION,
    CONF_ID,
    CONF_INPUT,
    CONF_OTA,
    CONF_VOLTAGE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_WATT,
    UNIT_VOLT,
)

CONF_CT_CLAMPS = "ct_clamps"
CONF_PHASES = "phases"
CONF_PHASE_ID = "phase_id"
CONF_SENSOR_POLL_INTERVAL = "sensor_poll_interval"

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

PhaseInputWire = emporia_vue_ns.enum("PhaseInputWire")
PHASE_INPUT = {
    "BLACK": PhaseInputWire.BLACK,
    "RED": PhaseInputWire.RED,
    "BLUE": PhaseInputWire.BLUE,
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

SCHEMA_CT_CLAMP = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(): cv.declare_id(CTSensor),
        cv.Required(CONF_PHASE_ID): cv.use_id(PhaseConfig),
        cv.Required(CONF_INPUT): cv.enum(CT_INPUT),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmporiaVueComponent),
            cv.OnlyWith(CONF_OTA, "ota"): cv.use_id(OTAComponent),
            cv.Optional(
                CONF_SENSOR_POLL_INTERVAL, default="240ms"
            ): cv.positive_time_period_milliseconds,
            cv.Required(CONF_PHASES): cv.ensure_list(
                {
                    cv.Required(CONF_ID): cv.declare_id(PhaseConfig),
                    cv.Required(CONF_INPUT): cv.enum(PHASE_INPUT),
                    cv.Optional(CONF_CALIBRATION, default=0.022): cv.zero_to_one_float,
                    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                        unit_of_measurement=UNIT_VOLT,
                        device_class=DEVICE_CLASS_VOLTAGE,
                        state_class=STATE_CLASS_MEASUREMENT,
                        accuracy_decimals=1,
                    ),
                }
            ),
            cv.Required(CONF_CT_CLAMPS): cv.ensure_list(SCHEMA_CT_CLAMP),
        },
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x64)),
    cv.only_with_esp_idf,
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_sensor_poll_interval(config[CONF_SENSOR_POLL_INTERVAL]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    phases = []
    for phase_config in config[CONF_PHASES]:
        phase_var = cg.new_Pvariable(phase_config[CONF_ID], PhaseConfig())
        cg.add(phase_var.set_input_wire(phase_config[CONF_INPUT]))
        cg.add(phase_var.set_calibration(phase_config[CONF_CALIBRATION]))

        if CONF_VOLTAGE in phase_config:
            voltage_sensor = await sensor.new_sensor(phase_config[CONF_VOLTAGE])
            cg.add(phase_var.set_voltage_sensor(voltage_sensor))

        phases.append(phase_var)
    cg.add(var.set_phases(phases))

    ct_sensors = []
    for ct_config in config[CONF_CT_CLAMPS]:
        power_var = cg.new_Pvariable(ct_config[CONF_ID], CTSensor())
        phase_var = await cg.get_variable(ct_config[CONF_PHASE_ID])
        cg.add(power_var.set_phase(phase_var))
        cg.add(power_var.set_input_port(ct_config[CONF_INPUT]))

        await sensor.register_sensor(power_var, ct_config)

        ct_sensors.append(power_var)
    cg.add(var.set_ct_sensors(ct_sensors))

    if CONF_OTA in config:
        ota = await cg.get_variable(config[CONF_OTA])
        cg.add_define("USING_OTA_COMPONENT")
        cg.add_define("USE_OTA_STATE_CALLBACK")
        cg.add(var.set_ota(ota))

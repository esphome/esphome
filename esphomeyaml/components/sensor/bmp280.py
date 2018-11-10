import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_IIR_FILTER, CONF_MAKE_ID, \
    CONF_NAME, CONF_OVERSAMPLING, CONF_PRESSURE, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable, setup_component

DEPENDENCIES = ['i2c']

BMP280Oversampling = sensor.sensor_ns.enum('BMP280Oversampling')
OVERSAMPLING_OPTIONS = {
    'NONE': BMP280Oversampling.BMP280_OVERSAMPLING_NONE,
    '1X': BMP280Oversampling.BMP280_OVERSAMPLING_1X,
    '2X': BMP280Oversampling.BMP280_OVERSAMPLING_2X,
    '4X': BMP280Oversampling.BMP280_OVERSAMPLING_4X,
    '8X': BMP280Oversampling.BMP280_OVERSAMPLING_8X,
    '16X': BMP280Oversampling.BMP280_OVERSAMPLING_16X,
}

BMP280IIRFilter = sensor.sensor_ns.enum('BMP280IIRFilter')
IIR_FILTER_OPTIONS = {
    'OFF': BMP280IIRFilter.BMP280_IIR_FILTER_OFF,
    '2X': BMP280IIRFilter.BMP280_IIR_FILTER_2X,
    '4X': BMP280IIRFilter.BMP280_IIR_FILTER_4X,
    '8X': BMP280IIRFilter.BMP280_IIR_FILTER_8X,
    '16X': BMP280IIRFilter.BMP280_IIR_FILTER_16X,
}

BMP280_OVERSAMPLING_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): vol.All(vol.Upper, cv.one_of(*OVERSAMPLING_OPTIONS)),
})

MakeBMP280Sensor = Application.struct('MakeBMP280Sensor')
BMP280TemperatureSensor = sensor.sensor_ns.class_('BMP280TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BMP280PressureSensor = sensor.sensor_ns.class_('BMP280PressureSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeBMP280Sensor),
    vol.Optional(CONF_ADDRESS, default=0x77): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(BMP280_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP280TemperatureSensor),
    })),
    vol.Required(CONF_PRESSURE): cv.nameable(BMP280_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP280PressureSensor),
    })),
    vol.Optional(CONF_IIR_FILTER): vol.All(vol.Upper, cv.one_of(*IIR_FILTER_OPTIONS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_bmp280_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    bmp280 = make.Pbmp280
    if CONF_OVERSAMPLING in config[CONF_TEMPERATURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_TEMPERATURE][CONF_OVERSAMPLING]]
        add(bmp280.set_temperature_oversampling(constant))
    if CONF_OVERSAMPLING in config[CONF_PRESSURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_PRESSURE][CONF_OVERSAMPLING]]
        add(bmp280.set_pressure_oversampling(constant))
    if CONF_IIR_FILTER in config:
        constant = IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]
        add(bmp280.set_iir_filter(constant))

    sensor.setup_sensor(bmp280.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(bmp280.Pget_pressure_sensor(), make.Pmqtt_pressure,
                        config[CONF_PRESSURE])
    setup_component(bmp280, config)


BUILD_FLAGS = '-DUSE_BMP280'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE])]

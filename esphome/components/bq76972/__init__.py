import esphome.codegen as cg
from esphome.components import i2c, number, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@limpkin"]
AUTO_LOAD = ["number", "sensor"]

bq76972_ns = cg.esphome_ns.namespace("bq76972")
BQ76972Component = bq76972_ns.class_(
    "BQ76972Component", cg.PollingComponent, i2c.I2CDevice
)
BQ76972AddressNumber = bq76972_ns.class_("BQ76972AddressNumber", number.Number)

CONF_REG_DISABLED = "reg_disabled"
CONF_CRC_ENABLED = "crc_enabled"
CONF_STACK_VOLTAGE = "stack_voltage"
CONF_I2C_ADDRESS_SETTER = "i2c_address_setter"

TEMP_SENSORS = {
    "internal_temperature": "set_internal_temp_sensor",
    "cfetoff_temperature": "set_cfetoff_temp_sensor",
    "dfetoff_temperature": "set_dfetoff_temp_sensor",
    "alert_temperature": "set_alert_temp_sensor",
    "ts1_temperature": "set_ts1_temp_sensor",
    "ts2_temperature": "set_ts2_temp_sensor",
    "ts3_temperature": "set_ts3_temp_sensor",
    "hdq_temperature": "set_hdq_temp_sensor",
    "dchg_temperature": "set_dchg_temp_sensor",
    "ddsg_temperature": "set_ddsg_temp_sensor",
}

CONF_CELL_VOLTAGES = [f"cell_{i}_voltage" for i in range(1, 17)]

voltage_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)

temp_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.ensure_list(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(BQ76972Component),
            cv.Optional(CONF_CRC_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_REG_DISABLED, default=True): cv.boolean,
            cv.Optional(CONF_STACK_VOLTAGE): voltage_sensor_schema,
            cv.Optional(CONF_I2C_ADDRESS_SETTER): number.number_schema(
                BQ76972AddressNumber
            ),
        }
    )
    .extend(
        cv.Schema(
            {
                cv.Optional(cell_key): voltage_sensor_schema
                for cell_key in CONF_CELL_VOLTAGES
            }
        )
    )
    .extend(
        cv.Schema(
            {cv.Optional(temp_key): temp_sensor_schema for temp_key in TEMP_SENSORS}
        )
    )
    .extend(i2c.i2c_device_schema(0x08))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    for conf in config:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await i2c.register_i2c_device(var, conf)

        cg.add(var.set_address(conf[CONF_ADDRESS]))
        cg.add(var.set_crc_mode(conf[CONF_CRC_ENABLED]))
        cg.add(var.set_reg_disable(conf[CONF_REG_DISABLED]))
        cg.add(var.set_component_id(str(conf[CONF_ID])))

        if CONF_STACK_VOLTAGE in conf:
            sens = await sensor.new_sensor(conf[CONF_STACK_VOLTAGE])
            cg.add(var.set_stack_voltage_sensor(sens))

        for temp_key, setter in TEMP_SENSORS.items():
            if temp_key in conf:
                sens = await sensor.new_sensor(conf[temp_key])
                cg.add(getattr(var, setter)(sens))

        for idx, cell_key in enumerate(CONF_CELL_VOLTAGES):
            if cell_key in conf:
                sens = await sensor.new_sensor(conf[cell_key])
                cg.add(var.set_cell_sensor(idx, sens))

        if CONF_I2C_ADDRESS_SETTER in conf:
            n_var = await number.new_number(
                conf[CONF_I2C_ADDRESS_SETTER],
                min_value=8,
                max_value=119,
                step=1,
            )
            cg.add(n_var.set_hub(var))
            cg.add(var.set_address_number(n_var))

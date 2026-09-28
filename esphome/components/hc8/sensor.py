from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_BASELINE,
    CONF_CO2,
    CONF_ID,
    CONF_WARMUP_TIME,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

hc8_ns = cg.esphome_ns.namespace("hc8")
HC8Component = hc8_ns.class_("HC8Component", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HC8Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(
                CONF_WARMUP_TIME, default="75s"
            ): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hc8",
    baud_rate=9600,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if co2 := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2)
        cg.add(var.set_co2_sensor(sens))

    cg.add(var.set_warmup_seconds(config[CONF_WARMUP_TIME]))


CALIBRATION_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(HC8Component),
        cv.Required(CONF_BASELINE): cv.templatable(cv.uint16_t),
    }
)


automation.register_apply_action(
    "hc8.calibrate",
    CALIBRATION_ACTION_SCHEMA,
    automation.ApplyField(CONF_BASELINE, "calibrate", cg.uint16),
)

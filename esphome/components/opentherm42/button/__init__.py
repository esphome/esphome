import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from .. import OpenTherm42Hub, opentherm42_ns
from ..const import (
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2,
    CONF_OPENTHERM42_ID,
    CONF_REMOTE_REQUEST_AUTOMATIC_HYDRONIC_AIR_PURGE,
    CONF_REMOTE_REQUEST_BACK_TO_NORMAL_OPERATION_MODE,
    CONF_REMOTE_REQUEST_BOILER_LOCKOUT_RESET,
    CONF_REMOTE_REQUEST_CH_WATER_FILLING,
    CONF_REMOTE_REQUEST_RESET_SERVICE_REQUEST_FLAG,
    CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_CH,
    CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_DHW,
    CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MAXIMUM_SPEED,
    CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MINIMUM_SPEED,
    CONF_REMOTE_REQUEST_SERVICE_MODE_MAXIMUM_POWER,
    CONF_REMOTE_REQUEST_SERVICE_MODE_MINIMUM_POWER,
    CONF_REMOTE_REQUEST_SERVICE_MODE_SPARK_TEST,
    CONF_REMOTE_REQUEST_SERVICE_TEST_1,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES_RESET,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS_RESET,
)

OpenTherm42RemoteRequestButton = opentherm42_ns.class_(
    "OpenTherm42RemoteRequestButton", button.Button, cg.Component
)
OpenTherm42ManualDhwPush2Button = opentherm42_ns.class_(
    "OpenTherm42ManualDhwPush2Button", button.Button, cg.Component
)
OpenTherm42ResetCounterButton = opentherm42_ns.class_(
    "OpenTherm42ResetCounterButton", button.Button, cg.Component
)

# §5.3.3 Class 3, ID 4 HB: Request-Code. Pressing a button sends WRITE-DATA(id=4, code, 00); the
# boiler's WRITE-ACK reports acceptance via the remote_request_last_response_code sensor.
CODES: dict[str, int] = {
    # 0: Back to Normal operation mode
    CONF_REMOTE_REQUEST_BACK_TO_NORMAL_OPERATION_MODE: 0,
    # 1: "BLOR" = Boiler Lock-out Reset request
    CONF_REMOTE_REQUEST_BOILER_LOCKOUT_RESET: 1,
    # 2: "CHWF" = CH water filling request
    CONF_REMOTE_REQUEST_CH_WATER_FILLING: 2,
    # 3: Service mode maximum power request (for instance for CO2 measurement during Chimney Sweep Function)
    CONF_REMOTE_REQUEST_SERVICE_MODE_MAXIMUM_POWER: 3,
    # 4: Service mode minimum power request (CO2 measurement)
    CONF_REMOTE_REQUEST_SERVICE_MODE_MINIMUM_POWER: 4,
    # 5: Service mode spark test request (no gas)
    CONF_REMOTE_REQUEST_SERVICE_MODE_SPARK_TEST: 5,
    # 6: Service mode fan maximum speed request (no flame)
    CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MAXIMUM_SPEED: 6,
    # 7: Service mode fan to minimum speed request (no flame)
    CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MINIMUM_SPEED: 7,
    # 8: Service mode 3-way valve to CH request (no pump, no flame)
    CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_CH: 8,
    # 9: Service mode 3-way valve to DHW request (no pump, no flame)
    CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_DHW: 9,
    # 10: Request to reset service request flag
    CONF_REMOTE_REQUEST_RESET_SERVICE_REQUEST_FLAG: 10,
    # 11: Service test 1. This is an OEM specific test.
    CONF_REMOTE_REQUEST_SERVICE_TEST_1: 11,
    # 12: Automatic hydronic air purge.
    CONF_REMOTE_REQUEST_AUTOMATIC_HYDRONIC_AIR_PURGE: 12,
}

# §5.3.4 Class 4: these 14 counter/hour ids are all "R W" with reset-by-writing-zero optional for the
# boiler; pressing a button sends WRITE-DATA(id, 0x0000, 0x0000). ID 111 (Electricity production) is
# read-only and has no reset button.
RESETTABLE_COUNTERS: dict[str, int] = {
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS_RESET: 96,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES_RESET: 97,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS_RESET: 109,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS_RESET: 110,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION_RESET: 112,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS_RESET: 113,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW_RESET: 114,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS_RESET: 116,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS_RESET: 117,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS_RESET: 118,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS_RESET: 119,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS_RESET: 120,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS_RESET: 121,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS_RESET: 122,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS_RESET: 123,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{
            cv.Optional(marker): button.button_schema(
                OpenTherm42RemoteRequestButton
            ).extend(cv.COMPONENT_SCHEMA)
            for marker in CODES
        },
        # §5.3.8.3 Class 8, ID 99 HB bit 4: Manual DHW push2 -- rises the DHW temperature once to
        # Comfort level and returns to the previous Operating Mode.
        cv.Optional(
            CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2
        ): button.button_schema(OpenTherm42ManualDhwPush2Button).extend(
            cv.COMPONENT_SCHEMA
        ),
        **{
            cv.Optional(marker): button.button_schema(
                OpenTherm42ResetCounterButton
            ).extend(cv.COMPONENT_SCHEMA)
            for marker in RESETTABLE_COUNTERS
        },
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker, code in CODES.items():
        if (marker_config := config.get(marker)) is not None:
            var = await button.new_button(marker_config, hub, code)
            await cg.register_component(var, marker_config)

    if (
        marker_config := config.get(
            CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2
        )
    ) is not None:
        var = await button.new_button(marker_config, hub)
        await cg.register_component(var, marker_config)

    for marker, data_id in RESETTABLE_COUNTERS.items():
        if (marker_config := config.get(marker)) is not None:
            var = await button.new_button(marker_config, hub, data_id)
            await cg.register_component(var, marker_config)

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_COMMAND, CONF_ID, CONF_SOURCE, CONF_UART_ID, CONF_VALUE
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]
DEPENDENCIES = ["uart"]
DOMAIN = "vbus"
MULTI_CONF = True
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"vbus_tx.cpp": "USE_VBUS_TX", "vbus_transactions.cpp": "USE_VBUS_TRANSACTIONS"}
)

vbus_ns = cg.esphome_ns.namespace("vbus")
VBus = vbus_ns.class_("VBus", uart.UARTDevice, cg.Component)
VBusTransmitter = vbus_ns.class_("VBusTransmitter", uart.UARTDevice, cg.Component)
VBusTransactions = vbus_ns.class_("VBusTransactions", cg.Component)
TransactionStatus = vbus_ns.enum("TransactionStatus", is_class=True)

CONF_VBUS_ID = "vbus_id"
CONF_ON_DATAGRAM = "on_datagram"
CONF_TRANSMIT = "transmit"
CONF_DESTINATION = "destination"
CONF_LEASE_DURATION = "lease_duration"
CONF_ON_BUS_GRANT = "on_bus_grant"
CONF_VALUE_ID = "value_id"
CONF_GRANT_ID = "grant_id"
CONF_EXPECT_REPLY = "expect_reply"
CONF_TRANSACTIONS = "transactions"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_ON_RESULT = "on_result"


def validate_address(value: object) -> int:
    address = cv.uint16_t(value)
    if address == 0 or address & 0x8080:
        raise cv.Invalid(
            "A VBus address must be nonzero and have both byte high bits clear"
        )
    return address


def validate_transmit(config: ConfigType) -> ConfigType:
    if config[CONF_SOURCE] == config[CONF_DESTINATION]:
        raise cv.Invalid("VBus source and controller destination must differ")
    return config


def validate_command(value: object) -> int:
    command = cv.uint16_t(value)
    if command & 0x8080 or command in (0x0500, 0x0600):
        raise cv.Invalid(
            "Command must have both byte high bits clear; bus grants and releases are managed internally"
        )
    return command


TRANSACTIONS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VBusTransactions),
        cv.Optional(CONF_RESPONSE_TIMEOUT, default="100ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(
                min=cv.TimePeriod(milliseconds=20), max=cv.TimePeriod(milliseconds=1000)
            ),
        ),
        cv.Optional(CONF_ON_RESULT): automation.validate_automation({}),
    }
)

TRANSMIT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VBusTransmitter),
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_SOURCE, default=0x0020): validate_address,
            cv.Required(CONF_DESTINATION): validate_address,
            cv.Optional(CONF_LEASE_DURATION, default="500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=200),
                    max=cv.TimePeriod(milliseconds=1000),
                ),
            ),
            cv.Optional(CONF_ON_BUS_GRANT): automation.validate_automation({}),
            cv.Optional(CONF_TRANSACTIONS): TRANSACTIONS_SCHEMA,
        }
    ),
    validate_transmit,
)

CONF_DELTASOL_BS_PLUS = "deltasol_bs_plus"
CONF_DELTASOL_BS_2009 = "deltasol_bs_2009"
CONF_DELTASOL_BS2 = "deltasol_bs2"
CONF_DELTASOL_C = "deltasol_c"
CONF_DELTASOL_CS2 = "deltasol_cs2"
CONF_DELTASOL_CS4 = "deltasol_cs4"
CONF_DELTASOL_CS_PLUS = "deltasol_cs_plus"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(VBus),
        cv.Optional(CONF_ON_DATAGRAM): automation.validate_automation({}),
        cv.Optional(CONF_TRANSMIT): TRANSMIT_SCHEMA,
    }
)


def final_validate(config: ConfigType) -> None:
    transmit = config.get(CONF_TRANSMIT)
    if str(config[CONF_ID]) in CORE.data.get(DOMAIN, {}).get(
        "transaction_actions", set()
    ) and (transmit is None or CONF_TRANSACTIONS not in transmit):
        raise cv.Invalid("VBus value actions require transmit.transactions on this hub")
    if transmit is None and str(config[CONF_ID]) in CORE.data.get(DOMAIN, {}).get(
        "tx_actions", set()
    ):
        raise cv.Invalid("VBus actions require transmit to be configured on this hub")
    same_uart = transmit is not None and (
        CONF_UART_ID not in transmit or transmit[CONF_UART_ID] == config[CONF_UART_ID]
    )
    uart.final_validate_device_schema(
        "vbus",
        baud_rate=9600,
        data_bits=8,
        parity="NONE",
        stop_bits=1,
        require_rx=transmit is not None,
        require_tx=same_uart,
    )(config)
    if transmit is not None and not same_uart:
        uart.final_validate_device_schema(
            "vbus transmit",
            baud_rate=9600,
            data_bits=8,
            parity="NONE",
            stop_bits=1,
            require_tx=True,
        )(transmit)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    if datagram_automations := config.get(CONF_ON_DATAGRAM):
        cg.add_define("USE_VBUS_DATAGRAM")
        for conf in datagram_automations:
            await automation.build_callback_automation(
                var,
                "add_on_datagram_callback",
                [
                    (cg.uint16, "command"),
                    (cg.uint16, "source"),
                    (cg.uint16, "destination"),
                    (cg.uint16, "value_id"),
                    (cg.uint32, "value"),
                ],
                conf,
            )

    if transmit := config.get(CONF_TRANSMIT):
        cg.add_define("USE_VBUS_TX")
        tx = cg.new_Pvariable(transmit[CONF_ID], var)
        await cg.register_component(tx, transmit)
        await uart.register_uart_device(
            tx, {CONF_UART_ID: transmit.get(CONF_UART_ID, config[CONF_UART_ID])}
        )
        cg.add(var.set_transmitter(tx))
        cg.add(tx.set_source(transmit[CONF_SOURCE]))
        cg.add(tx.set_destination(transmit[CONF_DESTINATION]))
        cg.add(tx.set_lease_duration(transmit[CONF_LEASE_DURATION]))
        if transactions := transmit.get(CONF_TRANSACTIONS):
            cg.add_define("USE_VBUS_TRANSACTIONS")
            cg.add_define("USE_VBUS_DATAGRAM")
            manager = cg.new_Pvariable(transactions[CONF_ID], var, tx)
            await cg.register_component(manager, transactions)
            cg.add(var.set_transactions(manager))
            cg.add(manager.set_response_timeout(transactions[CONF_RESPONSE_TIMEOUT]))
            for conf in transactions.get(CONF_ON_RESULT, []):
                await automation.build_callback_automation(
                    manager,
                    "add_on_result_callback",
                    [
                        (cg.uint32, "grant_id"),
                        (cg.uint16, "command"),
                        (cg.uint16, "value_id"),
                        (cg.uint32, "value"),
                        (TransactionStatus, "status"),
                    ],
                    conf,
                )
        for conf in transmit.get(CONF_ON_BUS_GRANT, []):
            await automation.build_callback_automation(
                tx, "add_on_bus_grant_callback", [(cg.uint32, "grant_id")], conf
            )


def validate_tx_action(config: ConfigType) -> ConfigType:
    CORE.data.setdefault(DOMAIN, {}).setdefault("tx_actions", set()).add(
        str(config[CONF_ID])
    )
    return config


TX_ID_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(VBus)})
TX_ACTION_SCHEMA = cv.All(TX_ID_SCHEMA, validate_tx_action)


def validate_send(config: ConfigType) -> ConfigType:
    if (
        config[CONF_COMMAND] in (0x0200, 0x0300, 0x0400)
        and config[CONF_EXPECT_REPLY] is False
    ):
        raise cv.Invalid(
            "Standard P2 read/write requests require expect_reply to be true"
        )
    return config


SEND_SCHEMA = cv.All(
    TX_ID_SCHEMA.extend(
        {
            cv.Required(CONF_GRANT_ID): cv.templatable(
                cv.All(cv.uint32_t, cv.Range(min=1))
            ),
            cv.Required(CONF_COMMAND): cv.templatable(validate_command),
            cv.Optional(CONF_VALUE_ID, default=0): cv.templatable(cv.uint16_t),
            cv.Optional(CONF_VALUE, default=0): cv.templatable(cv.uint32_t),
            cv.Optional(CONF_EXPECT_REPLY, default=True): cv.templatable(cv.boolean),
        }
    ),
    validate_send,
    validate_tx_action,
)

automation.register_apply_action(
    "vbus.request_bus", TX_ACTION_SCHEMA, automation.ApplyCall("request_bus()")
)
automation.register_apply_action(
    "vbus.release_bus",
    cv.All(
        TX_ID_SCHEMA.extend(
            {cv.Optional(CONF_GRANT_ID, default=0): cv.templatable(cv.uint32_t)}
        ),
        validate_tx_action,
    ),
    automation.ApplyCall("release_bus({})", ((CONF_GRANT_ID, cg.uint32),)),
)

automation.register_apply_action(
    "vbus.send_datagram",
    SEND_SCHEMA,
    automation.ApplyCall(
        "send_datagram({}, {}, {}, {}, {})",
        (
            (CONF_GRANT_ID, cg.uint32),
            (CONF_COMMAND, cg.uint16),
            (CONF_VALUE_ID, cg.uint16),
            (CONF_VALUE, cg.uint32),
            (CONF_EXPECT_REPLY, cg.bool_),
        ),
    ),
)


def validate_transaction_action(config: ConfigType) -> ConfigType:
    CORE.data.setdefault(DOMAIN, {}).setdefault("transaction_actions", set()).add(
        str(config[CONF_ID])
    )
    return config


VALUE_SCHEMA = TX_ID_SCHEMA.extend(
    {
        cv.Required(CONF_GRANT_ID): cv.templatable(
            cv.All(cv.uint32_t, cv.Range(min=1))
        ),
        cv.Required(CONF_VALUE_ID): cv.templatable(cv.uint16_t),
    }
)

READ_VALUE_SCHEMA = cv.All(VALUE_SCHEMA, validate_transaction_action)
WRITE_VALUE_SCHEMA = cv.All(
    VALUE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.uint32_t),
            cv.Optional(CONF_COMMAND, default=0x0200): cv.templatable(
                cv.one_of(0x0200, 0x0400, int=True)
            ),
        }
    ),
    validate_transaction_action,
)

automation.register_apply_action(
    "vbus.read_value",
    READ_VALUE_SCHEMA,
    automation.ApplyCall(
        "read_value({}, {})", ((CONF_GRANT_ID, cg.uint32), (CONF_VALUE_ID, cg.uint16))
    ),
)
automation.register_apply_action(
    "vbus.write_value",
    WRITE_VALUE_SCHEMA,
    automation.ApplyCall(
        "write_value({}, {}, {}, {})",
        (
            (CONF_GRANT_ID, cg.uint32),
            (CONF_VALUE_ID, cg.uint16),
            (CONF_VALUE, cg.uint32),
            (CONF_COMMAND, cg.uint16),
        ),
    ),
)

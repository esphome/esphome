from collections.abc import Callable, Iterable, Iterator

from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.const import CONF_ENABLED
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_ID,
    CONF_MODEL,
    CONF_NAME,
    CONF_ON_VALUE,
    CONF_SOURCE,
    CONF_UART_ID,
    CONF_UPDATE_INTERVAL,
    CONF_VALUE,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

CODEOWNERS = ["@ssieb"]
DEPENDENCIES = ["uart"]
DOMAIN = "vbus"
MULTI_CONF = True
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "vbus_tx.cpp": "USE_VBUS_TX",
        "vbus_transactions.cpp": "USE_VBUS_TRANSACTIONS",
        "vbus_polling.cpp": "USE_VBUS_POLLING",
        "resol_deltasol_cs_plus.cpp": "USE_VBUS_CS_PLUS",
        "vbus_cs_plus_policy.cpp": "USE_VBUS_CS_PLUS",
        "resol_deltasol_cs_plus_adaptive.cpp": "USE_VBUS_CS_PLUS_ADAPTIVE",
        "resol_deltasol_cs_plus_write.cpp": "USE_VBUS_CS_PLUS_WRITE",
        "vbus_setting_entity.cpp": "USE_VBUS_CS_PLUS_ENTITIES",
    }
)

vbus_ns = cg.esphome_ns.namespace("vbus")
VBus = vbus_ns.class_("VBus", uart.UARTDevice, cg.Component)
VBusTransmitter = vbus_ns.class_("VBusTransmitter", uart.UARTDevice, cg.Component)
VBusTransactions = vbus_ns.class_("VBusTransactions", cg.Component)
VBusPolling = vbus_ns.class_("VBusPolling", cg.PollingComponent)
ResolDeltaSolCSPlus = vbus_ns.class_("ResolDeltaSolCSPlus", cg.PollingComponent)
_request_poll_value_slot = cg.slot_counter("VBUS_POLL_VALUE_COUNT")
_request_setting_entity_slot = cg.slot_counter("VBUS_SETTING_ENTITY_COUNT")
TransactionStatus = vbus_ns.enum("TransactionStatus", is_class=True)
SettingWriteStatus = vbus_ns.enum("SettingWriteStatus", is_class=True)

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
CONF_POLLING = "polling"
CONF_VALUE_IDS = "value_ids"
CONF_RX_VALUE_IDS = "rx_value_ids"
CONF_BATCH_SIZE = "batch_size"
CONF_CONTROLLER = "controller"
CONF_SCHEDULER_ID = "scheduler_id"
CONF_ON_SNAPSHOT = "on_snapshot"
CONF_WRITES = "writes"
CONF_REQUIRE_WRITE_ARM = "require_write_arm"
CONF_ON_SETTING_RESULT = "on_setting_result"
CONF_CONTROLLER_ID = "controller_id"
CONF_PARAMETER = "parameter"
CONF_RX_DIAGNOSTICS = "rx_diagnostics"
CONF_ADAPTIVE_POLLING = "adaptive_polling"
CONF_CONTEXT_UPDATE_INTERVAL = "context_update_interval"
CS_PLUS_SETTING_COUNT = 84


def validate_address(value: object) -> int:
    address = cv.uint16_t(value)
    if address == 0 or address & 0x8080:
        raise cv.Invalid(
            "A VBus address must be nonzero and have both byte high bits clear"
        )
    return address


def validate_transmit(config: ConfigType) -> ConfigType:
    transactions = config.get(CONF_TRANSACTIONS, {})
    if CONF_DESTINATION not in config:
        if CONF_CONTROLLER not in transactions:
            raise cv.Invalid(
                "Set destination for generic TX, or configure a supported controller model",
                path=[CONF_DESTINATION],
            )
        config[CONF_DESTINATION] = 0x2211
    if config[CONF_SOURCE] == config[CONF_DESTINATION]:
        raise cv.Invalid("VBus source and controller destination must differ")
    if CONF_POLLING in transactions or CONF_CONTROLLER in transactions:
        if CONF_ON_BUS_GRANT in config:
            raise cv.Invalid("Polling owns bus grants; remove on_bus_grant")
        if config[CONF_LEASE_DURATION].total_milliseconds <= (
            140 + 3 * transactions[CONF_RESPONSE_TIMEOUT].total_milliseconds
        ):
            raise cv.Invalid(
                "Polling lease_duration must exceed 140ms + 3 * response_timeout"
            )
    if CONF_CONTROLLER in transactions and (
        config[CONF_DESTINATION] != 0x2211 or config[CONF_SOURCE] != 0x0020
    ):
        raise cv.Invalid("CS Plus requires destination 0x2211 and source 0x0020")
    return config


def validate_command(value: object) -> int:
    command = cv.uint16_t(value)
    if command & 0x8080 or command in (0x0500, 0x0600):
        raise cv.Invalid(
            "Command must have both byte high bits clear; bus grants and releases are managed internally"
        )
    return command


def validate_polling(config: ConfigType) -> ConfigType:
    if config[CONF_UPDATE_INTERVAL].total_milliseconds < 500:
        raise cv.Invalid("Polling update_interval must be at least 500ms or never")
    ids = config[CONF_VALUE_IDS]
    rx_ids = config[CONF_RX_VALUE_IDS]
    if len(set(ids)) != len(ids) or len(set(rx_ids)) != len(rx_ids):
        raise cv.Invalid("Polling value IDs must be unique within each list")
    if not set(rx_ids) <= set(ids):
        raise cv.Invalid("rx_value_ids must be included in value_ids")
    return config


POLLING_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VBusPolling),
            cv.Required(CONF_VALUE_IDS): cv.All(
                cv.ensure_list(cv.uint16_t), cv.Length(min=1)
            ),
            cv.Optional(CONF_RX_VALUE_IDS, default=[]): cv.ensure_list(cv.uint16_t),
            cv.Optional(CONF_BATCH_SIZE, default=16): cv.int_range(min=1, max=16),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation({}),
            cv.Optional(CONF_ON_RESULT): automation.validate_automation({}),
        }
    ).extend(cv.polling_component_schema("60s")),
    validate_polling,
)


def validate_controller(config: ConfigType) -> ConfigType:
    if isinstance(writes := config.get(CONF_WRITES), str):
        config[CONF_WRITES] = {CONF_REQUIRE_WRITE_ARM: writes == "armed"}
    if config[CONF_UPDATE_INTERVAL].total_milliseconds < 60000:
        raise cv.Invalid("Controller update_interval must be at least 60s or never")
    if adaptive := config.get(CONF_ADAPTIVE_POLLING):
        full = config[CONF_UPDATE_INTERVAL].total_milliseconds
        context = adaptive[CONF_CONTEXT_UPDATE_INTERVAL].total_milliseconds
        if full > 86400000:
            raise cv.Invalid(
                "Adaptive polling requires a finite update_interval of at most 24h"
            )
        if context < 60000 or context > full:
            raise cv.Invalid(
                "context_update_interval must be at least 60s and no longer than update_interval"
            )
    return config


CONTROLLER_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ResolDeltaSolCSPlus),
            cv.GenerateID(CONF_SCHEDULER_ID): cv.declare_id(VBusPolling),
            cv.Required(CONF_MODEL): cv.one_of("deltasol_cs_plus", lower=True),
            cv.Optional(CONF_ON_SNAPSHOT): automation.validate_automation({}),
            cv.Optional(CONF_ADAPTIVE_POLLING): cv.Schema(
                {
                    cv.Optional(
                        CONF_CONTEXT_UPDATE_INTERVAL, default="5min"
                    ): cv.positive_time_period_milliseconds,
                }
            ),
            cv.Optional(CONF_WRITES): cv.Any(
                cv.one_of("direct", "armed", lower=True),
                cv.Schema(
                    {
                        cv.Optional(CONF_REQUIRE_WRITE_ARM, default=True): cv.boolean,
                        cv.Optional(
                            CONF_ON_SETTING_RESULT
                        ): automation.validate_automation({}),
                    }
                ),
                msg="writes must be direct, armed, or a mapping with require_write_arm and callbacks",
            ),
        }
    ).extend(cv.polling_component_schema("5min")),
    validate_controller,
)


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
        cv.Exclusive(CONF_POLLING, "scheduler"): POLLING_SCHEMA,
        cv.Exclusive(CONF_CONTROLLER, "scheduler"): CONTROLLER_SCHEMA,
    }
)

TRANSMIT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VBusTransmitter),
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_SOURCE, default=0x0020): cv.All(
                cv.uint16_t, validate_address
            ),
            cv.Optional(CONF_DESTINATION): cv.All(cv.uint16_t, validate_address),
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


def normalize_controller(config: ConfigType) -> ConfigType:
    if not isinstance(config, dict) or CONF_CONTROLLER not in config:
        return config
    transmit = config.get(CONF_TRANSMIT, {})
    if not isinstance(transmit, dict):
        return config
    transactions = transmit.get(CONF_TRANSACTIONS, {})
    if not isinstance(transactions, dict):
        return config
    if CONF_CONTROLLER in transactions:
        raise cv.Invalid(
            "Configure controller either under vbus or under transmit.transactions, not both",
            path=[CONF_CONTROLLER],
        )
    config = config.copy()
    config[CONF_TRANSMIT] = {
        **transmit,
        CONF_TRANSACTIONS: {
            **transactions,
            CONF_CONTROLLER: config.pop(CONF_CONTROLLER),
        },
    }
    return config


CONFIG_SCHEMA = cv.All(
    normalize_controller,
    uart.UART_DEVICE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(VBus),
            cv.Optional(CONF_RX_DIAGNOSTICS, default=False): cv.boolean,
            cv.Optional(CONF_ON_DATAGRAM): automation.validate_automation({}),
            cv.Optional(CONF_TRANSMIT): TRANSMIT_SCHEMA,
            cv.Optional(CONF_CONTROLLER): CONTROLLER_SCHEMA,
        }
    ),
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
    polling = transmit is not None and CONF_POLLING in transmit.get(
        CONF_TRANSACTIONS, {}
    )
    controller = transmit is not None and CONF_CONTROLLER in transmit.get(
        CONF_TRANSACTIONS, {}
    )
    data = CORE.data.get(DOMAIN, {})
    if controller:
        profile = transmit[CONF_TRANSACTIONS][CONF_CONTROLLER]
        if (
            str(profile[CONF_ID]) in data.get("setting_actions", set())
            and CONF_WRITES not in profile
        ):
            raise cv.Invalid(
                "Setting actions require controller.writes on this controller"
            )
    if controller and str(config[CONF_ID]) in data.get("queue_actions", set()):
        raise cv.Invalid("Controller owns TX; raw queue actions are not allowed")
    if str(config[CONF_ID]) in data.get("queue_actions", set()) and not polling:
        raise cv.Invalid(
            "VBus queue actions require transmit.transactions.polling on this hub"
        )
    if str(config[CONF_ID]) in data.get("polling_actions", set()) and not (
        polling or controller
    ):
        raise cv.Invalid(
            "VBus polling actions require transmit.transactions.polling or controller on this hub"
        )
    if (polling or controller) and (
        str(config[CONF_ID]) in data.get("tx_actions", set())
        or str(config[CONF_ID]) in data.get("transaction_actions", set())
    ):
        if controller:
            raise cv.Invalid(
                "Controller owns TX; use setting controls, vbus.set_setting or vbus.scan_controller instead of manual bus actions"
            )
        raise cv.Invalid(
            "Polling owns TX; use queue_read and queue_write instead of manual bus actions"
        )
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
    if config[CONF_RX_DIAGNOSTICS]:
        cg.add_define("USE_VBUS_RX_DIAGNOSTICS")
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
            if polling := transactions.get(CONF_POLLING):
                cg.add_define("USE_VBUS_POLLING")
                poller = cg.new_Pvariable(polling[CONF_ID], manager, tx)
                await cg.register_component(poller, polling)
                cg.add(var.set_polling(poller))
                cg.add(poller.set_batch_size(polling[CONF_BATCH_SIZE]))
                for value_id in polling[CONF_VALUE_IDS]:
                    _request_poll_value_slot(str(var))
                    cg.add(
                        poller.register_value(
                            value_id, value_id in polling[CONF_RX_VALUE_IDS]
                        )
                    )
                for conf in polling.get(CONF_ON_VALUE, []):
                    await automation.build_callback_automation(
                        poller,
                        "add_on_value_callback",
                        [(cg.uint16, "value_id"), (cg.uint32, "value")],
                        conf,
                    )
                for conf in polling.get(CONF_ON_RESULT, []):
                    await automation.build_callback_automation(
                        poller,
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
            if controller_config := transactions.get(CONF_CONTROLLER):
                cg.add_define("USE_VBUS_CS_PLUS")
                cg.add_define("USE_VBUS_POLLING")
                poller = cg.new_Pvariable(
                    controller_config[CONF_SCHEDULER_ID], manager, tx, False
                )
                await cg.register_component(
                    poller,
                    {
                        CONF_ID: controller_config[CONF_SCHEDULER_ID],
                        CONF_UPDATE_INTERVAL: cv.update_interval("never"),
                    },
                )
                cg.add(var.set_polling(poller))
                for _ in range(CS_PLUS_SETTING_COUNT):
                    _request_poll_value_slot(str(var))
                controller = cg.new_Pvariable(
                    controller_config[CONF_ID], var, poller, tx
                )
                await cg.register_component(controller, controller_config)
                if adaptive := controller_config.get(CONF_ADAPTIVE_POLLING):
                    cg.add_define("USE_VBUS_CS_PLUS_ADAPTIVE")
                    cg.add(
                        controller.set_context_update_interval(
                            adaptive[CONF_CONTEXT_UPDATE_INTERVAL]
                        )
                    )
                if writes := controller_config.get(CONF_WRITES):
                    cg.add_define("USE_VBUS_CS_PLUS_WRITE")
                    cg.add(
                        controller.set_require_write_arm(writes[CONF_REQUIRE_WRITE_ARM])
                    )
                    for conf in writes.get(CONF_ON_SETTING_RESULT, []):
                        await automation.build_callback_automation(
                            controller,
                            "add_on_setting_result_callback",
                            [(cg.uint16, "value_id"), (SettingWriteStatus, "status")],
                            conf,
                        )
                for conf in controller_config.get(CONF_ON_SNAPSHOT, []):
                    await automation.build_callback_automation(
                        controller, "add_on_snapshot_callback", [], conf
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


def validate_queue_action(config: ConfigType) -> ConfigType:
    CORE.data.setdefault(DOMAIN, {}).setdefault("queue_actions", set()).add(
        str(config[CONF_ID])
    )
    return config


def validate_polling_action(config: ConfigType) -> ConfigType:
    CORE.data.setdefault(DOMAIN, {}).setdefault("polling_actions", set()).add(
        str(config[CONF_ID])
    )
    return config


QUEUE_READ_SCHEMA = cv.All(
    TX_ID_SCHEMA.extend({cv.Required(CONF_VALUE_ID): cv.templatable(cv.uint16_t)}),
    validate_queue_action,
)
QUEUE_WRITE_SCHEMA = cv.All(
    TX_ID_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE_ID): cv.templatable(cv.uint16_t),
            cv.Required(CONF_VALUE): cv.templatable(cv.uint32_t),
            cv.Optional(CONF_COMMAND, default=0x0200): cv.templatable(
                cv.one_of(0x0200, 0x0400, int=True)
            ),
        }
    ),
    validate_queue_action,
)

automation.register_apply_action(
    "vbus.queue_read",
    QUEUE_READ_SCHEMA,
    automation.ApplyCall("queue_read({})", ((CONF_VALUE_ID, cg.uint16),)),
)
automation.register_apply_action(
    "vbus.queue_write",
    QUEUE_WRITE_SCHEMA,
    automation.ApplyCall(
        "queue_write({}, {}, {})",
        (
            (CONF_VALUE_ID, cg.uint16),
            (CONF_VALUE, cg.uint32),
            (CONF_COMMAND, cg.uint16),
        ),
    ),
)


automation.register_apply_action(
    "vbus.suspend_polling",
    cv.All(TX_ID_SCHEMA, validate_polling_action),
    automation.ApplyCall("suspend_polling()"),
)
automation.register_apply_action(
    "vbus.resume_polling",
    cv.All(TX_ID_SCHEMA, validate_polling_action),
    automation.ApplyCall("resume_polling()"),
)

automation.register_apply_action(
    "vbus.scan_controller",
    cv.Schema({cv.Required(CONF_ID): cv.use_id(ResolDeltaSolCSPlus)}),
    automation.ApplyCall("start_scan()"),
)


def require_controller_writes(controller: ID) -> None:
    CORE.data.setdefault(DOMAIN, {}).setdefault("setting_actions", set()).add(
        str(controller)
    )


def validate_setting_controller(config: ConfigType) -> ConfigType:
    controller = fv.get_declaring_config(config[CONF_CONTROLLER_ID])
    if CONF_WRITES not in controller:
        raise cv.Invalid(
            "Setting controls require controller.writes; add writes: armed "
            "or writes: direct",
            path=[CONF_CONTROLLER_ID],
        )
    return config


def setting_entity_defaults(config: ConfigType) -> ConfigType:
    if (
        isinstance(config, dict)
        and CONF_NAME not in config
        and CONF_ID not in config
        and isinstance(parameter := config.get(CONF_PARAMETER), str)
    ):
        config = config.copy()
        parameter = parameter.lower()
        name = parameter.replace("min_", "minimum_").replace("max_", "maximum_")
        config[CONF_NAME] = name.replace("_", " ").capitalize()
        if (
            parameter == "system_time"
            or "_start_time" in parameter
            or "_end_time" in parameter
        ):
            config[CONF_NAME] += " (minutes after midnight)"
    return config


def register_setting_entity(controller: cg.MockObj, entity: cg.MockObj) -> None:
    cg.add_define("USE_VBUS_CS_PLUS_ENTITIES")
    _request_setting_entity_slot(str(controller))
    cg.add(controller.register_setting_listener(entity))


def setting_group_defaults(
    config: ConfigType,
    parameters: Iterable[str],
    defaults: Callable[[ConfigType], ConfigType],
) -> ConfigType:
    if not isinstance(config, dict) or CONF_PARAMETER in config:
        return config
    config = config.copy()
    for parameter in parameters:
        if isinstance(entity := config.get(parameter), dict):
            entity = defaults({**entity, CONF_PARAMETER: parameter})
            del entity[CONF_PARAMETER]
            config[parameter] = entity
    return config


def setting_configs(
    config: ConfigType, parameters: Iterable[str]
) -> Iterator[ConfigType]:
    if CONF_PARAMETER in config:
        yield config
    else:
        for parameter in parameters:
            if parameter in config:
                yield {**config[parameter], CONF_PARAMETER: parameter}


def setting_platform_schema(
    parameters: Iterable[str],
    entity_schema: cv.Schema,
    single_schema: cv.All,
    group_schema: cv.All,
) -> Callable[[ConfigType], ConfigType]:
    @schema_extractor("schema")
    def validate(config: ConfigType) -> ConfigType:
        if config is SCHEMA_EXTRACT:
            # The editor schema cannot express this union; expose both sets of keys.
            return entity_schema.extend(
                {
                    cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
                    cv.Optional(CONF_PARAMETER): cv.one_of(*parameters, lower=True),
                    **{cv.Optional(key): entity_schema for key in parameters},
                }
            )
        if isinstance(config, dict) and CONF_PARAMETER in config:
            return single_schema(config)
        return group_schema(config)

    return validate


def validate_setting_action(config: ConfigType) -> ConfigType:
    require_controller_writes(config[CONF_ID])
    return config


automation.register_apply_action(
    "vbus.set_setting",
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ResolDeltaSolCSPlus),
                cv.Required(CONF_VALUE_ID): cv.templatable(cv.uint16_t),
                cv.Required(CONF_VALUE): cv.templatable(cv.float_),
            }
        ),
        validate_setting_action,
    ),
    automation.ApplyCall(
        "request_setting({}, {})", ((CONF_VALUE_ID, cg.uint16), (CONF_VALUE, cg.float_))
    ),
)
automation.register_apply_action(
    "vbus.arm_writes",
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ResolDeltaSolCSPlus),
                cv.Required(CONF_ENABLED): cv.templatable(cv.boolean),
            }
        ),
        validate_setting_action,
    ),
    automation.ApplyCall("arm_writes({})", ((CONF_ENABLED, cg.bool_),)),
)

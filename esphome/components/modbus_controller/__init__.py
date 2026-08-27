import binascii
from dataclasses import dataclass
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.modbus.helpers import (
    MODBUS_REGISTER_TYPE,
    TYPE_REGISTER_MAP,
    EntityType,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CONTINUOUS,
    CONF_ID,
    CONF_LAMBDA,
    CONF_NAME,
    CONF_OFFSET,
)
from esphome.core import CORE
from esphome.cpp_helpers import logging
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import (
    CONF_ALLOW_DUPLICATE_COMMANDS,
    CONF_BITMASK,
    CONF_BYTE_OFFSET,
    CONF_COMMAND_THROTTLE,
    CONF_CUSTOM_COMMAND,
    CONF_CUSTOM_PDU,
    CONF_FORCE_NEW_RANGE,
    CONF_MAX_CMD_RETRIES,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_OFFLINE_SKIP_UPDATES,
    CONF_ON_COMMAND_SENT,
    CONF_ON_OFFLINE,
    CONF_ON_ONLINE,
    CONF_REGISTER_COUNT,
    CONF_REGISTER_TYPE,
    CONF_RESPONSE_SIZE,
    CONF_SERVER_COURTESY_RESPONSE,
    CONF_SERVER_REGISTERS,
    CONF_SKIP_UPDATES,
    CONF_VALUE_TYPE,
)

CODEOWNERS = ["@martgras"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

DOMAIN = "modbus_controller"

modbus_controller_ns = cg.esphome_ns.namespace("modbus_controller")
ModbusController = modbus_controller_ns.class_("ModbusController", cg.PollingComponent)

SensorItem = modbus_controller_ns.struct("SensorItem")

_LOGGER = logging.getLogger(__name__)


@dataclass
class ModbusControllerData:
    # Set once the deprecated 'skip_updates' warning has been emitted so we warn only once total.
    skip_updates_warned: bool = False


def _get_data() -> ModbusControllerData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ModbusControllerData()
    return CORE.data[DOMAIN]


# Remove before 2027.2.0
_REMOVED_OPTIONS = {
    CONF_COMMAND_THROTTLE: "Command spacing is handled by the 'modbus' component - use 'turnaround_time' there instead.",
    CONF_ALLOW_DUPLICATE_COMMANDS: "Polling commands are deduplicated by the modbus hub; one-shot commands (writes) are always transmitted.",
}


def _warn_removed_options(config: ConfigType) -> ConfigType:
    """Warn about options that no longer do anything, but let the config compile."""
    for option, replacement in _REMOVED_OPTIONS.items():
        if option in config:
            _LOGGER.warning(
                "[modbus_controller] '%s' no longer has any effect and will be removed in 2027.2.0. %s",
                option,
                replacement,
            )
    return config


def _reject_broadcast_address(config: ConfigType) -> ConfigType:
    """A modbus_controller polls one device, so its address cannot be the broadcast address (0):
    a broadcast is never answered (Modbus 4.1), so no register could ever read back."""
    modbus.reject_broadcast_address(
        config.get(CONF_ADDRESS),
        "a modbus_controller device address",
        "Assign the unit address of the device you want to poll.",
        [CONF_ADDRESS],
    )
    return config


# Remove before 2027.3.0. skip_updates (a per-sensor option) no longer does anything: every range is
# polled each update_interval. The key is still accepted so existing configs keep working, with a warning.
def validate_skip_updates_deprecated(value: Any) -> int:
    data = _get_data()
    if not data.skip_updates_warned:
        _LOGGER.warning(
            "[modbus_controller] 'skip_updates' no longer has any effect and will be removed in 2027.3.0. "
            "To poll some registers less often, add a second modbus_controller with the same address and a "
            "slower update_interval, and attach the slow sensors to it."
        )
        data.skip_updates_warned = True
    return cv.positive_int(value)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusController),
            # Removed options: accepted (and ignored) until 2027.2.0 so existing configs keep building.
            cv.Optional(CONF_ALLOW_DUPLICATE_COMMANDS): cv.boolean,
            cv.Optional(CONF_COMMAND_THROTTLE): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SERVER_COURTESY_RESPONSE): cv.invalid(
                "This option has been removed. Use modbus_server component instead: https://esphome.io/components/modbus_server/"
            ),
            cv.Optional(CONF_MAX_CMD_RETRIES, default=4): cv.positive_int,
            cv.Optional(CONF_OFFLINE_SKIP_UPDATES, default=0): cv.positive_int,
            **modbus.command_options_schema(direction="read"),
            cv.Optional(
                CONF_SERVER_REGISTERS,
            ): cv.invalid(
                "This option has been removed. Use modbus_server component instead: https://esphome.io/components/modbus_server/"
            ),
            cv.Optional(CONF_ON_COMMAND_SENT): automation.validate_automation({}),
            cv.Optional(CONF_ON_ONLINE): automation.validate_automation({}),
            cv.Optional(CONF_ON_OFFLINE): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01)),
    _warn_removed_options,
    _reject_broadcast_address,
)

ModbusItemBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        cv.Optional(CONF_ADDRESS): cv.positive_int,
        cv.Exclusive(
            CONF_CUSTOM_PDU,
            "custom_source",
            f"{CONF_CUSTOM_PDU} and {CONF_CUSTOM_COMMAND} can't be used together",
        ): cv.All(
            cv.ensure_list(cv.hex_uint8_t),
            cv.Length(min=1, max=modbus.MAX_PDU_SIZE),
        ),
        # Deprecated: takes a raw frame with a leading device address byte. Auto-migrated to
        # custom_pdu in migrate_custom_command (final validate). Remove before 2027.3.0. The upper
        # bound is MAX_PDU_SIZE + 1: the extra byte is the address the migration strips.
        cv.Exclusive(
            CONF_CUSTOM_COMMAND,
            "custom_source",
            f"{CONF_CUSTOM_PDU} and {CONF_CUSTOM_COMMAND} can't be used together",
        ): cv.All(
            cv.ensure_list(cv.hex_uint8_t),
            cv.Length(min=2, max=modbus.MAX_PDU_SIZE + 1),
        ),
        cv.Exclusive(
            CONF_OFFSET,
            "offset",
            f"{CONF_OFFSET} and {CONF_BYTE_OFFSET} can't be used together",
        ): cv.positive_int,
        cv.Exclusive(
            CONF_BYTE_OFFSET,
            "offset",
            f"{CONF_OFFSET} and {CONF_BYTE_OFFSET} can't be used together",
        ): cv.positive_int,
        cv.Optional(CONF_BITMASK, default=0xFFFFFFFF): cv.hex_uint32_t,
        cv.Optional(CONF_SKIP_UPDATES): validate_skip_updates_deprecated,
        cv.Optional(CONF_FORCE_NEW_RANGE, default=False): cv.boolean,
        cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_RESPONSE_SIZE, default=0): cv.positive_int,
    },
)


def validate_modbus_register(config: ConfigType) -> ConfigType:
    # custom_command is the deprecated alias for custom_pdu (migrated later in final validate); treat
    # either as "a custom frame is configured" so the address/register_type rules match.
    has_custom = CONF_CUSTOM_PDU in config or CONF_CUSTOM_COMMAND in config
    if not has_custom and CONF_ADDRESS not in config:
        raise cv.Invalid(
            f" {CONF_ADDRESS} is a required property if '{CONF_CUSTOM_PDU}:' isn't used"
        )
    if has_custom and CONF_REGISTER_TYPE in config:
        raise cv.Invalid(
            f"can't use '{CONF_REGISTER_TYPE}:' together with '{CONF_CUSTOM_PDU}:'",
        )

    if not has_custom and CONF_REGISTER_TYPE not in config:
        raise cv.Invalid(
            f" {CONF_REGISTER_TYPE} is a required property if '{CONF_CUSTOM_PDU}:' isn't used"
        )
    return config


def migrate_custom_command(config: ConfigType) -> None:
    """Final-validate: auto-migrate the deprecated custom_command (raw frame incl. device address)
    to custom_pdu (PDU only). custom_pdu is always sent to the controller's own address, so a frame
    whose address byte does not match the controller's address is a hard error (it targeted a
    different unit). Mutates config in place; final validate discards the return value."""
    frame = config.get(CONF_CUSTOM_COMMAND)
    if frame is None:
        return
    fconf = fv.full_config.get()
    path = fconf.get_path_for_id(config[CONF_MODBUS_CONTROLLER_ID])[:-1]
    controller = fconf.get_config_for_path(path)
    # the controller's DEVICE address (from modbus_device_schema)
    address = controller[CONF_ADDRESS]
    if frame[0] != address:
        raise cv.Invalid(
            f"'custom_command' begins with device address {frame[0]:#04x}, but this sensor's "
            f"modbus_controller uses address {address:#04x}. 'custom_command' is renamed to "
            f"'custom_pdu', which is always sent to the controller's own address. Drop the leading "
            f"address byte and use 'custom_pdu' if {address:#04x} is correct, or move this sensor to "
            f"the modbus_controller for device {frame[0]:#04x}.",
            [CONF_CUSTOM_COMMAND],
        )
    _LOGGER.warning(
        "[modbus_controller] 'custom_command' is deprecated and will be removed in 2027.3.0; "
        "auto-migrated to 'custom_pdu' (dropped the leading device address byte). Rename the key "
        "and drop that byte to silence this warning."
    )
    config[CONF_CUSTOM_PDU] = list(frame[1:])
    del config[CONF_CUSTOM_COMMAND]


def _reject_continuous_write_custom_pdu(config: ConfigType) -> None:
    """Final-validate: a custom_pdu whose function code writes (e.g. 0x17 read/write-multiple) cannot be
    polled continuously - the hub ignores continuous for mutating codes and would warn on every update
    while that range silently does not stream. Reject the combination instead. Runs after
    migrate_custom_command, so it sees custom_pdu whether written directly or migrated from
    custom_command."""
    pdu = config.get(CONF_CUSTOM_PDU)
    if pdu is None or not modbus.is_function_code_write(pdu[0]):
        return
    fconf = fv.full_config.get()
    path = fconf.get_path_for_id(config[CONF_MODBUS_CONTROLLER_ID])[:-1]
    controller = fconf.get_config_for_path(path)
    if controller.get(CONF_CONTINUOUS) is True:
        raise cv.Invalid(
            f"a '{CONF_CUSTOM_PDU}' with a write function code (0x{pdu[0] & 0x7F:02X}) can't be polled "
            f"continuously: the hub ignores 'continuous' for mutating codes. Remove 'continuous: true' "
            f"from the '{controller[CONF_ID]}' modbus_controller, or use a read function code.",
            [CONF_CUSTOM_PDU],
        )


def validate_custom_pdu_item(config: ConfigType) -> None:
    """Final-validate for the read platforms that accept custom_pdu (sensor, binary_sensor,
    text_sensor): migrate the deprecated custom_command, then reject a write-coded custom_pdu under a
    continuously-polling controller."""
    migrate_custom_command(config)
    _reject_continuous_write_custom_pdu(config)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("modbus_controller", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


def reject_odd_holding_write_offset(config: ConfigType) -> ConfigType:
    """Reject an odd byte offset on a holding-register write entity.

    A 16-bit register write cannot target half a register, so the residual byte is inexpressible.
    """
    key = CONF_BYTE_OFFSET if CONF_BYTE_OFFSET in config else CONF_OFFSET
    if config.get(key, 0) % 2:
        raise cv.Invalid(
            f"An odd '{key}' cannot be used with holding-register writes: a 16-bit register "
            "write cannot target half a register. Use an even offset, or fold it into 'address'",
            path=[key],
        )
    return config


def modbus_calc_properties(config: ConfigType) -> tuple[int, int]:
    byte_offset = 0
    reg_count = 0
    if CONF_OFFSET in config:
        byte_offset = config[CONF_OFFSET]
    # A CONF_BYTE_OFFSET setting overrides CONF_OFFSET
    if CONF_BYTE_OFFSET in config:
        byte_offset = config[CONF_BYTE_OFFSET]
    if CONF_REGISTER_COUNT in config:
        reg_count = config[CONF_REGISTER_COUNT]
    if CONF_VALUE_TYPE in config:
        value_type = config[CONF_VALUE_TYPE]
        if reg_count == 0:
            reg_count = TYPE_REGISTER_MAP[value_type]
    if CONF_CUSTOM_PDU in config:
        if CONF_ADDRESS not in config:
            # generate a unique modbus address using the hash of the name
            # CONF_NAME set even if only CONF_ID is used.
            # a modbus register address is required to add the item to sensormap
            value = config[CONF_NAME]
            if isinstance(value, str):
                value = value.encode()
            config[CONF_ADDRESS] = binascii.crc_hqx(value, 0)
        config[CONF_REGISTER_TYPE] = cv.enum(MODBUS_REGISTER_TYPE)("custom")
        config[CONF_FORCE_NEW_RANGE] = True
    return byte_offset, reg_count


async def add_modbus_base_properties(
    var: cg.MockObj,
    config: ConfigType,
    sensor_type: cg.MockObjClass,
    lambda_param_type: cg.MockObj = cg.float_,
    lambda_return_type: Any = float,
) -> None:
    if CONF_CUSTOM_PDU in config:
        cg.add(var.set_custom_pdu(config[CONF_CUSTOM_PDU]))

    if config[CONF_RESPONSE_SIZE] > 0:
        cg.add(var.set_register_size(config[CONF_RESPONSE_SIZE]))

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [
                (sensor_type.operator("ptr"), "item"),
                (lambda_param_type, "x"),
                (cg.std_span.template(cg.uint8.operator("const")), "data"),
            ],
            return_type=cg.optional.template(lambda_return_type),
        )
        cg.add(var.set_template(template_))


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_COMMAND_SENT,
        "add_on_command_sent_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ONLINE,
        "add_on_online_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
    automation.CallbackAutomation(
        CONF_ON_OFFLINE,
        "add_on_offline_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
)


async def to_code(config: ConfigType) -> None:
    # Await the hub first, so no entity can bind to a controller that doesn't have one yet.
    hub = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub, config[CONF_ADDRESS])
    await cg.register_component(var, config)
    cg.add(var.set_max_cmd_retries(config[CONF_MAX_CMD_RETRIES]))
    cg.add(var.set_offline_skip_updates(config[CONF_OFFLINE_SKIP_UPDATES]))
    cg.add(
        var.set_read_options(
            modbus.command_options_expression(config, direction="read")
        )
    )
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


async def register_modbus_device(var: cg.MockObj, config: ConfigType) -> cg.MockObj:
    # Remove before 2027.3.0
    _LOGGER.warning(
        "'modbus_controller.register_modbus_device' is deprecated, use "
        "'modbus.register_modbus_client_device' and set the address on your own "
        "class instead. Will be removed in 2027.3.0"
    )
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_client_device(var, config)


def function_code_to_register(function_code: str) -> cg.MockObj:
    FUNCTION_CODE_TYPE_MAP = {
        "read_coils": EntityType.COIL,
        "read_discrete_inputs": EntityType.DISCRETE_INPUT,
        "read_holding_registers": EntityType.HOLDING,
        "read_input_registers": EntityType.INPUT_REGISTER,
        "write_single_coil": EntityType.COIL,
        "write_single_register": EntityType.HOLDING,
        "write_multiple_coils": EntityType.COIL,
        "write_multiple_registers": EntityType.HOLDING,
    }
    return FUNCTION_CODE_TYPE_MAP[function_code]

from collections.abc import Callable
import functools
from typing import Any

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import ble_device_base, bluetooth_connection
from esphome.components.ble_device_base import (
    BT_UUID16_FORMAT as bt_uuid16_format,
    BT_UUID32_FORMAT as bt_uuid32_format,
    BT_UUID128_FORMAT as bt_uuid128_format,
    as_hex,
    as_reversed_hex_array,
    bt_uuid,
)
from esphome.config_helpers import (
    filter_source_files_from_platform,
    frameworks_for_platforms,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHARACTERISTIC_UUID,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NAME,
    CONF_NOTIFY,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
    CONF_VALUE,
    PLATFORM_ESP32,
    PlatformFramework,
)
from esphome.core import CORE, ID
from esphome.enum import StrEnum
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

# The esp32 BLE stack (esp32_ble, esp32_ble_tracker) is imported lazily inside
# the esp32 schema/codegen arms: importing those modules registers esp32-only
# automations as a side effect, which must not leak into the neutral
# platforms' registries (the bluetooth_proxy pattern).


def _legacy_engine() -> bool:
    """True when the build uses the legacy raw-gattc engine - one line to
    flip when esp32 moves to the neutral engine (with
    USE_BLE_CLIENT_LEGACY_ENGINE in _to_code_esp32)."""
    return CORE.is_esp32


def AUTO_LOAD() -> list[str]:
    """The engine's closure per platform: the legacy esp32 engine builds on
    esp32_ble_client plus bluetooth_connection (the shared service-table
    materializer; its sources compile empty in builds without a neutral
    node), the neutral engine on the bluetooth_connection backend. The
    platform-less arm is the union for manifest-resolving tooling."""
    if _legacy_engine() or CORE.target_platform is None:
        return ["bluetooth_connection", "esp32_ble_client"]
    return ["bluetooth_connection"]


CODEOWNERS = ["@buxtronix", "@clydebarrow"]

FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "ble_client.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        # Every framework of every non-esp32 registry platform: a platform
        # that validates the neutral arm must also compile the neutral engine.
        "ble_client_gatt.cpp": frameworks_for_platforms(
            set(bluetooth_connection.GATT_CLIENT_PLATFORMS) - {PLATFORM_ESP32}
        ),
    }
)

CONF_DESCRIPTOR_UUID = "descriptor_uuid"
CONF_ON_NOTIFY = "on_notify"


def validate_descriptor_not_notify(config: ConfigType) -> ConfigType:
    """Reject descriptor_uuid combined with notify or on_notify.

    BLE descriptors cannot send notifications; only characteristics can, and
    ESP-IDF has no descriptor variant of esp_ble_gattc_register_for_notify.
    """
    if CONF_DESCRIPTOR_UUID in config and (
        config.get(CONF_NOTIFY) or CONF_ON_NOTIFY in config
    ):
        raise cv.Invalid(
            f"'{CONF_DESCRIPTOR_UUID}' cannot be used with '{CONF_NOTIFY}' or "
            f"'{CONF_ON_NOTIFY}': BLE descriptors cannot send notifications; remove "
            f"'{CONF_DESCRIPTOR_UUID}' to receive characteristic notifications, or "
            f"remove '{CONF_NOTIFY}' and '{CONF_ON_NOTIFY}' to poll the descriptor"
        )
    return config


def notify_from_on_notify(config: ConfigType) -> ConfigType:
    """Enable notifications when an on_notify automation is configured.

    The triggers have no registration path of their own; without notify the
    automation would validate but never fire.
    """
    if CONF_ON_NOTIFY in config and not config[CONF_NOTIFY]:
        config = config.copy()
        config[CONF_NOTIFY] = True
    return config


ble_client_ns = cg.esphome_ns.namespace("ble_client")
# One codegen class for both engines: the exclusively-gated headers resolve
# the name to exactly one C++ definition per build.
BLEClient = ble_client_ns.class_("BLEClient", cg.Component)
BLEClientNode = ble_client_ns.class_("BLEClientNode")
BLEClientNodeConstRef = BLEClientNode.operator("ref").operator("const")
# Triggers
BLEClientConnectTrigger = ble_client_ns.class_(
    "BLEClientConnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientDisconnectTrigger = ble_client_ns.class_(
    "BLEClientDisconnectTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientPasskeyRequestTrigger = ble_client_ns.class_(
    "BLEClientPasskeyRequestTrigger", automation.Trigger.template(BLEClientNodeConstRef)
)
BLEClientPasskeyNotificationTrigger = ble_client_ns.class_(
    "BLEClientPasskeyNotificationTrigger",
    automation.Trigger.template(BLEClientNodeConstRef, cg.uint32),
)
BLEClientNumericComparisonRequestTrigger = ble_client_ns.class_(
    "BLEClientNumericComparisonRequestTrigger",
    automation.Trigger.template(BLEClientNodeConstRef, cg.uint32),
)

# Actions
BLEWriteAction = ble_client_ns.class_("BLEClientWriteAction", automation.Action)
BLEConnectAction = ble_client_ns.class_("BLEClientConnectAction", automation.Action)
BLEDisconnectAction = ble_client_ns.class_(
    "BLEClientDisconnectAction", automation.Action
)
BLEPasskeyReplyAction = ble_client_ns.class_(
    "BLEClientPasskeyReplyAction", automation.Action
)
BLENumericComparisonReplyAction = ble_client_ns.class_(
    "BLEClientNumericComparisonReplyAction", automation.Action
)
BLERemoveBondAction = ble_client_ns.class_(
    "BLEClientRemoveBondAction", automation.Action
)

CONF_PASSKEY = "passkey"
CONF_ACCEPT = "accept"
CONF_ON_PASSKEY_REQUEST = "on_passkey_request"
CONF_ON_PASSKEY_NOTIFICATION = "on_passkey_notification"
CONF_ON_NUMERIC_COMPARISON_REQUEST = "on_numeric_comparison_request"
CONF_AUTO_CONNECT = "auto_connect"

MULTI_CONF = True

# Keys shared by both engines' schemas.
_COMMON_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEClient),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_AUTO_CONNECT, default=True): cv.boolean,
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEClientConnectTrigger),
            }
        ),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEClientDisconnectTrigger
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@functools.cache
def _esp32_config_schema() -> cv.All:
    """The legacy engine's schema, byte-compatible with what esp32 always had
    (including the Bluedroid security triggers)."""
    from esphome.components import esp32_ble_tracker

    return cv.All(
        _COMMON_SCHEMA.extend(
            {
                # Accepted-but-unused legacy key; not propagated to the
                # neutral schema.
                cv.Optional(CONF_NAME): cv.string,
                cv.Optional(CONF_ON_PASSKEY_REQUEST): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            BLEClientPasskeyRequestTrigger
                        ),
                    }
                ),
                cv.Optional(
                    CONF_ON_PASSKEY_NOTIFICATION
                ): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            BLEClientPasskeyNotificationTrigger
                        ),
                    }
                ),
                cv.Optional(
                    CONF_ON_NUMERIC_COMPARISON_REQUEST
                ): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            BLEClientNumericComparisonRequestTrigger
                        ),
                    }
                ),
            }
        ).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA),
        bluetooth_connection.consume_gatt_slot("ble_client"),
    )


@functools.cache
def _gatt_config_schema(platform: str) -> cv.All:
    """The neutral engine's schema: the shared keys plus the hub reference
    (parsed-advertisement sightings) and the GATT backend declaration.
    Keyed by platform - the backend fragment differs per platform."""
    return cv.All(
        _COMMON_SCHEMA.extend(ble_device_base.BLE_DEVICE_SCHEMA).extend(
            bluetooth_connection.gatt_client_schema(platform)
        ),
        bluetooth_connection.consume_gatt_slot("ble_client"),
    )


@schema_extractor("schema")
def _validate_platform(config: ConfigType) -> ConfigType:
    if config is SCHEMA_EXTRACT:
        # Deliberate gap (the bluetooth_proxy pattern): the dumper gets only
        # this shape, so the neutral arm's ble_hub_id is absent from editor
        # schemas and the esp32-only keys are advertised on every platform.
        # The language-schema dumper runs without a platform; expose the
        # esp32 (legacy-engine) shape.
        return _esp32_config_schema()
    if _legacy_engine():
        return _esp32_config_schema()(config)
    if CORE.target_platform in bluetooth_connection.GATT_CLIENT_PLATFORMS:
        return _gatt_config_schema(CORE.target_platform)(config)
    raise cv.Invalid(f"ble_client is not supported on {CORE.target_platform}")


CONFIG_SCHEMA = _validate_platform

CONF_BLE_CLIENT_ID = "ble_client_id"


class BLEClientFeatures(StrEnum):
    """Per-platform engine capabilities consumers declare against."""

    # The platform-neutral node interface (on_connected/table + completion
    # callbacks) - every platform with a ble_client engine.
    GATT_NODE = "gatt_node"
    # The raw esp32 GATT client event stream (gattc/gap handlers,
    # node_state) - the legacy engine only.
    RAW_GATTC = "raw_gattc"
    # Pairing dialog replies and bond management (Bluedroid GAP/SMP).
    SECURITY = "security"


def _engine_features() -> set[BLEClientFeatures]:
    """Features the validated platform's engine provides."""
    if _legacy_engine():
        return {
            BLEClientFeatures.GATT_NODE,
            BLEClientFeatures.RAW_GATTC,
            BLEClientFeatures.SECURITY,
        }
    if CORE.target_platform in bluetooth_connection.GATT_CLIENT_PLATFORMS:
        return {BLEClientFeatures.GATT_NODE}
    return set()


def requires_feature(
    feature: BLEClientFeatures, description: str
) -> Callable[[Any], Any]:
    """Validator gating a consumer to platforms whose engine provides
    `feature`, naming the missing capability in the error."""

    def validator(value: Any) -> Any:
        features = _engine_features()
        if feature not in features:
            available = (
                f"; this platform's engine provides: {', '.join(sorted(features))}"
                if features
                else ""
            )
            raise cv.Invalid(
                f"{description} requires the ble_client '{feature}' feature, "
                f"which {CORE.target_platform} does not provide{available}"
            )
        return value

    return validator


# The one choke point for every node component still on the raw esp32 event
# stream; migrating to the neutral interface (NODE_BLE_CLIENT_SCHEMA +
# register_gatt_node) lifts it.
_legacy_engine_only = requires_feature(
    BLEClientFeatures.RAW_GATTC,
    "This component drives the raw ESP32 GATT client events and has not "
    "been migrated to the platform-neutral node interface yet; it",
)

BLE_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_CLIENT_ID): cv.All(
            cv.use_id(BLEClient), _legacy_engine_only
        ),
    }
)

# For node components on the neutral interface: valid wherever ble_client
# itself is.
NODE_BLE_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_CLIENT_ID): cv.All(
            cv.use_id(BLEClient),
            requires_feature(BLEClientFeatures.GATT_NODE, "This component"),
        ),
    }
)


async def register_ble_node(var, config):
    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(parent.register_ble_node(var))


def _request_gatt_node_build() -> None:
    """Node storage and the one define meaning "the neutral node surface is
    compiled in", plus the esp32 bridge/materializer defines."""
    _request_node_slot()
    cg.add_define("USE_BLE_CLIENT_GATT_NODES")
    if _legacy_engine():
        # Deliberately not ble_device_base.request_gatt_client(): that would
        # claim a phantom backend slot on combined proxy builds.
        cg.add_define("USE_BLE_GATT_CLIENT")
        cg.add_define("USE_BLE_GATT_BACKEND_BLUEDROID")
        cg.add_define("USE_BLUEDROID_GATT_SERVICE_TABLE")


async def register_gatt_node(var, config):
    """Register a node on the platform-neutral interface (both engines)."""
    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    _request_gatt_node_build()
    cg.add(parent.register_gatt_node(var))


BLE_WRITE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
        cv.Required(CONF_SERVICE_UUID): bt_uuid,
        cv.Required(CONF_CHARACTERISTIC_UUID): bt_uuid,
        cv.Required(CONF_VALUE): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
    }
)

BLE_CONNECT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
    }
)

BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA = cv.All(
    requires_feature(BLEClientFeatures.SECURITY, "This action"),
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
            cv.Required(CONF_ACCEPT): cv.templatable(cv.boolean),
        }
    ),
)

BLE_PASSKEY_REPLY_ACTION_SCHEMA = cv.All(
    requires_feature(BLEClientFeatures.SECURITY, "This action"),
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
            cv.Required(CONF_PASSKEY): cv.templatable(cv.int_range(min=0, max=999999)),
        }
    ),
)


BLE_REMOVE_BOND_ACTION_SCHEMA = cv.All(
    requires_feature(BLEClientFeatures.SECURITY, "This action"),
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(BLEClient),
        }
    ),
)


@automation.register_action(
    "ble_client.disconnect",
    BLEDisconnectAction,
    BLE_CONNECT_ACTION_SCHEMA,
    synchronous=False,
)
async def ble_disconnect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "ble_client.connect",
    BLEConnectAction,
    BLE_CONNECT_ACTION_SCHEMA,
    synchronous=False,
)
async def ble_connect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "ble_client.ble_write",
    BLEWriteAction,
    BLE_WRITE_ACTION_SCHEMA,
    synchronous=False,
)
async def ble_write_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    # The action registers itself as a neutral node in its constructor.
    _request_gatt_node_build()
    var = cg.new_Pvariable(action_id, template_arg, parent)

    value = config[CONF_VALUE]
    if cg.is_template(value):
        templ = await cg.templatable(value, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_value_template(templ))
    else:
        # Generate static array in flash to avoid RAM copy
        if isinstance(value, bytes):
            value = list(value)
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*value))
        cg.add(var.set_value_simple(arr, len(value)))

    if len(config[CONF_SERVICE_UUID]) == len(bt_uuid16_format):
        cg.add(var.set_service_uuid16(as_hex(config[CONF_SERVICE_UUID])))
    elif len(config[CONF_SERVICE_UUID]) == len(bt_uuid32_format):
        cg.add(var.set_service_uuid32(as_hex(config[CONF_SERVICE_UUID])))
    elif len(config[CONF_SERVICE_UUID]) == len(bt_uuid128_format):
        uuid128 = as_reversed_hex_array(config[CONF_SERVICE_UUID])
        cg.add(var.set_service_uuid128(uuid128))

    if len(config[CONF_CHARACTERISTIC_UUID]) == len(bt_uuid16_format):
        cg.add(var.set_char_uuid16(as_hex(config[CONF_CHARACTERISTIC_UUID])))
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(bt_uuid32_format):
        cg.add(var.set_char_uuid32(as_hex(config[CONF_CHARACTERISTIC_UUID])))
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(bt_uuid128_format):
        uuid128 = as_reversed_hex_array(config[CONF_CHARACTERISTIC_UUID])
        cg.add(var.set_char_uuid128(uuid128))

    return var


@automation.register_action(
    "ble_client.numeric_comparison_reply",
    BLENumericComparisonReplyAction,
    BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA,
    synchronous=True,
)
async def numeric_comparison_reply_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    accept = config[CONF_ACCEPT]
    if cg.is_template(accept):
        templ = await cg.templatable(accept, args, cg.bool_)
        cg.add(var.set_value_template(templ))
    else:
        cg.add(var.set_value_simple(accept))

    return var


@automation.register_action(
    "ble_client.passkey_reply",
    BLEPasskeyReplyAction,
    BLE_PASSKEY_REPLY_ACTION_SCHEMA,
    synchronous=True,
)
async def passkey_reply_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    passkey = config[CONF_PASSKEY]
    if cg.is_template(passkey):
        templ = await cg.templatable(passkey, args, cg.uint32)
        cg.add(var.set_value_template(templ))
    else:
        cg.add(var.set_value_simple(passkey))

    return var


@automation.register_action(
    "ble_client.remove_bond",
    BLERemoveBondAction,
    BLE_REMOVE_BOND_ACTION_SCHEMA,
    synchronous=True,
)
async def remove_bond_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


async def _to_code_esp32(config: ConfigType) -> cg.MockObj:
    from esphome.components import esp32_ble, esp32_ble_tracker
    from esphome.components.esp32_ble import BTLoggers

    # Register the loggers this component needs
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.SMP)
    cg.add_define("USE_ESP32_BLE_UUID")
    cg.add_define("USE_BLE_CLIENT_LEGACY_ENGINE")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_client(var, config)
    return var


# Sizes the neutral client's node storage; the client itself requests a
# baseline slot so the define exists on every build that compiles the engine.
_request_node_slot = cg.slot_counter("ESPHOME_BLE_CLIENT_MAX_NODES")


async def _to_code_gatt(config: ConfigType) -> cg.MockObj:
    # The engine always carries the node surface (the client itself owns the
    # baseline slot).
    _request_gatt_node_build()
    backend = await bluetooth_connection.new_gatt_backend(config)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_backend(backend))
    # Sighting-gated connects: the client listens for the peer's parsed
    # advertisements through the hub.
    await ble_device_base.register_ble_device(var, config)
    return var


async def to_code(config: ConfigType) -> None:
    if _legacy_engine():
        var = await _to_code_esp32(config)
    else:
        var = await _to_code_gatt(config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_auto_connect(config[CONF_AUTO_CONNECT]))
    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PASSKEY_REQUEST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PASSKEY_NOTIFICATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "passkey")], conf)
    for conf in config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint32, "passkey")], conf)

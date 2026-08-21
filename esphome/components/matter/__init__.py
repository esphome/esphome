from pathlib import Path

from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32S3,
    add_idf_component,
    add_idf_sdkconfig_option,
    get_esp32_variant,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_IPV6, CONF_ID, CONF_INTERNAL, CONF_PLATFORM
from esphome.core import CORE, EsphomeError
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

# VARIANT_ESP32 / get_esp32_variant are still imported: the PSRAM-override
# coroutine below uses them to gate the NimBLE-in-PSRAM tweak, which is
# variant-specific. The source-patch hook, in contrast, now runs on every
# variant (see _write_cmake_project_include).
#
# esp-matter 1.6.0 upstream supports these five ESP32 variants only. Other
# variants either lack Wi-Fi/BLE on-chip (P4, S2), are too RAM-constrained
# (C2), or are too new for the SDK to have been ported (C5, C61, H4, H21,
# S31). Config validation below rejects unsupported variants at config
# time; source files also carry a compile-time guard so clang-tidy on the
# variant-specific jobs strips them out (esp_matter.h isn't shipped for
# those targets).
_MATTER_SUPPORTED_VARIANTS = [
    VARIANT_ESP32,
    VARIANT_ESP32S3,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
]

CODEOWNERS = ["@gtjadsonsantos"]
DEPENDENCIES = ["esp32"]

matter_ns = cg.esphome_ns.namespace("matter")
MatterComponent = matter_ns.class_("MatterComponent", cg.Component)

CONF_MATTER = "matter"
CONF_VENDOR_ID = "vendor_id"
CONF_PRODUCT_ID = "product_id"
CONF_SETUP_CODE = "setup_code"
CONF_DISCRIMINATOR = "discriminator"
# Human-readable identity strings written to the chip-factory NVS namespace
# on boot so CHIP's BasicInformation cluster serves them instead of the
# esp-matter defaults (TEST_VENDOR / TEST_PRODUCT). All optional — empty
# strings are skipped (the CHIP default remains).
CONF_VENDOR_NAME = "vendor_name"
CONF_PRODUCT_NAME = "product_name"
CONF_NODE_LABEL = "node_label"
CONF_HARDWARE_VERSION_STRING = "hardware_version_string"
CONF_SOFTWARE_VERSION_STRING = "software_version_string"
# Opt-in to Matter's spec-native BLE commissioning path. When true the device
# advertises via BLE, receives Wi-Fi creds over BTP, and joins on-network.
# Costs ~150KB flash + a BT/BLE stack in RAM. Requires Wi-Fi transport (BLE
# commissioning makes no sense on Ethernet — the fabric is already reachable).
CONF_BLE_COMMISSIONING = "ble_commissioning"
# Matter endpoint topology mode. "bridge" (default) creates an Aggregator
# endpoint (device type 0x000E) as the parent of every entity endpoint and
# attaches a BridgedDeviceBasicInformation cluster with a per-endpoint
# NodeLabel — the classic Matter Bridge presentation, where each entity shows
# up as its own accessory in Apple Home / Google Home / SmartThings. Some
# consumer hubs (eWeLink Cube / NSPanel Pro app path) misbehave with bridges
# that carry more than a handful of endpoints — they get stuck polling
# AdministratorCommissioning attributes per bridged endpoint and the UI marks
# every sub-device offline even though CASE + subscriptions are healthy.
# "composed" skips both the Aggregator and BDBI: all entity endpoints attach
# directly to the root_node as parts of one composed device (Matter spec §9.5).
# Trade-off is naming: without BDBI.NodeLabel some controllers fall back to
# generic labels ("Endpoint 2") and rely on FixedLabel — served by our custom
# DeviceInfoProvider — which not every controller reads.
CONF_TOPOLOGY = "topology"
_TOPOLOGY_BRIDGE = "bridge"
_TOPOLOGY_COMPOSED = "composed"

# Internal marker set by FINAL_VALIDATE and read in to_code so the sdkconfig
# reflects the actual transport (Ethernet vs Wi-Fi). Not part of the user
# schema.
_CONF_TRANSPORT_ETHERNET = "_transport_ethernet"
# Detected in _validate_final and consumed by _apply_matter_psram_overrides
# in to_code. Underscore-prefixed so it never collides with a user key.
_CONF_PSRAM_PRESENT = "_psram_present"
# Auto-computed Matter dynamic endpoint budget populated by FINAL_VALIDATE and
# consumed in to_code. Also carries a flag noting whether the user pinned the
# value themselves in esp32.framework.sdkconfig_options, in which case we
# defer to their choice.
_CONF_MAX_DYNAMIC_ENDPOINT_COUNT = "_max_dynamic_endpoint_count"
_CONF_MAX_DYNAMIC_ENDPOINT_COUNT_PINNED = "_max_dynamic_endpoint_count_pinned"

# Kconfig for CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT: range [1, 255],
# default 16. Every entity gets its own dynamic endpoint plus root_node (id 0)
# and the aggregator (bridge topology). Keep a small margin so trivial edits
# don't force a sdkconfig change.
_MATTER_ENDPOINT_KCONFIG_MIN = 1
_MATTER_ENDPOINT_KCONFIG_MAX = 255
_MATTER_ENDPOINT_KCONFIG_DEFAULT = 16
# root_node + aggregator both count against CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT
# (esp_matter_data_model.cpp:1659 checks get_count(node) against it, and both
# create() calls end up in that count).
_MATTER_FIXED_ENDPOINT_OVERHEAD = 2
# Extra headroom on top of the counted entities so casual yaml tweaks
# (add/remove a couple of switches) don't require rebuilding esp-matter.
_MATTER_ENDPOINT_HEADROOM = 4

# ESPHome platform-family keys the C++ MatterComponent scanner turns into
# Matter endpoints. Must stay in sync with scan_and_register_*() in
# matter_component.cpp.
_MATTER_ENDPOINT_ENTITY_KEYS = (
    "switch",
    "binary_sensor",
    "sensor",
    "cover",
    "fan",
    "lock",
    "valve",
    "select",
    "button",
    "light",
    "climate",
)

ESP_MATTER_REF = "1.6.0"

# Matter spec length caps on BasicInformation strings. Enforced in the schema
# so users see the limit at config-validate time instead of a truncated string
# at runtime.
_MAX_VENDOR_NAME_LEN = 32
_MAX_PRODUCT_NAME_LEN = 32
_MAX_NODE_LABEL_LEN = 32
_MAX_HARDWARE_VERSION_STRING_LEN = 64
_MAX_SOFTWARE_VERSION_STRING_LEN = 64


def _string_with_max(max_len: int):
    def validator(value: str) -> str:
        v = cv.string_strict(value)
        if len(v) > max_len:
            raise cv.Invalid(
                f"Matter identity string is capped at {max_len} chars, got {len(v)}"
            )
        return v

    return validator


# Reserved passcodes from Matter spec §5.1.6.1 — repeating digits, ascending
# 12345678, descending 87654321. Commissioners reject them, so surface at
# config time instead of shipping a device that will never pair.
_MATTER_RESERVED_PASSCODES = frozenset(
    {
        "00000000",
        "11111111",
        "22222222",
        "33333333",
        "44444444",
        "55555555",
        "66666666",
        "77777777",
        "88888888",
        "99999999",
        "12345678",
        "87654321",
    }
)


def _validate_setup_code(value: str) -> str:
    v = cv.string_strict(value)
    if not v.isdigit():
        raise cv.Invalid(
            f"matter.setup_code must be an 8-digit numeric string, got {v!r}"
        )
    if len(v) != 8:
        raise cv.Invalid(
            f"matter.setup_code must be exactly 8 digits (leading zeros allowed), "
            f"got {len(v)} digit(s)"
        )
    n = int(v)
    if n < 1 or n > 99999998:
        raise cv.Invalid(
            "matter.setup_code numeric value must be in [1, 99999998] per Matter spec"
        )
    if v in _MATTER_RESERVED_PASSCODES:
        raise cv.Invalid(
            f"matter.setup_code {v!r} is reserved by Matter spec and will be "
            f"rejected by commissioners"
        )
    return v


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MatterComponent),
            cv.Required(CONF_VENDOR_ID): cv.hex_uint16_t,
            cv.Required(CONF_PRODUCT_ID): cv.hex_uint16_t,
            cv.Required(CONF_SETUP_CODE): _validate_setup_code,
            cv.Required(CONF_DISCRIMINATOR): cv.int_range(min=0, max=0xFFF),
            cv.Optional(CONF_VENDOR_NAME, default=""): _string_with_max(
                _MAX_VENDOR_NAME_LEN
            ),
            cv.Optional(CONF_PRODUCT_NAME, default=""): _string_with_max(
                _MAX_PRODUCT_NAME_LEN
            ),
            cv.Optional(CONF_NODE_LABEL, default=""): _string_with_max(
                _MAX_NODE_LABEL_LEN
            ),
            cv.Optional(CONF_HARDWARE_VERSION_STRING, default=""): _string_with_max(
                _MAX_HARDWARE_VERSION_STRING_LEN
            ),
            cv.Optional(CONF_SOFTWARE_VERSION_STRING, default=""): _string_with_max(
                _MAX_SOFTWARE_VERSION_STRING_LEN
            ),
            cv.Optional(CONF_BLE_COMMISSIONING, default=False): cv.boolean,
            cv.Optional(CONF_TOPOLOGY, default=_TOPOLOGY_BRIDGE): cv.one_of(
                _TOPOLOGY_BRIDGE, _TOPOLOGY_COMPOSED, lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    only_on_variant(
        supported=_MATTER_SUPPORTED_VARIANTS,
        msg_prefix="The 'matter' component",
    ),
)


def _is_esp_idf() -> bool:
    # CORE.target_framework is the single source of truth in this tree —
    # the previous CORE.using_esp_idf probe and CORE.data["esp32"] fallback
    # were left over from an older API shape and never fire.
    return CORE.target_framework == "esp-idf"


def _count_entry_endpoints(key: str, entry: dict) -> int:
    """Return how many ESPHome entities a single platform-family entry
    registers. Handles multi-output platforms whose sub-schemas each
    define an entity (e.g. ``sensor: - platform: bme280_i2c`` with
    ``temperature:``, ``humidity:``, ``pressure:`` sub-blocks).

    Returns 0 if the entry itself is marked internal — the top-level
    ``internal:`` filter still applies. Sub-block-level internal flags
    aren't inspected: leaving those in the total is the safer bias.
    """
    if entry.get(CONF_INTERNAL, False):
        return 0
    # Platforms whose sub-keys carry their own name/id → count sub-entities.
    # sensor and binary_sensor are the common ones (BME280 fans out to 3
    # sensors, DHT to 2). switch/light/etc. are one entity per entry.
    if key in ("sensor", "binary_sensor"):
        sub_count = 0
        for sub_val in entry.values():
            if not isinstance(sub_val, dict):
                continue
            if sub_val.get(CONF_INTERNAL, False):
                continue
            if "name" in sub_val or "id" in sub_val:
                sub_count += 1
        # Some entries publish directly (template sensor with name at the
        # entry level) and have zero name/id-bearing sub-dicts; fall back
        # to 1 there.
        return max(1, sub_count)
    return 1


def _count_matter_endpoint_entities(full_conf) -> int:
    """Count ESPHome entities the C++ scanner will turn into Matter endpoints.

    Mirrors scan_and_register_*() in matter_component.cpp:
    - one endpoint per non-internal entity across the platform-family keys
      listed in ``_MATTER_ENDPOINT_ENTITY_KEYS``, expanding multi-output
      sensor platforms to their sub-entity count (see _count_entry_endpoints);
    - ``button: platform: matter`` (MatterActionButton) is skipped — the
      scanner filters it out via ``MatterActionButton::is_instance``;
    - sensors without a supported unit/device_class are dropped by
      ``detect_kind()`` at runtime, but we cannot cheaply reproduce that
      heuristic here without pulling in every platform's schema, so we
      OVER-count on purpose (better a couple of unused endpoint slots than
      a boot-time abort in ``esp_matter_data_model.cpp`` on overflow).
    """
    total = 0
    for key in _MATTER_ENDPOINT_ENTITY_KEYS:
        try:
            entries = full_conf.get_config_for_path([key])
        except KeyError:
            continue
        if entries is None:
            continue
        if isinstance(entries, dict):
            entries = [entries]
        if not isinstance(entries, list):
            raise cv.Invalid(
                f"matter: unexpected shape for '{key}' block "
                f"({type(entries).__name__}) — cannot count endpoints"
            )
        for entry in entries:
            if not isinstance(entry, dict):
                raise cv.Invalid(
                    f"matter: unexpected entry shape under '{key}' "
                    f"({type(entry).__name__}) — cannot count endpoints"
                )
            if key == "button" and entry.get(CONF_PLATFORM) == "matter":
                continue
            total += _count_entry_endpoints(key, entry)
    return total


def _validate_final(config: ConfigType) -> ConfigType:
    if not _is_esp_idf():
        raise cv.Invalid(
            "The matter component requires framework 'esp-idf'. "
            "Set esp32.framework.type: esp-idf on this device."
        )
    # Matter mandates IPv6 fabric transport. ESPHome's network: component owns
    # the LWIP IPv6 sdkconfig; require the user to explicitly opt in there
    # rather than silently forcing it under their feet.
    full_conf = fv.full_config.get()
    try:
        network_conf = full_conf.get_config_for_path(["network"])
    except KeyError:
        network_conf = None
    ipv6_enabled = bool(network_conf and network_conf.get(CONF_ENABLE_IPV6, False))
    if not ipv6_enabled:
        raise cv.Invalid(
            "The matter component requires IPv6. Add:\n"
            "  network:\n"
            "    enable_ipv6: true\n"
            "to your yaml (Matter fabric transport is IPv6-only)."
        )

    # Determine which network transport carries the Matter fabric. At least one
    # of ethernet: or wifi: must be present — Matter has no other transport on
    # the classic ESP32. When ethernet: is present, esp-matter's Ethernet
    # NetworkCommissioning driver TU gets compiled by CHIP (via
    # CONFIG_ENABLE_ETHERNET_TELEMETRY), and matter_ethernet_stub.cpp is also
    # compiled (via USE_ETHERNET) to override the PHY-init path so it does not
    # fight ESPHome's ethernet: component for the PHY. On a pure Wi-Fi build
    # both are compiled out.
    has_ethernet = False
    try:
        full_conf.get_config_for_path(["ethernet"])
        has_ethernet = True
    except KeyError:
        pass
    has_wifi = False
    try:
        full_conf.get_config_for_path(["wifi"])
        has_wifi = True
    except KeyError:
        pass
    if not has_ethernet and not has_wifi:
        raise cv.Invalid(
            "The matter component requires a network transport. Add either an "
            "ethernet: or a wifi: block to your yaml — Matter has no other "
            "transport on the ESP32."
        )
    config[_CONF_TRANSPORT_ETHERNET] = has_ethernet

    # Detect PSRAM presence up-front — used both by the BLE-commissioning
    # branch below (which forces SPIRAM_USE_MALLOC to fit NimBLE + CHIP into
    # a mixed internal+PSRAM heap) and by the plain Matter path (where a
    # bridge with many endpoints exhausts internal DRAM otherwise, starving
    # DMA-capable allocations like the W5500 SPI priv RX buffer). Flipped
    # later in _apply_matter_psram_overrides.
    try:
        full_conf.get_config_for_path(["psram"])
        config[_CONF_PSRAM_PRESENT] = True
    except KeyError:
        # "no psram: block" — expected for many configs; the SPIRAM_USE_MALLOC
        # overrides just don't apply. Any other exception is a real
        # path-resolution bug and should surface at config time rather than
        # be silently mapped to "no psram".
        config[_CONF_PSRAM_PRESENT] = False

    if config.get(CONF_BLE_COMMISSIONING, False):
        # BLE commissioning is spec-native for Wi-Fi Matter devices only. On
        # Ethernet the fabric can already reach the device over the LAN — BLE
        # is dead weight there.
        if not has_wifi:
            raise cv.Invalid(
                "matter.ble_commissioning: true requires a wifi: block. "
                "BLE commissioning provisions Wi-Fi credentials — it is not "
                "used on Ethernet builds."
            )
        # esp-matter needs sole ownership of the BLE controller during
        # commissioning. If ESPHome's own BLE components also want it, they
        # will fight for the controller and one of them fails to init.
        for conflicting in (
            "esp32_ble",
            "esp32_ble_tracker",
            "esp32_ble_beacon",
            "bluetooth_proxy",
            "improv_serial",
            "esp32_improv",
        ):
            try:
                full_conf.get_config_for_path([conflicting])
            except (KeyError, TypeError):
                continue
            raise cv.Invalid(
                f"matter.ble_commissioning: true is incompatible with the "
                f"'{conflicting}' component — esp-matter needs sole ownership "
                f"of the BLE controller during commissioning."
            )
        # PSRAM is strongly recommended (BLE + Wi-Fi + Matter is tight on the
        # 320KB internal RAM of the classic ESP32), but not required — a
        # board without PSRAM can still try, at the cost of higher OOM risk.
        # When present, we flip SPIRAM_USE_MALLOC + move NimBLE to PSRAM in
        # _apply_matter_psram_overrides. When absent, we keep NimBLE on
        # internal RAM and rely on the SRAM1_AS_IRAM (+40KB DRAM) trick and
        # reduced Wi-Fi buffers to survive.
        # _psram_present is set above, before this branch

    # Auto-compute CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT so users don't
    # have to hand-tune it every time they add or remove an entity. The
    # Kconfig default (16) is far below the entity count of a modest bridge
    # (tested with ~74 bridged entities on one board), and overrunning the
    # limit aborts endpoint creation at boot. Sum: entity endpoints scanned
    # by the C++ side
    # (scan_and_register_*), plus root_node + aggregator overhead, plus a small
    # headroom so incidental yaml edits don't force a sdkconfig change and a
    # full esp-matter rebuild. Clamped to the Kconfig range; floored at the
    # Kconfig default so tiny devices stay at the vendor baseline.
    entity_count = _count_matter_endpoint_entities(full_conf)
    computed = (
        entity_count + _MATTER_FIXED_ENDPOINT_OVERHEAD + _MATTER_ENDPOINT_HEADROOM
    )
    computed = max(computed, _MATTER_ENDPOINT_KCONFIG_DEFAULT)
    # Refuse to silently clamp at the Kconfig ceiling: if the entity count
    # exceeds what esp-matter can hold, the user needs to see it at config
    # time (with the concrete numbers) rather than getting a boot-time abort
    # in esp_matter_data_model.cpp after firmware ships.
    if computed > _MATTER_ENDPOINT_KCONFIG_MAX:
        raise cv.Invalid(
            f"matter: config needs {computed} Matter endpoints "
            f"({entity_count} entities + {_MATTER_FIXED_ENDPOINT_OVERHEAD} "
            f"root/aggregator + {_MATTER_ENDPOINT_HEADROOM} headroom) but "
            f"esp-matter's Kconfig ceiling is "
            f"{_MATTER_ENDPOINT_KCONFIG_MAX}. Reduce the number of exposed "
            f"entities or mark unused ones internal: true."
        )
    computed = max(computed, _MATTER_ENDPOINT_KCONFIG_MIN)
    config[_CONF_MAX_DYNAMIC_ENDPOINT_COUNT] = computed

    # Respect a user-pinned value in esp32.framework.sdkconfig_options — power
    # users may want to tune this manually, and silently overriding them from
    # under the hood would surprise. Detected here (FINAL_VALIDATE has visibility
    # into the full merged config); to_code just skips the auto-set branch.
    pinned = False
    try:
        sdk_opts = full_conf.get_config_for_path(
            ["esp32", "framework", "sdkconfig_options"]
        )
    except (KeyError, TypeError):
        sdk_opts = None
    if isinstance(sdk_opts, dict) and (
        "CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT" in sdk_opts
    ):
        pinned = True
    config[_CONF_MAX_DYNAMIC_ENDPOINT_COUNT_PINNED] = pinned

    return config


FINAL_VALIDATE_SCHEMA = _validate_final


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_VENDOR_ID],
        config[CONF_PRODUCT_ID],
        config[CONF_SETUP_CODE],
        config[CONF_DISCRIMINATOR],
        config[CONF_BLE_COMMISSIONING],
        config[CONF_VENDOR_NAME],
        config[CONF_PRODUCT_NAME],
        config[CONF_NODE_LABEL],
        config[CONF_HARDWARE_VERSION_STRING],
        config[CONF_SOFTWARE_VERSION_STRING],
        config[CONF_TOPOLOGY] == _TOPOLOGY_COMPOSED,
    )
    await cg.register_component(var, config)

    add_idf_component(name="espressif/esp_matter", ref=ESP_MATTER_REF)

    # Transport is picked up by FINAL_VALIDATE from whether the user configured
    # ethernet: or wifi: (both are allowed simultaneously; presence of ethernet:
    # wins for ETHERNET_TELEMETRY purposes because that is what pulls in the
    # ESPEthernetDriver TU we override). BLE is off — on-network commissioning
    # is used (device is already reachable on the LAN before pairing). IPv6 is
    # a hard requirement for Matter but is enabled through the ESPHome
    # `network:` component (network.enable_ipv6: true) which sets
    # CONFIG_LWIP_IPV6=y + CONFIG_LWIP_IPV6_AUTOCONFIG=y itself — we validate
    # that upstream in FINAL_VALIDATE_SCHEMA rather than duplicating the
    # sdkconfig here.
    #
    # Custom partition table is intentionally NOT set here. ESPHome manages
    # partitions its own way; the Matter fctry namespace lives in the default
    # nvs partition. Revisit for certified/production builds.
    transport_ethernet = bool(config.get(_CONF_TRANSPORT_ETHERNET, False))
    ble_commissioning = bool(config.get(CONF_BLE_COMMISSIONING, False))
    sdkconfig = {
        # Enabled whenever the user configured an ethernet: block. On a pure
        # Wi-Fi build ESPEthernetDriver is not compiled by CHIP, so leaving
        # this on would pull references to a driver we neither drive nor need.
        # On non-EMAC variants (S3/S2/C3/C6/H2) the CHIP TU that hardcodes the
        # internal ESP32 MAC (NetworkCommissioningDriver_Ethernet.cpp) is
        # stripped from the esp-matter library via CMake surgery in our
        # CMakeLists.txt — matter_ethernet_stub.cpp still provides
        # ESPEthernetDriver::Init, and ConnectivityManagerImpl_Ethernet.cpp
        # compiles cleanly on those variants because it only touches
        # esp_netif_init() and esp_eth.h enums.
        "CONFIG_ENABLE_ETHERNET_TELEMETRY": transport_ethernet,
        # Enabled whenever we actually want the CHIP Wi-Fi station to run:
        # every non-Ethernet build, plus Ethernet builds that use BLE
        # commissioning (the commissioner ships Wi-Fi credentials over BTP
        # and CHIP needs the station up to receive them — even though on
        # our devices the runtime data path is Ethernet).
        #
        # On a pure Ethernet-only build (no BLE), keeping this off means
        # CHIP_DEVICE_CONFIG_ENABLE_WIFI=0 and ConnectivityManagerImpl::_Init
        # skips InitWiFi() entirely (connectedhomeip src/platform/ESP32/
        # ConnectivityManagerImpl.cpp:59-71 in the 1.6 baseline), so CHIP
        # never touches the never-initialised Wi-Fi driver.
        #
        # The historic 1.5.1 reason to keep this on even in Ethernet builds
        # was a esp-matter CMakeLists.txt bug that excluded ESP32DnssdImpl.cpp
        # when both WIFI_STATION and WIFI_AP were off; 1.6.0 rewrote the
        # CMake and the guard is gone.
        "CONFIG_ENABLE_WIFI_STATION": (not transport_ethernet) or ble_commissioning,
        # Conditional: on Ethernet builds we keep this OFF to avoid a
        # double-Create race on endpoint 0 (both ETHERNET_ and WIFI_
        # NetworkCommissioningDriver would try to register the same
        # NetworkCommissioning cluster). On Wi-Fi-only builds we MUST turn
        # it on, otherwise kNetworkCommissioningClusterCount reduces to 0
        # and network_commissioning_integration.cpp fails its static_assert.
        # ESPWiFiDriver::Init is benign (reads staged config + registers a
        # callback), so it does not fight ESPHome's wifi: component the way
        # the Ethernet driver did.
        "CONFIG_WIFI_NETWORK_COMMISSIONING_DRIVER": not transport_ethernet,
        "CONFIG_CHIP_MDNS_PLATFORM": True,
        "CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER": True,
        # Matter is IPv6-only in the spec (Core 1.4 §4.3.1), but consumer
        # hubs are not always spec-clean: the Sonoff NS Panel Gen2 / eWeLink
        # Cube path was observed to pin the peer address to an IPv4 socket
        # at commissioning time and then reach us only over v4 afterwards.
        # Setting CONFIG_DISABLE_IPV4=y silenced sporadic LwIP `ERR_IF`
        # (0x0300000c) on the report path — but also cut NS Panel Gen2 off
        # the bridge entirely (device import stopped). Keep IPv4 enabled in
        # CHIP by default so those hubs stay reachable.
        #
        # Advanced users on an all-v6-clean fabric who want to drop IPv4
        # from CHIP for flash savings should NOT set
        # `CONFIG_DISABLE_IPV4=y` — that path in esp_matter's CMakeLists
        # hits a FATAL_ERROR unless CONFIG_LWIP_IPV4=n, which would also
        # break ESPHome's own api/ota/web-server. Instead, pass the
        # C-preprocessor macro directly via
        # `esphome.build_flags: ["-DCHIP_DEVICE_CONFIG_ENABLE_IPV4=false"]`
        # or the equivalent framework flag — CHIP compiles IPv6-only,
        # LwIP IPv4 stays live for the rest of the application, no CMake
        # gate involved.
        "CONFIG_DISABLE_IPV4": False,
        # Matter-native BLE commissioning path. When enabled, the device
        # advertises via BLE, the fabric-side commissioner picks up the payload
        # via BTP, sends the target Wi-Fi credentials, and the device joins.
        # Requires the underlying BT stack (BT_ENABLED + a BLE host stack) —
        # we use NimBLE below because it is smaller than Bluedroid on the
        # classic ESP32 (~50KB delta) and CHIP supports both.
        "CONFIG_ENABLE_CHIPOBLE": ble_commissioning,
        "CONFIG_BT_ENABLED": ble_commissioning,
        "CONFIG_BT_NIMBLE_ENABLED": ble_commissioning,
        # Shave RAM when BLE is on and PSRAM is not (best-effort attempt to
        # fit into 320KB internal). All values are conservative but functional
        # for one commissioner-to-device BLE session at a time.
        **(
            {
                # NimBLE defaults allow up to 4 concurrent BLE connections. Matter
                # commissioning uses exactly one; every unused slot costs ~4-8KB.
                "CONFIG_BT_NIMBLE_MAX_CONNECTIONS": 1,
                # NimBLE host task stack — 4KB default. On the classic ESP32
                # a shallower 3KB stack survived commissioning in an earlier
                # slimmer configuration, but on ESP32-S3 the CHIP
                # ↔ NimBLE bridge is deeper: the GATT service registration
                # and PASE-start callbacks all run inside the nimble_host task
                # and blow 3KB the moment a central connects, causing a
                # `vApplicationStackOverflowHook` panic right after
                # `BLE GAP connection established`. Give it a generous 5KB
                # everywhere — cheap on the S3's 512KB SRAM, and even on the
                # classic ESP32 it just costs 2KB of static task stack.
                "CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE": 5120,
                # Wi-Fi driver RX buffers stay at IDF defaults (10 static /
                # 32 dynamic). Earlier we shrank STATIC_RX to 4 to save RAM
                # on the classic ESP32, but on the ESP32-S3 with BLE
                # commissioning enabled that broke Matter's post-commissioning
                # Wi-Fi provisioning: WPA2
                # 4-way handshake sends four EAPOL frames back-to-back while
                # beacons + probe responses are still arriving, and four RX
                # slots overflow → EAPOL dropped → `Auth Expired`. The
                # `[wifi]: Coexist: Wi-Fi connect fail, apply reconnect coex
                # policy` line right before disconnect confirms it. Since the
                # S3 has 512KB SRAM the ~10KB we were saving is not worth
                # the failure mode, and even on the classic ESP32 with BLE
                # commissioning active we would rather spend the RAM than
                # burn a fabric per bad handshake.
                #
                # A-MPDU aggregation stays disabled — it saves ~4KB per Wi-Fi
                # station and only affects peak throughput (irrelevant for
                # Matter's low-rate command/attribute traffic), not the
                # handshake window.
                "CONFIG_ESP_WIFI_AMPDU_RX_ENABLED": False,
                "CONFIG_ESP_WIFI_AMPDU_TX_ENABLED": False,
                # NOTE on logging: the ESP-IDF-wide compile-time log ceiling
                # (CONFIG_LOG_MAXIMUM_LEVEL_*) and CHIP's own ceiling
                # (CONFIG_CHIP_LOG_DEFAULT_LEVEL_*) are deliberately NOT
                # touched here. Enabling ble_commissioning is a commissioning-
                # transport choice, not a debugging opt-in; raising those
                # ceilings would compile INFO/DEBUG strings across the entire
                # IDF tree (Wi-Fi, LwIP, SPI, NVS, mbedTLS) for every user of
                # this flag, adding flash and serial noise nobody asked for.
                # If BLE lifecycle traces are needed for debugging, set them
                # at runtime with esp_log_level_set("NimBLE", ESP_LOG_INFO)
                # (and the CHIP tag) or via esp32.framework.sdkconfig_options.
            }
            if ble_commissioning
            else {}
        ),
        # Use ESP-IDF mDNS (espressif/mdns 1.11.0) instead of CHIP's minimal
        # mDNS. ESPHome's network: component auto-loads its own mdns: which
        # pulls the exact same espressif/mdns dependency, so both stacks
        # share one mDNS instance and there is no port 5353 conflict.
        "CONFIG_USE_MINIMAL_MDNS": False,
        # esp-matter uses mbedtls_hkdf for session key derivation but ESP-IDF
        # leaves HKDF off by default to save flash. Turn it on or the linker
        # fails with "undefined reference to `mbedtls_hkdf`".
        "CONFIG_MBEDTLS_HKDF_C": True,
        # Disable Closure Control / Closure Dimension clusters. In esp-matter
        # 1.5.1 + GCC 14 (xtensa-esp-elf toolchain 14.2.0) their
        # implementation triggers a stdlib mismatch:
        #   Nullable<T> forwards operator== to std::optional<T>::operator==
        #   which requires T::operator==. GenericOverallCurrentState (used
        #   by ClosureControl) does not define it, so compilation aborts.
        # We do not expose any Closure device types from ESPHome, so turning
        # these off is free.
        "CONFIG_SUPPORT_CLOSURE_CONTROL_CLUSTER": False,
        "CONFIG_SUPPORT_CLOSURE_DIMENSION_CLUSTER": False,
        # CHIP resource caps sized for multi-fabric bridge devices. A large
        # bridge (Aggregator + ~72 Bridged Device Basic Info endpoints) is a
        # realistic worst case, and controllers like eWeLink Cube / Sonoff open
        # wildcard subscriptions against every one of them the moment they
        # commission. The Kconfig defaults (5 fabrics → 15 subscriptions →
        # 45 subscription path groups → 45 read path groups) were sized for a
        # single-endpoint device on one fabric; they exhaust the moment a
        # second aggressive hub commissions, at which point subscription
        # reports stop flowing, keep-alives drop, and controllers mark the
        # device offline even though CASE session is up. Apple Home masks this
        # because it opens ONE compact subscription per fabric with narrow
        # paths; eWeLink and Google Home wildcard-subscribe and blow the pool.
        #
        # MAX_FABRICS is the master knob because
        # src/lib/core/CHIPConfig.h derives:
        #   CHIP_IM_MAX_NUM_SUBSCRIPTIONS               = MAX_FABRICS * 3
        #   CHIP_IM_MAX_NUM_READS                       = MAX_FABRICS
        #   CHIP_IM_SERVER_MAX_NUM_PATH_GROUPS_FOR_SUBSCRIPTIONS = SUBSC * 3
        #   CHIP_IM_SERVER_MAX_NUM_PATH_GROUPS_FOR_READS         = READS * 9
        # PSRAM boards get 10 fabrics (Apple + Google + Alexa + Sonoff/eWeLink +
        # SmartThings + slack for unbind/rebind cycles) since the derived pools
        # spill to PSRAM via POOL_USE_HEAP below. No-PSRAM boards stay at 6
        # fabrics: the pools are static there and every extra fabric bakes
        # ~3 subscriptions × ~3 path groups × per-group state permanently into
        # internal DRAM, which a 320 KB ESP32-C6/H2 cannot spare.
        "CONFIG_MAX_FABRICS": 10 if config.get(_CONF_PSRAM_PRESENT, False) else 6,
        # Exchange contexts track in-flight CHIP conversations. Aggressive
        # multi-controller subscription set-up saturates the default of 8
        # during commissioning + first subscribe-storm. 32 buys ~4× headroom
        # at ~192B of pool memory (5-6KB total) — cheap on PSRAM-backed heap.
        "CONFIG_MAX_EXCHANGE_CONTEXTS": 32,
        # Unsolicited message handlers: one per protocol-server registration.
        # esp-matter registers a handler per interaction-model protocol +
        # secure channel; on a bridge the count grows with the number of
        # active servers. 8 → 16 gives room for future cluster additions
        # without another rebuild cycle.
        "CONFIG_MAX_UNSOLICITED_MESSAGE_HANDLERS": 16,
        # MRP (Message Reliable Protocol) timing knobs — critical for
        # aggressive multi-controller subscribers on high-endpoint bridges.
        # The default 300ms active retry × 4 retransmissions gives ~1.2s of
        # tolerance before we mark an exchange failed and the controller
        # marks endpoints offline. Under subscription-storm from eWeLink/
        # Google over 70+ endpoints the ESPHome main loop can be blocked
        # 1-3s at a time (GPIO polling, CHIP report encoding, log I/O), so
        # our own retransmit tempo blows the exchange before we get a chance
        # to ACK. Relaxing:
        #   ACTIVE 300 → 1500 ms  (we're mid-conversation)
        #   IDLE   500 → 2000 ms  (background subscriptions)
        #   BOOST    0 →  500 ms  (constant added; tolerates jitter)
        #   MAX_RETRANS 4 → 6     (two more attempts before giving up)
        # Trade-off: slightly slower failure detection on genuinely broken
        # links — irrelevant on wired Ethernet.
        "CONFIG_MRP_LOCAL_ACTIVE_RETRY_INTERVAL_FOR_WIFI_ETHERNET": 1500,
        "CONFIG_MRP_LOCAL_IDLE_RETRY_INTERVAL_FOR_WIFI_ETHERNET": 2000,
        "CONFIG_MRP_RETRY_INTERVAL_SENDER_BOOST_FOR_WIFI_ETHERNET": 500,
        "CONFIG_MRP_MAX_RETRANS": 6,
        # Matter TCP transport. Spec-optional (Matter 1.4+), but resolves a
        # class of "hub silently drops subscription reports" failures on
        # high-endpoint bridges. UDP is used by default for all Matter
        # messages; when a payload would exceed the IPv6 minimum MTU (1280B)
        # CHIP fragments at the IPv6 layer, and each fragment must reach the
        # peer intact — a single dropped fragment loses the whole datagram
        # and the peer never ACKs. With 73 bridged endpoints under one
        # aggregator, the initial subscription report from a wildcard
        # subscriber (eWeLink Cube, Google Home) is well past the MTU, so
        # any hop that gets iffy about IPv6 fragments (some cheap switches,
        # consumer hubs' own network stacks) will drop them and we see it
        # as "Time out! failed to receive status response from Exchange".
        # TCP transport lets CHIP negotiate large-payload delivery over a
        # reliable stream (SYN/ACK, retransmit at the segment level), which
        # is immune to the fragmentation-drop failure mode. Peers that
        # support TCP will use it for large messages; peers that don't fall
        # back to UDP transparently, so this is safe to enable regardless.
        #
        # Requires USE_LWIP_PBUF_RAM_PACKETBUFFER (heap-based pbufs). The
        # Kconfig `select` chain auto-enables it, but we set it explicitly
        # to survive ESPHome's sdkconfig injection ordering. Costs ~40KB
        # flash + a modest lwIP heap allocation. Existing yaml already
        # provisions LWIP_MAX_ACTIVE_TCP=8 / MAX_LISTENING_TCP=4, so no
        # additional TCP pool tuning required.
        "CONFIG_ENABLE_TCP_TRANSPORT": True,
        "CONFIG_USE_LWIP_PBUF_RAM_PACKETBUFFER": True,
        # CHIP platform event queue. Default 40 fills up when the fabric is
        # firing many subscription reports concurrently while GPIO callbacks
        # and mDNS events also queue events. Overflow drops events silently
        # and manifests as "Long dispatch time" warnings. 100 is a safe
        # ceiling for a 73-endpoint bridge under multi-fabric load.
        "CONFIG_MAX_EVENT_QUEUE_SIZE": 100,
        # POOL_USE_HEAP is gated on PSRAM presence below — see the block
        # after this dict. With static pools, CHIP hard-caps the number of
        # simultaneously-tracked subscription/read/attribute paths at
        # (MAX_FABRICS-derived) compile-time constants — even with MAX_FABRICS
        # at 10 that is only 90 subscription path groups, which a single
        # wildcard subscriber over 72 bridged endpoints × ~4 attributes each
        # expands well past. POOL_USE_HEAP makes every CHIP ObjectPool
        # allocate through the standard allocator instead. Only enabled when
        # PSRAM is present because on a no-PSRAM board the static pools are
        # the last line of defense against a remote-driven OOM — a wildcard-
        # subscribing controller can otherwise drive allocation until the
        # 320KB internal DRAM is exhausted and the device aborts.
        # Custom DeviceInfoProvider so we can serve per-endpoint FixedLabel
        # entries (endpoint name shown by controllers) without touching the
        # factory data path. Turning on ENABLE_ESP32_FACTORY_DATA_PROVIDER
        # (Kconfig gate for the ESP32 DeviceInfoProvider) would flip the
        # CommissionableDataProvider and DAC provider defaults over to
        # factory-partition-backed implementations that expect DAC/PAI/CD +
        # spake2p verifier in NVS — we do not populate those, and dev-mode
        # commissioning breaks. CUSTOM_DEVICE_INFO_PROVIDER is independent:
        # esp_matter_providers.cpp just calls SetDeviceInfoProvider with
        # whatever pointer we hand to set_custom_device_info_provider(), and
        # the EXAMPLE Commissionable/DAC providers stay in place.
        "CONFIG_CUSTOM_DEVICE_INFO_PROVIDER": True,
        # Same story for the DeviceInstanceInfoProvider — the generic
        # implementation returns compile-time constants ("TEST_VENDOR" /
        # "TEST_PRODUCT" from CHIPDeviceConfig.h) regardless of what we
        # write to chip-factory NVS. Custom provider lets us serve the
        # YAML-configured vendor_name/product_name without pulling in
        # ENABLE_ESP32_FACTORY_DATA_PROVIDER.
        "CONFIG_CUSTOM_DEVICE_INSTANCE_INFO_PROVIDER": True,
    }

    # Sized in FINAL_VALIDATE from the count of Matter-eligible entities in the
    # merged config. Skipped when the user pinned the value themselves in
    # esp32.framework.sdkconfig_options — their explicit choice wins.
    if not config.get(_CONF_MAX_DYNAMIC_ENDPOINT_COUNT_PINNED, False):
        sdkconfig["CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT"] = config[
            _CONF_MAX_DYNAMIC_ENDPOINT_COUNT
        ]

    # POOL_USE_HEAP: only enable when PSRAM is present. See the comment on
    # the sdkconfig dict above — on a no-PSRAM board the static ObjectPools
    # are the only bound on remote-driven allocation, and turning them off
    # trades a bounded, well-understood failure (subscription refused) for
    # an unbounded one (heap exhaustion → abort). Users who explicitly want
    # heap pools on a no-PSRAM board can still set the option themselves via
    # esp32.framework.sdkconfig_options.
    if config.get(_CONF_PSRAM_PRESENT, False):
        sdkconfig["CONFIG_CHIP_SYSTEM_CONFIG_POOL_USE_HEAP"] = True

    # External Platform: replace esp-matter's "else()" branch (which folds
    # $MATTER_SDK_PATH/src/platform/ESP32 into the build with hard-coded
    # includes) with our own manifest. Our external_platform.cmake mirrors
    # the same conditionals Espressif ships in
    # examples/common/blemesh_platform/platform/ESP32_custom/, plus one
    # unconditional exclusion of NetworkCommissioningDriver_Ethernet.cpp
    # (its IP101-hardcoded Init fights with ESPHome's ethernet: component;
    # the replacement lives in matter_ethernet_stub.cpp). Path is absolute
    # so the resolver in esp_matter's CMakeLists (which falls back to
    # ${CMAKE_SOURCE_DIR}/... for relative paths) always finds it,
    # regardless of the .esphome build layout.
    external_platform_dir = (Path(__file__).parent / "external_platform").resolve()
    sdkconfig["CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM"] = True
    sdkconfig["CONFIG_CHIP_EXTERNAL_PLATFORM_DIR"] = str(external_platform_dir)

    for key, value in sdkconfig.items():
        add_idf_sdkconfig_option(key, value)

    # EXTERNAL_*_HEADER macros are required by esp_matter core sources when
    # CHIP_ENABLE_EXTERNAL_PLATFORM=y — each names the header that replaces
    # a default ``<platform/ESP32/XXX.h>`` include. We are NOT renaming the
    # platform layer (unlike blemesh_platform's ESP32_custom mirror), so
    # every value points back at the standard CHIP path.
    #
    # Use the quoted form ("..." instead of <...>) because PlatformIO
    # writes build_flags to platformio.ini and re-splits them through the
    # shell before ninja sees them; ``<...>`` there is interpreted as
    # input redirection and the compile aborts with "No such file or
    # directory". Escaped double-quotes survive PIO's parsing and CHIP's
    # ``#include EXTERNAL_..._HEADER`` accepts either delimiter (K&R
    # quoted vs. angle-bracket).
    _EXTERNAL_HEADER_MAP = {
        "EXTERNAL_ESP32UTILS_HEADER": "platform/ESP32/ESP32Utils.h",
        "EXTERNAL_ESP32OTAIMAGEPROCESSORIMPL_HEADER": (
            "platform/ESP32/OTAImageProcessorImpl.h"
        ),
        "EXTERNAL_ESP32DEVICEINFOPROVIDER_HEADER": (
            "platform/ESP32/ESP32DeviceInfoProvider.h"
        ),
        "EXTERNAL_ESP32FACTORYDATAPROVIDER_HEADER": (
            "platform/ESP32/ESP32FactoryDataProvider.h"
        ),
        "EXTERNAL_ESP32SECURECERTDACPROVIDER_HEADER": (
            "platform/ESP32/ESP32SecureCertDACProvider.h"
        ),
        "EXTERNAL_ESP32SECURECERTDATAPROVIDER_HEADER": (
            "platform/ESP32/ESP32SecureCertDataProvider.h"
        ),
        # Platform-config includes that the *BuildConfig.h headers would set
        # for us if we were NOT in external-platform mode. See
        # external_platform/external_platform.cmake for the rationale — the
        # generated headers guard these on ``#ifndef
        # CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM``, so we compensate here so
        # the ESP32 platform is picked up regardless of the guard.
        "CHIP_PLATFORM_CONFIG_INCLUDE": "platform/ESP32/CHIPPlatformConfig.h",
        "SYSTEM_PLATFORM_CONFIG_INCLUDE": "platform/ESP32/SystemPlatformConfig.h",
        "CHIP_DEVICE_PLATFORM_CONFIG_INCLUDE": (
            "platform/ESP32/CHIPDevicePlatformConfig.h"
        ),
        "BLE_PLATFORM_CONFIG_INCLUDE": "platform/ESP32/BlePlatformConfig.h",
        "INET_PLATFORM_CONFIG_INCLUDE": "platform/ESP32/InetPlatformConfig.h",
    }
    for _macro, _header in _EXTERNAL_HEADER_MAP.items():
        cg.add_build_flag(f'-D{_macro}=\\"{_header}\\"')
    # CHIP_DEVICE_LAYER_TARGET is an identifier (not a header path); pass as
    # a plain -D.
    cg.add_build_flag("-DCHIP_DEVICE_LAYER_TARGET=ESP32")

    cg.add_define("USE_MATTER")
    # Companion symbol referenced in the compile-time guard at the top of
    # every matter_*.{h,cpp} file. Injected here (not in esphome/core/
    # defines.h) so clang-tidy / static-analysis passes — which read
    # defines.h and do NOT run codegen — see it undefined and strip the
    # matter TU entirely. The esp_matter.h header is a third-party managed
    # component fetched only at real-build time, so linting matter code
    # against it isn't feasible in the ESPHome CI environment. Real builds
    # get -D USE_MATTER_VARIANT_SUPPORTED and compile normally. Runtime
    # variant enforcement is upstream of this — the only_on_variant
    # validator on CONFIG_SCHEMA already rejects unsupported chips.
    cg.add_define("USE_MATTER_VARIANT_SUPPORTED")
    cg.add_build_flag("-DCHIP_HAVE_CONFIG_H")

    CORE.add_job(_write_cmake_project_include)

    if config.get(_CONF_PSRAM_PRESENT, False):
        # Overriding SPIRAM_USE_* has to happen AFTER ESPHome's psram: to_code
        # writes SPIRAM_USE_CAPS_ALLOC=y — otherwise psram overwrites us. Punt
        # to a FINAL coroutine so we always win.
        #
        # Applied unconditionally when PSRAM is present, not just BLE. On
        # many-endpoint Matter builds (74 entities + Aggregator + Bridged
        # basic info per endpoint) the CHIP data-model provider allocations
        # exhaust internal DRAM at boot, starving the W5500 SPI DMA
        # priv-RX-buffer allocation and either aborting on ``operator new``
        # (bad_alloc → __cxa_allocate_exception on ESP-IDF without C++
        # exceptions) or crashing the periodic W5500 RX polling. USE_MALLOC
        # lets those allocations spill to PSRAM freely; DMA-capable allocs
        # (spi_master, wifi RX) still go to internal via MALLOC_CAP_DMA.
        if ble_commissioning:
            CORE.data.setdefault("matter", {})["ble"] = True
        CORE.data.setdefault("matter", {})["psram"] = True
        CORE.add_job(_apply_matter_psram_overrides)


@coroutine_with_priority(CoroPriority.FINAL - 1)
async def _write_cmake_project_include() -> None:
    """Append EXECUTABLE_COMPONENT_NAME to cmake_extra_args.

    esp_matter's own CMakeLists.txt (managed_components/…/CMakeLists.txt:
    ~688) calls ``idf_component_get_property(main_lib ${EXECUTABLE_COMPONENT_NAME}
    COMPONENT_LIB)`` and defaults ``EXECUTABLE_COMPONENT_NAME`` to ``main`` when
    unset. ESPHome names the app component ``src``, so we must define this
    BEFORE esp_matter's CMakeLists.txt runs, otherwise configure fails with
    ``Failed to resolve component 'main'``.

    Two channels set it:
      - ``build_gen/espidf.py`` writes ``set(EXECUTABLE_COMPONENT_NAME src)``
        at project scope (single source of truth for the dev checkout / CI).
      - This coroutine also passes ``-DEXECUTABLE_COMPONENT_NAME=src`` via
        ``cmake_extra_args`` so downstream builds against SHIPPED ESPHome
        (whose espidf.py template predates the ``set()`` line) still work.
        Same value → no conflict when both paths fire.

    Priority FINAL - 1 runs after esp32's FINAL coroutine (which sets
    EXCLUDE_COMPONENTS on the same option) so our appended value wins
    the last-write-wins race; add_platformio_option on a str key
    OVERWRITES, so we read-modify-write.
    """
    existing = CORE.platformio_options.get("board_build.cmake_extra_args", "")
    executable_name_arg = "-DEXECUTABLE_COMPONENT_NAME=src"
    if not isinstance(existing, str):
        raise EsphomeError(
            "matter: board_build.cmake_extra_args has an unexpected shape "
            f"({type(existing).__name__}); cannot append the Matter CMake "
            f"flags safely — file an issue with this configuration."
        )
    if executable_name_arg not in existing:
        cg.add_platformio_option(
            "board_build.cmake_extra_args",
            f"{existing} {executable_name_arg}".strip(),
        )


@coroutine_with_priority(CoroPriority.FINAL - 2)
async def _apply_matter_psram_overrides() -> None:
    """Force SPIRAM_USE_MALLOC so operator new spills into PSRAM.

    ESPHome's psram: to_code sets CONFIG_SPIRAM_USE_CAPS_ALLOC=y, which
    restricts PSRAM to allocations made via heap_caps_malloc(..., MALLOC_CAP_SPIRAM).
    The CHIP stack (and libstdc++ containers everywhere in the descriptor /
    IM engine / fabric storage code) uses plain new/malloc — those never
    touch PSRAM under CAPS_ALLOC and instead OOM in the internal DRAM. On the
    ESP32-S3 that manifested as `spi_master: Failed to allocate priv RX
    buffer` from the W5500 link-check timer — CHIP had eaten so much internal
    DRAM (74 endpoints × ~5 clusters each, plus fabric/IM/descriptor state)
    that the SPI driver's DMA-capable allocation for a few hundred bytes
    failed. USE_MALLOC lets CHIP spill freely to PSRAM; MALLOC_CAP_DMA
    allocations (SPI driver, Wi-Fi RX buffers) still target internal DRAM.

    SPIRAM_USE_MALLOC changes the mode so the general allocator falls back
    to PSRAM once internal is exhausted. Small allocs still prefer internal
    RAM for latency (CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL threshold).

    Priority FINAL - 2 runs AFTER esp32's FINAL and _write_cmake_project_include
    (FINAL - 1) but crucially AFTER psram: to_code (default component priority),
    so our overwrites in the sdkconfig dict win the dict-insertion-order race.
    """
    if not CORE.data.get("matter", {}).get("psram"):
        return
    # SPIRAM_USE_* is a Kconfig `choice` — only one entry may be y. Setting
    # USE_MALLOC=True and USE_CAPS_ALLOC=False both make Kconfig pick MALLOC
    # (choice resolves by last-selected wins).
    add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", False)
    add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_MALLOC", True)
    # 0 means "no allocation is FORCED to internal RAM based on size". The
    # allocator still prefers internal first (faster, no MSPI hop) but spills
    # to PSRAM freely when internal is exhausted. Any non-zero threshold
    # bricks Matter's tiny hashmap-node allocs (~24-48 bytes) — they get
    # forced into an internal RAM that is already saturated by NimBLE + Wi-Fi
    # buffers, and OOM.
    add_idf_sdkconfig_option("CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL", 0)
    # Move NimBLE's own allocator to PSRAM — but only on the classic ESP32 AND
    # only when the BLE commissioning path is active. Rationale: on the classic
    # ESP32 the BT controller has its own dedicated memory and the NimBLE host
    # can safely live in PSRAM (HCI buffers get copied at the controller
    # boundary). On ESP32-S3/C3/C6 the controller and host share HCI buffers
    # directly, and the controller ISRs run with the PSRAM cache disabled — an
    # HCI buffer in PSRAM would be unreadable from the ISR, so
    # `esp_nimble_init()` fails with "nimble host init failed" during setup.
    # The S3 has 512KB of internal SRAM (vs. 320KB on classic), so NimBLE fits
    # internal without pushing us into OOM.
    if CORE.data.get("matter", {}).get("ble") and get_esp32_variant() == VARIANT_ESP32:
        add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_INTERNAL", False)
        add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL", True)

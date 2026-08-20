from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, spi
from esphome.components.esp32 import only_on_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
from esphome.components.storage import (
    CONF_MOUNT_PATH,
    FILE_SYSTEM_SCHEMA_ENTRY,
    MountableStorage,
    file_system_to_code,
    final_validate_file_system,
    register_mount_path,
    request_fatfs_path_length,
    request_storage_device,
    request_storage_worker,
    validate_file_system_value,
    validate_mount_path,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_ID,
    CONF_SPI,
    CONF_TRIGGER_ID,
    CONF_TYPE,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@p1ngb4ck"]

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["storage"]

sd_storage_ns = cg.esphome_ns.namespace("sd_storage")
# MountableStorage parent makes SD ids valid targets for the generic storage.mount /
# storage.unmount actions (cv.use_id(MountableStorage) checks declared Python parents).
SdStorageBase = sd_storage_ns.class_("SdStorageBase", cg.Component, MountableStorage)
SdMmc = sd_storage_ns.class_("SdMmc", SdStorageBase)
SdSpi = sd_storage_ns.class_("SdSpi", spi.SPIDevice, SdStorageBase)

CardMountedTrigger = sd_storage_ns.class_(
    "CardMountedTrigger", automation.Trigger.template(cg.const_char_ptr)
)
CardRemovedTrigger = sd_storage_ns.class_(
    "CardRemovedTrigger", automation.Trigger.template()
)
CardInsertedTrigger = sd_storage_ns.class_(
    "CardInsertedTrigger", automation.Trigger.template()
)
MountCardAction = sd_storage_ns.class_("MountCardAction", automation.Action)
UnmountCardAction = sd_storage_ns.class_("UnmountCardAction", automation.Action)
ListFilesAction = sd_storage_ns.class_("ListFilesAction", automation.Action)
CardMountedCondition = sd_storage_ns.class_(
    "CardMountedCondition", automation.Condition
)

CONF_CMD_PIN = "cmd_pin"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_MODE_1BIT = "mode_1bit"
CONF_SLOT = "slot"
CONF_ON_MOUNTED = "on_mounted"
CONF_ON_REMOVED = "on_removed"
CONF_ON_INSERTED = "on_inserted"
CONF_CD_PIN = "cd_pin"
CONF_FORMAT_ON_MISMATCH = "format_on_mismatch"
CONF_SPI_INTERFACE = "spi_interface"
CONF_ASSUME_EXCLUSIVE_BUS = "assume_exclusive_bus"

TYPE_SD_MMC = "sd_mmc"
TYPE_SD_SPI = "sd_spi"


def validate_spi_cs_config(config):
    data3_pin_config = config.get(CONF_DATA3_PIN)
    cs_pin_config = config.get(CONF_CS_PIN)
    if data3_pin_config and cs_pin_config:
        raise cv.Invalid(
            f"{CONF_DATA3_PIN} is the same as {CONF_CS_PIN}. Please remove one."
        )
    if not data3_pin_config and not cs_pin_config:
        raise cv.Invalid(
            f"{CONF_DATA3_PIN} or {CONF_CS_PIN} required. Please specify one."
        )
    if data3_pin_config:
        config[CONF_CS_PIN] = data3_pin_config
        del config[CONF_DATA3_PIN]
    return config


def validate_spi_bus_pins(config):
    cmd_pin = config.get(CONF_CMD_PIN)
    data0_pin = config.get(CONF_DATA0_PIN)
    clk_pin = config.get(CONF_CLK_PIN)
    if cmd_pin or data0_pin or clk_pin:
        raise cv.Invalid(
            f"Please move pins to SPI bus definition:\n"
            f" '{CONF_CMD_PIN}' to '{spi.CONF_MOSI_PIN}'\n"
            f" '{CONF_DATA0_PIN}' to '{spi.CONF_MISO_PIN}'\n"
            f" '{CONF_CLK_PIN}' to '{spi.CONF_CLK_PIN}'"
        )
    return config


def validate_spi_mode(config):
    if CORE.using_arduino:
        raise cv.Invalid("Only esp-idf supported for SD SPI")
    if config[CONF_MODE_1BIT] is False:
        raise cv.Invalid("Only 1bit mode supported for SPI")
    return config


SDMMC_VARIANTS = [VARIANT_ESP32S3, VARIANT_ESP32P4, VARIANT_ESP32S31]


def validate_platform_variant(config):
    if config.get(CONF_TYPE) == TYPE_SD_MMC:
        only_on_variant(
            supported=SDMMC_VARIANTS,
            msg_prefix="SD MMC mode",
        )(config)
    return config


SD_MMC_SCHEMA = cv.Schema(
    {
        # Only exists together with esp32 enable_exfat -- see storage/__init__.py.
        FILE_SYSTEM_SCHEMA_ENTRY: validate_file_system_value,
        cv.GenerateID(): cv.declare_id(SdMmc),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
        cv.Optional(CONF_SLOT, default=0): cv.int_range(min=0, max=1),
        # Default is platform dependent and applied in to_code(): SDMMC_FREQ_HIGHSPEED
        # (40 MHz) on ESP32-P4, SDMMC_FREQ_DEFAULT (20 MHz) elsewhere.
        cv.Optional(CONF_DATA_RATE): cv.All(
            cv.frequency, cv.Range(min=400e3, max=40e6)
        ),
        cv.Optional(CONF_MOUNT_PATH, default="/sdcard"): validate_mount_path,
        cv.Optional(CONF_CD_PIN): pins.gpio_input_pullup_pin_schema,
        cv.Optional(CONF_FORMAT_ON_MISMATCH, default=False): cv.boolean,
        cv.Optional(CONF_ON_MOUNTED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardMountedTrigger)}
        ),
        cv.Optional(CONF_ON_REMOVED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardRemovedTrigger)}
        ),
        cv.Optional(CONF_ON_INSERTED): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardInsertedTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

SD_SPI_SCHEMA = (
    cv.Schema(
        {
            # Only exists together with esp32 enable_exfat -- see storage/__init__.py.
            FILE_SYSTEM_SCHEMA_ENTRY: validate_file_system_value,
            cv.GenerateID(): cv.declare_id(SdSpi),
            cv.Optional(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA0_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA3_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DATA1_PIN): pins.gpio_input_pullup_pin_schema,
            cv.Optional(CONF_DATA2_PIN): pins.gpio_input_pullup_pin_schema,
            cv.Optional(CONF_MODE_1BIT, default=True): cv.boolean,
            cv.Optional(CONF_SLOT, default=0): cv.int_range(min=0, max=1),
            cv.Optional(CONF_MOUNT_PATH, default="/sdcard"): validate_mount_path,
            cv.Optional(CONF_CD_PIN): pins.gpio_input_pullup_pin_schema,
            cv.Optional(CONF_FORMAT_ON_MISMATCH, default=False): cv.boolean,
            # Opt in to task-safe I/O when this card is the only device on its SPI bus. Enforced
            # in FINAL_VALIDATE (Check A: alone on the bus). Off by default; esp32-only because
            # the background worker task that would use it exists only there.
            cv.Optional(CONF_ASSUME_EXCLUSIVE_BUS, default=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_ON_MOUNTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardMountedTrigger)}
            ),
            cv.Optional(CONF_ON_REMOVED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardRemovedTrigger)}
            ),
            cv.Optional(CONF_ON_INSERTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CardInsertedTrigger)}
            ),
        },
        extra_schemas=[
            validate_spi_cs_config,
            validate_spi_bus_pins,
            validate_spi_mode,
        ],
    )
    .extend(spi.spi_device_schema(cs_pin_required=False))
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            TYPE_SD_MMC: SD_MMC_SCHEMA,
            TYPE_SD_SPI: SD_SPI_SCHEMA,
        },
        default_type=TYPE_SD_MMC,
    ),
    validate_platform_variant,
)


def _final_validate_spi_interface(config):
    if config[CONF_TYPE] != TYPE_SD_SPI:
        return

    spi_id = config.get(spi.CONF_SPI_ID)
    if spi_configs := fv.full_config.get().get(CONF_SPI):
        if len(spi_configs) > 1 and spi_id is None:
            raise cv.Invalid(
                "Multiple SPI buses defined. Please specify which one to use with 'spi_id'"
            )

        if spi_id is None:
            spi_conf = spi_configs[0]
        else:
            spi_conf = None
            for conf in spi_configs:
                if conf[spi.CONF_ID] == spi_id:
                    spi_conf = conf
                    break
            if spi_conf is None:
                raise cv.Invalid(f"SPI bus '{spi_id}' not found")

        index = spi_conf.get(spi.CONF_INTERFACE_INDEX)
        if index is None:
            raise cv.Invalid(
                f"SD card requires hardware SPI interface. "
                f"The spi bus '{spi_conf[spi.CONF_ID]}' is configured as software SPI. "
                f"Please use hardware SPI pins or specify 'interface: hardware' in your spi: config."
            )

        config[CONF_SPI_INTERFACE] = spi.get_spi_interface(index)


def _count_devices_on_spi_bus(fconf, bus_id):
    """Count component configs across the whole config that reference the same spi bus id.

    Walks every domain's component list and looks for the spi_id key. The bus is only READ
    here -- never modified. Returns (count, names) where names lists the other devices' ids for
    a helpful error message.
    """
    count = 0
    names = []
    root = fconf.get_config_for_path([])
    for domain_conf in root.values():
        entries = domain_conf if isinstance(domain_conf, list) else [domain_conf]
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            ref = entry.get(spi.CONF_SPI_ID)
            if ref is not None and str(ref) == str(bus_id):
                count += 1
                if (other_id := entry.get(CONF_ID)) is not None:
                    names.append(str(other_id))
    return count, names


def _final_validate_assume_exclusive_bus(config):
    """Enforce the assume_exclusive_bus promise for SD-over-SPI.

    The user asserts this card is alone on its SPI bus. We do not believe it blindly -- Check A:
    no other device may reference the same bus. (SD-SPI is always a hardware bus here, enforced
    separately by _final_validate_spi_interface, so the hardware-bus check is already covered.)
    The bus config is only inspected, never changed.
    """
    if config[CONF_TYPE] != TYPE_SD_SPI:
        return
    if not config.get(CONF_ASSUME_EXCLUSIVE_BUS):
        return

    bus_id = config.get(spi.CONF_SPI_ID)
    fconf = fv.full_config.get()

    # --- Check A: this card must be alone on its bus ---
    if bus_id is not None:
        count, others = _count_devices_on_spi_bus(fconf, bus_id)
        if count > 1:
            shared_with = (
                ", ".join(n for n in others if n != str(config.get(CONF_ID)))
                or f"{count - 1} other device(s)"
            )
            raise cv.Invalid(
                f"'{CONF_ASSUME_EXCLUSIVE_BUS}: true' requires this SD card to be the only "
                f"device on bus '{bus_id}', but it is shared with: {shared_with}. A background "
                f"task driving a shared bus would corrupt the other devices' traffic. Give the "
                f"card its own SPI bus, or remove '{CONF_ASSUME_EXCLUSIVE_BUS}'."
            )


def _final_validate(config):
    _final_validate_spi_interface(config)
    _final_validate_assume_exclusive_bus(config)
    final_validate_file_system(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    esp32.require_vfs_dir()
    esp32.require_vfs_select()
    # LFN mode/length and volume count are set as user-overridable defaults by the esp32
    # platform once any component calls require_fatfs() -- see
    # _reconcile_vfs_fatfs_sdkconfig(). Override via esp32 framework sdkconfig_options.
    esp32.require_fatfs()
    esp32.include_builtin_idf_component("fatfs")
    esp32.include_builtin_idf_component("esp_vfs_fat")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await file_system_to_code(var, config)

    card_type = config[CONF_TYPE]
    if card_type == TYPE_SD_SPI:
        cg.add_define("USE_SD_STORAGE_SPI")

        await spi.register_spi_device(var, config)

        if (mode_1bit := config.get(CONF_MODE_1BIT)) is not None:
            cg.add(var.set_mode_1bit(mode_1bit))

        if spi_interface := config.get(CONF_SPI_INTERFACE):
            cg.add(var.set_spi_interface(cg.RawExpression(spi_interface)))

        if pin := config.get(CONF_DATA1_PIN):
            cg.add(var.set_data1_pin(await cg.gpio_pin_expression(pin)))
        if pin := config.get(CONF_DATA2_PIN):
            cg.add(var.set_data2_pin(await cg.gpio_pin_expression(pin)))

    elif card_type == TYPE_SD_MMC:
        cg.add_define("USE_SD_STORAGE_SDMMC")

        mode_1bit = config.get(CONF_MODE_1BIT, False)
        cg.add(var.set_mode_1bit(mode_1bit))

        # The P4 host runs the bus at high speed; every other variant stays at the
        # SD default speed. An explicit data_rate wins over both.
        if (data_rate := config.get(CONF_DATA_RATE)) is not None:
            data_rate_khz = int(data_rate / 1000)
        elif esp32.get_esp32_variant() == VARIANT_ESP32P4:
            data_rate_khz = 40000
        else:
            data_rate_khz = 20000
        cg.add(var.set_data_rate_khz(data_rate_khz))

        if CONF_SLOT in config:
            cg.add(var.set_slot(config[CONF_SLOT]))

        cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
        cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
        cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))

        if not mode_1bit:
            if CONF_DATA1_PIN in config:
                cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
            if CONF_DATA2_PIN in config:
                cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
            if CONF_DATA3_PIN in config:
                cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))

    cg.add(var.set_mount_path(config[CONF_MOUNT_PATH]))
    # Full VFS paths carry the mount point; the storage component sizes its buffers from the
    # paths registered here. Shared by both card types: SdMmc and SdSpi differ in pins only.
    register_mount_path(config[CONF_MOUNT_PATH])
    cg.add(var.set_id(str(config[CONF_ID])))

    if cd_pin := config.get(CONF_CD_PIN):
        cg.add(var.set_cd_pin(await cg.gpio_pin_expression(cd_pin)))
    cg.add(var.set_format_on_mismatch(config[CONF_FORMAT_ON_MISMATCH]))

    request_storage_device()
    # Bounded by FATFS long filenames, i.e. by CONFIG_FATFS_MAX_LFN -- resolved at codegen
    # time rather than baked in, so a user who lowers it gets a matching API bound.
    request_fatfs_path_length()
    # SdMmc has a dedicated SDIO controller and is always safe to drive from the worker's
    # background task. SdSpi shares a general SPI bus, so it is task-safe only when the user has
    # opted in with assume_exclusive_bus (enforced in FINAL_VALIDATE: alone on the bus).
    sd_spi_exclusive = card_type == TYPE_SD_SPI and config.get(
        CONF_ASSUME_EXCLUSIVE_BUS
    )
    if sd_spi_exclusive:
        cg.add(var.set_assume_exclusive_bus(True))
    request_storage_worker(task_safe=(card_type == TYPE_SD_MMC or sd_spi_exclusive))

    for conf in config.get(CONF_ON_MOUNTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.const_char_ptr, "mount_path")], conf
        )

    for conf in config.get(CONF_ON_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_INSERTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


SD_STORAGE_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.Required(CONF_ID): cv.use_id(SdStorageBase)}
)


@automation.register_action(
    "sd_storage.mount", MountCardAction, SD_STORAGE_ACTION_SCHEMA, synchronous=True
)
async def sd_storage_mount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sd_storage.unmount", UnmountCardAction, SD_STORAGE_ACTION_SCHEMA, synchronous=True
)
async def sd_storage_unmount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


LIST_FILES_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SdStorageBase),
        cv.Optional("path", default=""): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "sd_storage.list_files", ListFilesAction, LIST_FILES_SCHEMA, synchronous=True
)
async def sd_storage_list_files_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if "path" in config:
        template_ = await cg.templatable(config["path"], args, cg.const_char_ptr)
        cg.add(var.set_path(template_))
    return var


@automation.register_condition(
    "sd_storage.is_mounted", CardMountedCondition, SD_STORAGE_ACTION_SCHEMA
)
async def sd_storage_is_mounted_to_code(config, condition_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, parent)

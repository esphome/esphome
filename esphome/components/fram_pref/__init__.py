import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fram
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_KEY, CONF_SIZE

CODEOWNERS = ["@sharkydog"]
DEPENDENCIES = ["fram"]
CONF_FRAM_ID = "fram_id"
CONF_POOL_SIZE = "pool_size"
CONF_POOL_START = "pool_start"
CONF_STATIC_PREFS = "static_prefs"
CONF_ADDR = "addr"
CONF_PERSIST_KEY = "persist_key"

fram_pref_ns = cg.esphome_ns.namespace("fram_pref")
FramPrefComponent = fram_pref_ns.class_(
    "FramPref", cg.Component, cg.esphome_ns.class_("ESPPreferences")
)


def validate_pref_range(conf_pref):
    f = validate_pref_range

    if conf_pref[CONF_KEY] in f.keys:
        raise cv.Invalid(f'key "{conf_pref[CONF_KEY]}" already used')

    if len(conf_pref.keys() & {CONF_ADDR, CONF_SIZE}) == 1:
        raise cv.Invalid(f'Add both "{CONF_ADDR}" and "{CONF_SIZE}" or none')

    if CONF_ADDR not in conf_pref:
        return conf_pref

    if conf_pref[CONF_ADDR] < f.range_min:
        raise cv.Invalid(
            f"Preference overlaps with previous (end address {f.range_min-1})"
        )

    f.range_min = conf_pref[CONF_ADDR] + conf_pref[CONF_SIZE]
    f.keys.add(conf_pref[CONF_KEY])
    conf_pref["_pref_addr"] = f"{conf_pref[CONF_ADDR]} - {f.range_min-1}"

    return conf_pref


validate_pref_range.range_min = 0
validate_pref_range.keys = set()


def final_validate(config):
    config = CONFIG_SCHEMA_(config)

    if CONF_POOL_SIZE not in config and CONF_STATIC_PREFS not in config:
        raise cv.Invalid(
            f'Add either "{CONF_POOL_SIZE}" or "{CONF_STATIC_PREFS}" or both'
        )

    if CONF_POOL_SIZE not in config and CONF_POOL_START in config:
        raise cv.Invalid(f'Either remove "{CONF_POOL_START}" or set "{CONF_POOL_SIZE}"')

    if CONF_POOL_SIZE in config:
        pool_start = config[CONF_POOL_START] if CONF_POOL_START in config else 0
        pool_end = pool_start + config[CONF_POOL_SIZE] - 1
        config["_pool_addr"] = f"{pool_start} - {pool_end}"

        if CONF_STATIC_PREFS in config:
            errors = []

            for idx, conf_pref in enumerate(config[CONF_STATIC_PREFS]):
                if CONF_ADDR not in conf_pref:
                    continue

                addr_start = conf_pref[CONF_ADDR]
                addr_end = conf_pref[CONF_ADDR] + conf_pref[CONF_SIZE] - 1

                if addr_start <= pool_end and addr_end >= pool_start:
                    errors.append(
                        cv.Invalid(
                            f"{CONF_STATIC_PREFS}[{idx}] address range ({addr_start} - {addr_end}) overlaps with pool ({pool_start} - {pool_end})"
                        )
                    )

            if errors:
                raise cv.MultipleInvalid(errors)

    return config


CONFIG_SCHEMA_ = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FramPrefComponent),
        cv.GenerateID(CONF_FRAM_ID): cv.use_id(fram.FramComponent),
        cv.Optional(CONF_POOL_SIZE): cv.All(
            fram.validate_bytes_1024, cv.int_range(min=7)
        ),
        cv.Optional(CONF_POOL_START): cv.int_range(min=0),
        cv.Optional(CONF_STATIC_PREFS): cv.ensure_list(
            {
                cv.Required(CONF_KEY): cv.string_strict,
                cv.Required(CONF_LAMBDA): cv.returning_lambda,
                cv.Optional(CONF_ADDR): cv.int_range(min=0),
                cv.Optional(CONF_SIZE): cv.All(
                    fram.validate_bytes_1024, cv.int_range(min=3)
                ),
                cv.Optional(CONF_PERSIST_KEY, default=False): cv.boolean,
            },
            validate_pref_range,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = final_validate


async def to_code(config):
    framComponent = await cg.get_variable(config[CONF_FRAM_ID])
    pool_size = config[CONF_POOL_SIZE] if CONF_POOL_SIZE in config else 0
    pool_start = config[CONF_POOL_START] if CONF_POOL_START in config else 0

    var = cg.new_Pvariable(config[CONF_ID], framComponent)
    await cg.register_component(var, config)

    if pool_size:
        cg.add(var.set_pool(pool_size, pool_start))

    for conf_pref in config.get(CONF_STATIC_PREFS, []):
        lambda_ = await cg.process_lambda(
            conf_pref[CONF_LAMBDA], [], return_type=cg.uint32
        )

        addr = conf_pref[CONF_ADDR] if CONF_ADDR in conf_pref else 0
        size = conf_pref[CONF_SIZE] if CONF_SIZE in conf_pref else 0

        cg.add(
            var.set_static_pref(
                conf_pref[CONF_KEY], addr, size, lambda_, conf_pref[CONF_PERSIST_KEY]
            )
        )

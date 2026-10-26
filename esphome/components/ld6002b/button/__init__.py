from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import button
from esphome.components.ld600x import ld600x_ns
from esphome.components.select import DOMAIN as SELECT_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA_ID,
    CONF_ID,
    CONF_WAKEUP_PIN,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.cpp_generator import ParameterListExpression
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import LD6002BComponent
from ..const import (
    CONF_APPLY_AREA,
    CONF_AUTO_INTERFERENCE,
    CONF_CLEAR_INTERFERENCE,
    CONF_GET_AREAS,
    CONF_GET_DELAY,
    CONF_GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME,
    CONF_GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE,
    CONF_LD6002B_ID,
    CONF_RESET_DETECTION_AREA,
    CONF_RESET_UNATTENDED,
    CONF_WAKE,
)

DEPENDENCIES = ["ld6002b"]

ButtonType = ld600x_ns.enum("ButtonType")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_APPLY_AREA): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_AUTO_INTERFERENCE): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_GET_AREAS): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_CLEAR_INTERFERENCE): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_RESET_DETECTION_AREA): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_GET_DELAY): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_SENSITIVITY): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_TRIGGER_SPEED): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_Z_RANGE): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_INSTALLATION): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_LOW_POWER_MODE): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_GET_LOW_POWER_SLEEP_TIME): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_RESET_UNATTENDED): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_WAKE): button.button_schema(
            button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    }
)


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    hub_id = config[CONF_LD6002B_ID]

    if config.get(CONF_APPLY_AREA):
        has_area_id_select = any(
            entry.get(CONF_LD6002B_ID) == hub_id and entry.get(CONF_AREA_ID) is not None
            for entry in full_config.get(SELECT_DOMAIN, [])
        )
        if not has_area_id_select:
            raise cv.Invalid(
                f"{CONF_APPLY_AREA} requires select.area_id for the same ld6002b instance",
                path=[CONF_APPLY_AREA],
            )

    if config.get(CONF_WAKE):
        hub_config = fv.get_declaring_config(hub_id, full_config)
        if hub_config.get(CONF_WAKEUP_PIN) is None:
            raise cv.Invalid(
                f"{CONF_WAKE} requires {CONF_WAKEUP_PIN} on the parent ld6002b component",
                path=[CONF_WAKE],
            )


FINAL_VALIDATE_SCHEMA = final_validate

BUTTON_MAP = {
    CONF_APPLY_AREA: ButtonType.BUTTON_APPLY_AREA,
    CONF_AUTO_INTERFERENCE: ButtonType.BUTTON_AUTO_INTERFERENCE,
    CONF_GET_AREAS: ButtonType.BUTTON_GET_AREAS,
    CONF_CLEAR_INTERFERENCE: ButtonType.BUTTON_CLEAR_INTERFERENCE,
    CONF_RESET_DETECTION_AREA: ButtonType.BUTTON_RESET_DETECTION_AREA,
    CONF_GET_DELAY: ButtonType.BUTTON_GET_DELAY,
    CONF_GET_SENSITIVITY: ButtonType.BUTTON_GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED: ButtonType.BUTTON_GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE: ButtonType.BUTTON_GET_Z_RANGE,
    CONF_GET_INSTALLATION: ButtonType.BUTTON_GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE: ButtonType.BUTTON_GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME: ButtonType.BUTTON_GET_LOW_POWER_SLEEP_TIME,
    CONF_RESET_UNATTENDED: ButtonType.BUTTON_RESET_UNATTENDED,
    CONF_WAKE: ButtonType.BUTTON_WAKE,
}


async def to_code(config: ConfigType) -> None:
    confs = [
        (button_type, conf)
        for key, button_type in BUTTON_MAP.items()
        if (conf := config.get(key)) is not None
    ]
    if not confs:
        return
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    params = button.APPLY_ENTITY.params
    # One function for all buttons, picking the type by pointer: one class instead of one per button.
    # Named after the first button so two button blocks on one hub do not clash.
    fn = f"esphome__{confs[0][1][CONF_ID].id}__button_fn"
    cg.add_global(
        cg.RawStatement(f"static void {fn}({ParameterListExpression(*params)});")
    )
    template = cg.TemplateArguments(cg.RawExpression(fn))
    created = [
        (button_type, await button.new_button(conf, template))
        for button_type, conf in confs
    ]
    *head, (last_type, _) = created
    chain = "".join(f"entity == {parent_ref(var)} ? {t} : " for t, var in head)
    cg.static_function(
        fn,
        cg.void,
        list(params),
        [
            f"const auto type = {chain}{last_type};",
            f"{parent_ref(hub)}->press_button(type);",
        ],
    )

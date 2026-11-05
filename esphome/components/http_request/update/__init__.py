from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components import update
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SOURCE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_HTTP_REQUEST_ID, DOMAIN, HttpRequestComponent, http_request_ns

AUTO_LOAD = ["json"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["http_request"]

HttpRequestUpdate = http_request_ns.class_(
    "HttpRequestUpdate", update.UpdateEntity, cg.PollingComponent
)

# Interface for components that install an image announced by an update
# manifest. See update_target.h.
UpdateTarget = http_request_ns.class_("UpdateTarget")

CONF_OTA_ID = "ota_id"
CONF_TARGET_ID = "target_id"

CONFIG_SCHEMA = cv.All(
    cv.rename_key(
        CONF_OTA_ID,
        CONF_TARGET_ID,
        removed_in="2027.5.0",
        component="update.http_request",
    ),
    update.update_schema(HttpRequestUpdate)
    .extend(
        {
            cv.GenerateID(CONF_TARGET_ID): cv.use_id(UpdateTarget),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Required(CONF_SOURCE): cv.url,
        }
    )
    .extend(cv.polling_component_schema("6h")),
)


@dataclass
class HttpRequestUpdateData:
    # Target id to the id of the update entity that installs through it.
    target_owners: dict[str, str] = field(default_factory=dict)


def _get_data(fconf: fv.FinalValidateConfig) -> HttpRequestUpdateData:
    if DOMAIN not in fconf.data:
        fconf.data[DOMAIN] = HttpRequestUpdateData()
    return fconf.data[DOMAIN]


def _no_shared_targets(config: ConfigType) -> ConfigType:
    """A target reports its install progress to one entity, so two cannot share it."""
    owners = _get_data(fv.full_config.get()).target_owners
    target_id = str(config[CONF_TARGET_ID])
    entity_id = str(config[CONF_ID])
    if (other := owners.setdefault(target_id, entity_id)) != entity_id:
        raise cv.Invalid(
            f"'{target_id}' is already updated by '{other}'. An update target can only "
            f"be used by one update entity.",
            path=[CONF_TARGET_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _no_shared_targets


async def to_code(config: ConfigType) -> None:
    var = await update.new_update(config)

    # Lets a target compile in the code that reports install progress back here.
    cg.add_define("USE_HTTP_REQUEST_UPDATE")

    await cg.add_variable(var.set_target, config[CONF_TARGET_ID])
    await cg.add_variable(var.set_request_parent, config[CONF_HTTP_REQUEST_ID])

    cg.add(var.set_source_url_static(config[CONF_SOURCE]))

    await cg.register_component(var, config)

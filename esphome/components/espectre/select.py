import logging

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core import CORE
from esphome.types import ConfigType

from . import (
    CONF_CSI_SAMPLE_SPACING,
    CONF_DETECTION_ALGORITHM,
    CONF_ESPECTRE_ID,
    CONF_TRAFFIC_GENERATOR_MODE,
    CSI_SAMPLE_SPACINGS,
    DETECTION_ALGORITHMS,
    DOMAIN,
    ESPectreComponent,
    supported_traffic_generator_modes,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["espectre"]

SELECT_KEYS = (
    CONF_TRAFFIC_GENERATOR_MODE,
    CONF_DETECTION_ALGORITHM,
    CONF_CSI_SAMPLE_SPACING,
)


def _move_to_traffic_generator_mode(config: ConfigType) -> ConfigType:
    """Move a select configured directly on the platform under traffic_generator_mode."""
    # Remove before 2027.5.0
    if not isinstance(config, dict) or any(key in config for key in SELECT_KEYS):
        return config
    _LOGGER.warning(
        "[espectre] Configuring the select directly on the platform is deprecated, "
        "move it under '%s'. Will be removed in 2027.5.0",
        CONF_TRAFFIC_GENERATOR_MODE,
    )
    entity = dict(config)
    migrated = {CONF_TRAFFIC_GENERATOR_MODE: entity}
    if CONF_ESPECTRE_ID in entity:
        migrated[CONF_ESPECTRE_ID] = entity.pop(CONF_ESPECTRE_ID)
    return migrated


SELECT_SCHEMA = select.select_schema(
    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
)


CONFIG_SCHEMA = cv.All(
    _move_to_traffic_generator_mode,
    cv.Schema(
        {
            cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
            cv.Optional(CONF_TRAFFIC_GENERATOR_MODE): SELECT_SCHEMA,
            cv.Optional(CONF_DETECTION_ALGORITHM): SELECT_SCHEMA,
            cv.Optional(CONF_CSI_SAMPLE_SPACING): SELECT_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(*SELECT_KEYS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    selects = select.sub_selects(config, parent=hub)
    # The hub publishes each option once the runtime has applied it.
    await selects(
        CONF_TRAFFIC_GENERATOR_MODE,
        hub.set_traffic_mode_select,
        options=supported_traffic_generator_modes(CORE.config[DOMAIN]),
        apply="request_traffic_generator_mode(option)",
    )
    await selects(
        CONF_DETECTION_ALGORITHM,
        hub.set_detection_algorithm_select,
        options=list(DETECTION_ALGORITHMS),
        apply="request_detection_algorithm(option)",
    )
    await selects(
        CONF_CSI_SAMPLE_SPACING,
        hub.set_csi_sample_spacing_select,
        options=list(CSI_SAMPLE_SPACINGS),
        apply="request_csi_sample_spacing(option)",
    )

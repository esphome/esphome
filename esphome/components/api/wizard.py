"""The device wizard of the api component: schema, validation and code generation.

Home Assistant shows the wizard when the device is added. See api_wizard.h for the C++ side.
"""

from collections.abc import Callable
import importlib
import json
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_DESCRIPTION
from esphome.components.homeassistant import DOMAIN as HOMEASSISTANT_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_DEVICE_ID,
    CONF_DOMAIN,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_PAGES,
    CONF_PLATFORM,
    CONF_TARGET,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.helpers import (
    fnv1_hash,
    fnv1_hash_object_id,
    fnv1a_32bit_hash,
    zstd_module,
)
from esphome.types import ConfigType

API_DOMAIN = "api"

CONF_ENTITIES = "entities"
CONF_ENTITY = "entity"
CONF_INPUTS = "inputs"
CONF_INTEGRATION = "integration"
CONF_SUPPORTED_FEATURES = "supported_features"
CONF_TITLE = "title"
CONF_WIZARD = "wizard"

_API = cg.esphome_ns.namespace("api")
WizardInput = _API.class_("WizardInput")

WIZARD_ENTITY_ID_BUFFER_SIZE = (
    256  # api_wizard.h; Home Assistant entity IDs are at most 255 bytes
)
# One API message must fit APIBuffer::MAX_SIZE (65535) together with the largest frame header (7 bytes,
# Noise) and footer (16 bytes, Noise MAC)
WIZARD_RESPONSE_MAX_SIZE = 65535 - 7 - 16
# Version of the JSON the wizard is sent as, see wizard_document()
WIZARD_JSON_VERSION = 1
# zstd level the JSON is compressed with. Output is deterministic for a given zstd version.
WIZARD_ZSTD_LEVEL = 19

# Wizard string limits; api.proto documents the same values
WIZARD_TITLE_MAX_LENGTH = 127
WIZARD_DESCRIPTION_MAX_LENGTH = 255
WIZARD_FILTER_MAX_LENGTH = 63
WIZARD_SUPPORTED_FEATURE_MAX_LENGTH = 127


def _wizard_text(max_length: int) -> Callable[[Any], str]:
    """A string passed to Home Assistant verbatim, so it may be a [%key:...%] translation placeholder."""
    return cv.All(cv.string_strict, cv.Length(max=max_length))


def _wizard_strings(max_length: int) -> Callable[[Any], list[str]]:
    """A single string or a list of strings, always validated to a non-empty list."""
    return cv.All(
        cv.ensure_list(cv.All(cv.string_strict, cv.Length(min=1, max=max_length))),
        cv.Length(min=1),
    )


# Mirrors Home Assistant's EntityFilterSelectorConfig
WIZARD_ENTITY_FILTER_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_INTEGRATION): cv.All(
                cv.string_strict, cv.Length(min=1, max=WIZARD_FILTER_MAX_LENGTH)
            ),
            cv.Optional(CONF_DOMAIN): _wizard_strings(WIZARD_FILTER_MAX_LENGTH),
            cv.Optional(CONF_DEVICE_CLASS): _wizard_strings(WIZARD_FILTER_MAX_LENGTH),
            cv.Optional(CONF_SUPPORTED_FEATURES): _wizard_strings(
                WIZARD_SUPPORTED_FEATURE_MAX_LENGTH
            ),
        }
    ),
    cv.has_at_least_one_key(
        CONF_INTEGRATION, CONF_DOMAIN, CONF_DEVICE_CLASS, CONF_SUPPORTED_FEATURES
    ),
)

WIZARD_ENTITY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(cg.EntityBase),
        cv.Optional(CONF_DESCRIPTION): _wizard_text(WIZARD_DESCRIPTION_MAX_LENGTH),
    }
)


# An input is either standalone (id declares a new WizardInput) or linked to a homeassistant entity that has no
# entity_id of its own: Home Assistant sets it
WIZARD_INPUT_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_ID): cv.declare_id(WizardInput),
            cv.Optional(CONF_ENTITY): cv.use_id(cg.EntityBase),
            cv.Optional(CONF_DESCRIPTION): _wizard_text(WIZARD_DESCRIPTION_MAX_LENGTH),
            cv.Optional(CONF_TARGET): cv.Schema(
                {
                    cv.Required(CONF_ENTITY): cv.All(
                        cv.ensure_list(WIZARD_ENTITY_FILTER_SCHEMA), cv.Length(min=1)
                    ),
                }
            ),
        }
    ),
    cv.has_exactly_one_key(CONF_ID, CONF_ENTITY),
)

WIZARD_PAGE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_TITLE): _wizard_text(WIZARD_TITLE_MAX_LENGTH),
            cv.Optional(CONF_DESCRIPTION): _wizard_text(WIZARD_DESCRIPTION_MAX_LENGTH),
            cv.Optional(CONF_ENTITIES): cv.All(
                cv.ensure_list(WIZARD_ENTITY_SCHEMA), cv.Length(min=1)
            ),
            cv.Optional(CONF_INPUTS): cv.All(
                cv.ensure_list(WIZARD_INPUT_SCHEMA), cv.Length(min=1)
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_ENTITIES, CONF_INPUTS),
)


def _wizard_inputs(wizard: ConfigType) -> list[ConfigType]:
    return [conf for page in wizard[CONF_PAGES] for conf in page.get(CONF_INPUTS, [])]


def _wizard_input_id(conf: ConfigType) -> ID:
    """The ID that names an input: the one a standalone input declares, or its linked entity."""
    return conf[CONF_ID] if CONF_ID in conf else conf[CONF_ENTITY]


def _validate_unique_wizard_inputs(wizard: ConfigType) -> ConfigType:
    """An input is identified on the wire by a hash of its ID, so IDs and hashes must be unique."""
    seen: dict[int, str] = {}
    for conf in _wizard_inputs(wizard):
        input_id = _wizard_input_id(conf).id
        if (key := fnv1_hash(input_id)) in seen:
            if seen[key] == input_id:
                raise cv.Invalid(f"Wizard input '{input_id}' is used more than once")
            raise cv.Invalid(
                f"Wizard inputs '{seen[key]}' and '{input_id}' have the same hash, rename one"
            )
        seen[key] = input_id
    return wizard


WIZARD_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PAGES): cv.All(
                cv.ensure_list(WIZARD_PAGE_SCHEMA), cv.Length(min=1)
            ),
        }
    ),
    _validate_unique_wizard_inputs,
)

# Platforms of the homeassistant component that a wizard input can stand for
WIZARD_INPUT_DOMAINS = (
    "binary_sensor",
    "button",
    "number",
    "select",
    "sensor",
    "switch",
    "text",
    "text_sensor",
)
# Platforms that act on one family of Home Assistant domains, which the input's filters must stay within
WIZARD_DOMAIN_LIMITED_PLATFORMS = ("button", "number", "select", "switch", "text")


def wizard_input_ids(api_config: ConfigType) -> set[str]:
    """The IDs of the entities that are linked inputs of the wizard in the given api config."""
    if (wizard := api_config.get(CONF_WIZARD)) is None:
        return set()
    return {
        conf[CONF_ENTITY].id for conf in _wizard_inputs(wizard) if CONF_ENTITY in conf
    }


def _wizard_buffer_name(entity_id: ID) -> str:
    return f"api_wizard_input_{entity_id.id}"


def wizard_input_buffer(entity_id: ID) -> str | None:
    """Name of the RAM buffer holding the Home Assistant entity ID of a wizard input.

    Returns None when the entity is not an input of the wizard. The buffer is defined by the api
    codegen, and the homeassistant entity of the input is given it in place of a constant.
    """
    if entity_id.id not in wizard_input_ids(CORE.config.get(API_DOMAIN, {})):
        return None
    return _wizard_buffer_name(entity_id)


def _wizard_input_declaration(
    config: fv.FinalValidateConfig, entity_id: ID
) -> tuple[str, ConfigType]:
    """The domain (like sensor) and the config an input ID is declared in."""
    path = config.get_path_for_id(entity_id)[:-1]
    return path[0], config.get_config_for_path(path)


def _wizard_input_filters(
    conf: ConfigType, config: fv.FinalValidateConfig
) -> list[ConfigType]:
    """The entity filters of an input, with the defaults of a domain limited linked platform."""
    if (filters := conf.get(CONF_TARGET, {}).get(CONF_ENTITY)) is not None:
        return filters
    if CONF_ENTITY in conf:
        domain, _ = _wizard_input_declaration(config, conf[CONF_ENTITY])
        if domains := _wizard_default_domains(domain):
            return [{CONF_DOMAIN: domains}]
    return []


def _wizard_defines(wizard: ConfigType) -> set[str]:
    """The defines for the parts of the wizard the configuration uses, so the rest is not compiled."""
    inputs = _wizard_inputs(wizard)
    defines = {"USE_API_WIZARD"}
    if inputs:
        defines.add("USE_API_WIZARD_INPUTS")
    if any(CONF_ENTITY in conf for conf in inputs):
        defines.add("USE_API_WIZARD_LINKED_INPUTS")
    if any(CONF_ID in conf for conf in inputs):
        defines.add("USE_API_WIZARD_STANDALONE_INPUTS")
    return defines


def _wizard_default_domains(domain: str) -> list[str] | None:
    """The domains Home Assistant entities can be picked from when the input sets no target."""
    if domain in WIZARD_DOMAIN_LIMITED_PLATFORMS:
        platform = importlib.import_module(f"esphome.components.homeassistant.{domain}")
        return list(platform.SUPPORTED_DOMAINS)
    return None


def _validate_wizard_input(conf: ConfigType) -> ConfigType:
    if CONF_ENTITY not in conf:
        return conf
    domain, declaration = _wizard_input_declaration(
        fv.full_config.get(), conf[CONF_ENTITY]
    )
    if (
        declaration.get(CONF_PLATFORM) != HOMEASSISTANT_DOMAIN
        or domain not in WIZARD_INPUT_DOMAINS
    ):
        raise cv.Invalid(
            f"Wizard input '{conf[CONF_ENTITY].id}' must be a homeassistant "
            f"{', '.join(WIZARD_INPUT_DOMAINS)} entity"
        )
    if CONF_ENTITY_ID in declaration:
        # An entity_id in the configuration is a static entry, which is kept apart from the dynamic ones
        raise cv.Invalid(
            f"'{conf[CONF_ENTITY].id}' has an entity_id set in its configuration, so it cannot be a "
            "wizard input. Remove entity_id to let Home Assistant set it through the wizard."
        )
    if domain in WIZARD_DOMAIN_LIMITED_PLATFORMS:
        supported = _wizard_default_domains(domain)
        for entity_filter in conf.get(CONF_TARGET, {}).get(CONF_ENTITY, []):
            if not (domains := entity_filter.get(CONF_DOMAIN)):
                raise cv.Invalid(
                    f"Every filter of a homeassistant {domain} input must set domain"
                )
            if unsupported := [d for d in domains if d not in supported]:
                raise cv.Invalid(
                    f"The homeassistant {domain} does not support the domain(s) "
                    f"{', '.join(unsupported)}. Supported: {', '.join(supported)}"
                )
    return conf


def _validate_wizard_entity_exposed(value: ID) -> ID:
    """Reject entities that are internal, as they are not exposed over the API, or have no name.

    The key a client knows an entity by is a hash of its name. Without a name of its own, the
    device works the name out at runtime from its friendly name, which can add the MAC address,
    so the key cannot be known when the wizard is built.
    """
    _, declaration = _wizard_input_declaration(fv.full_config.get(), value)
    if declaration.get(CONF_INTERNAL, False):
        raise cv.Invalid(
            f"Entity '{value.id}' is internal, so it is not exposed over the API "
            "and cannot be used in the wizard"
        )
    if not declaration.get(CONF_NAME):
        raise cv.Invalid(
            f"Entity '{value.id}' has no name of its own, so its key is not known "
            "when the wizard is built. Give it a name to use it in the wizard"
        )
    return value


_WIZARD_FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WIZARD): {
            cv.Optional(CONF_PAGES): [
                {
                    cv.Optional(CONF_ENTITIES): [
                        {cv.Optional(CONF_ID): _validate_wizard_entity_exposed}
                    ],
                    cv.Optional(CONF_INPUTS): [_validate_wizard_input],
                }
            ]
        }
    },
    extra=cv.ALLOW_EXTRA,
)


def final_validate(config: ConfigType) -> None:
    """Final validation of the wizard in the given api config, if it has one."""
    _WIZARD_FINAL_VALIDATE_SCHEMA(config)
    if (wizard := config.get(CONF_WIZARD)) is not None:
        size = len(wizard_blob(wizard, fv.full_config.get()))
        if size > WIZARD_RESPONSE_MAX_SIZE:
            raise cv.Invalid(
                f"The compressed wizard is {size} bytes, {size - WIZARD_RESPONSE_MAX_SIZE} "
                f"bytes over the {WIZARD_RESPONSE_MAX_SIZE} bytes one API message can hold. "
                "Shorten the texts or use fewer pages, entities or filters",
                path=[CONF_WIZARD],
            )


WIZARD_INPUT_IS_SET_SCHEMA = cv.maybe_simple_value(
    {cv.Required(CONF_ID): cv.use_id(WizardInput)}, key=CONF_ID
)

# Only for standalone inputs: a linked input is read through its homeassistant entity
automation.register_apply_condition(
    "api.wizard.input_is_set", WIZARD_INPUT_IS_SET_SCHEMA, "has_entity_id()"
)


def _entity_document(conf: ConfigType, config: fv.FinalValidateConfig) -> ConfigType:
    """An entity of the device that the page shows, keyed as ListEntitiesResponse keys it."""
    _, declaration = _wizard_input_declaration(config, conf[CONF_ID])
    document: ConfigType = {"key": fnv1_hash_object_id(declaration[CONF_NAME])}
    if (device := declaration.get(CONF_DEVICE_ID)) is not None:
        document["device_id"] = fnv1a_32bit_hash(device.id)
    if description := conf.get(CONF_DESCRIPTION):
        document[CONF_DESCRIPTION] = description
    return document


def _input_document(conf: ConfigType, config: fv.FinalValidateConfig) -> ConfigType:
    document: ConfigType = {"key": fnv1_hash(_wizard_input_id(conf).id)}
    if description := conf.get(CONF_DESCRIPTION):
        document[CONF_DESCRIPTION] = description
    if filters := _wizard_input_filters(conf, config):
        document["entity_filters"] = [dict(entity_filter) for entity_filter in filters]
    return document


def wizard_document(wizard: ConfigType, config: fv.FinalValidateConfig) -> ConfigType:
    """The wizard as the JSON document the device sends, before it is serialised.

    This is the format Home Assistant reads, and api.proto documents it for clients. Version 1:

        {"version": 1,
         "pages": [{"title": "...", "description": "...",
           "entities": [{"key": 123, "device_id": 456, "description": "..."}],
           "inputs": [{"key": 789, "description": "...",
             "entity_filters": [{"integration": "...", "domain": ["..."],
                                 "device_class": ["..."], "supported_features": ["..."]}]}]}]}

    Anything empty or unset, and every empty list, is left out. Strings are passed through as
    written, so they may be Home Assistant translation placeholders.

    - An entity key is the key ListEntitiesResponse sends for the entity: the FNV-1 hash of the
      object id made from its name (entity_helpers). device_id is the hash of the ESPHome id of
      the device it belongs to (esphome/core/config.py), and is left out for the main device.
    - An input key is the FNV-1 hash of the ESPHome id of the input, or of the linked entity. A linked
      entity must not set entity_id, as that is a static entry kept apart from the ones the wizard sets.
    - entity_filters are the filters of the input, or the default filters of a linked switch,
      number, text, select or button.
    """
    pages: list[ConfigType] = []
    for page in wizard[CONF_PAGES]:
        document: ConfigType = {}
        for key in (CONF_TITLE, CONF_DESCRIPTION):
            if value := page.get(key):
                document[key] = value
        if entities := [
            _entity_document(e, config) for e in page.get(CONF_ENTITIES, [])
        ]:
            document[CONF_ENTITIES] = entities
        if inputs := [_input_document(i, config) for i in page.get(CONF_INPUTS, [])]:
            document[CONF_INPUTS] = inputs
        pages.append(document)
    return {"version": WIZARD_JSON_VERSION, CONF_PAGES: pages}


def wizard_blob(wizard: ConfigType, config: fv.FinalValidateConfig) -> bytes:
    """The wizard document as compact, sorted UTF-8 JSON in a single zstd frame."""
    text = json.dumps(
        wizard_document(wizard, config),
        separators=(",", ":"),
        sort_keys=True,
        ensure_ascii=False,
    )
    return zstd_module().compress(text.encode("utf-8"), level=WIZARD_ZSTD_LEVEL)


async def to_code(wizard: ConfigType) -> None:
    """Emit the compressed wizard, the table of inputs and the defines.

    The API reads both tables from its own sources, so they are externally linked PROGMEM arrays.
    """
    blob = wizard_blob(wizard, CORE.config)
    cg.extern_progmem_array("esphome::api::API_WIZARD_DATA", cg.uint8, list(blob))
    cg.add_define("API_WIZARD_DATA_SIZE", len(blob))
    for define in sorted(_wizard_defines(wizard)):
        cg.add_define(define)
    entries: list[cg.RawExpression] = []
    for conf in _wizard_inputs(wizard):
        input_id = _wizard_input_id(conf)
        # Every buffer starts empty, until the wizard sets it. A linked homeassistant entity uses it as its entity id.
        buffer = _wizard_buffer_name(input_id)
        cg.add_global(
            cg.RawStatement(
                f'static char {buffer}[{WIZARD_ENTITY_ID_BUFFER_SIZE}] = "";'
            )
        )
        if CONF_ID in conf:
            cg.new_Pvariable(input_id, cg.RawExpression(buffer))
        entries.append(cg.RawExpression(f"{{{fnv1_hash(input_id.id)}u, {buffer}}}"))
    if entries:
        cg.extern_progmem_array(
            "esphome::api::API_WIZARD_INPUTS",
            cg.esphome_ns.namespace("api").struct("WizardInputEntry"),
            entries,
        )
        cg.add_define("API_WIZARD_INPUT_COUNT", len(entries))

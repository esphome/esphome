import collections
import logging
import re

# pylint: disable=unused-import, wrong-import-order
from contextlib import contextmanager

import voluptuous as vol

from esphome import core, yaml_util, loader
import esphome.core.config as core_config
from esphome.const import (
    CONF_ESPHOME,
    CONF_PLATFORM,
    CONF_PACKAGES,
    CONF_SUBSTITUTIONS,
    CONF_EXTERNAL_COMPONENTS,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import indent
from esphome.util import safe_print, OrderedDict

from typing import List, Optional, Tuple, Union
from esphome.loader import get_component, get_platform, ComponentManifest
from esphome.yaml_util import is_secret, ESPHomeDataBase, ESPForceValue
from esphome.voluptuous_schema import ExtraKeysInvalid
from esphome.log import color, Fore
import esphome.final_validate as fv
import esphome.config_validation as cv
from esphome.types import ConfigType, ConfigPathType, ConfigFragmentType

_LOGGER = logging.getLogger(__name__)


def iter_components(config):
    for domain, conf in config.items():
        component = get_component(domain)
        if component.multi_conf:
            for conf_ in conf:
                yield domain, component, conf_
        else:
            yield domain, component, conf
        if component.is_platform_component:
            for p_config in conf:
                p_name = "{}.{}".format(domain, p_config[CONF_PLATFORM])
                platform = get_platform(domain, p_config[CONF_PLATFORM])
                yield p_name, platform, p_config


ConfigPath = List[Union[str, int]]


def _path_begins_with(path, other):  # type: (ConfigPath, ConfigPath) -> bool
    if len(path) < len(other):
        return False
    return path[: len(other)] == other


class Config(OrderedDict, fv.FinalValidateConfig):
    def __init__(self):
        super().__init__()
        # A list of voluptuous errors
        self.errors = []  # type: List[vol.Invalid]
        # A list of paths that should be fully outputted
        # The values will be the paths to all "domain", for example (['logger'], 'logger')
        # or (['sensor', 'ultrasonic'], 'sensor.ultrasonic')
        self.output_paths = []  # type: List[Tuple[ConfigPath, str]]
        # A list of components ids with the config path
        self.declare_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
        self._data = {}

    def add_error(self, error):
        # type: (vol.Invalid) -> None
        if isinstance(error, vol.MultipleInvalid):
            for err in error.errors:
                self.add_error(err)
            return
        if cv.ROOT_CONFIG_PATH in error.path:
            # Root value means that the path before the root should be ignored
            last_root = max(
                i for i, v in enumerate(error.path) if v is cv.ROOT_CONFIG_PATH
            )
            error.path = error.path[last_root + 1 :]
        self.errors.append(error)

    @contextmanager
    def catch_error(self, path=None):
        path = path or []
        try:
            yield
        except vol.Invalid as e:
            e.prepend(path)
            self.add_error(e)

    def add_str_error(self, message, path):
        # type: (str, ConfigPath) -> None
        self.add_error(vol.Invalid(message, path))

    def add_output_path(self, path, domain):
        # type: (ConfigPath, str) -> None
        self.output_paths.append((path, domain))

    def remove_output_path(self, path, domain):
        # type: (ConfigPath, str) -> None
        self.output_paths.remove((path, domain))

    def is_in_error_path(self, path):
        # type: (ConfigPath) -> bool
        for err in self.errors:
            if _path_begins_with(err.path, path):
                return True
        return False

    def set_by_path(self, path, value):
        conf = self
        for key in path[:-1]:
            conf = conf[key]
        conf[path[-1]] = value

    def get_error_for_path(self, path):
        # type: (ConfigPath) -> Optional[vol.Invalid]
        for err in self.errors:
            if self.get_deepest_path(err.path) == path:
                return err
        return None

    def get_deepest_document_range_for_path(self, path):
        # type: (ConfigPath) -> Optional[ESPHomeDataBase]
        data = self
        doc_range = None
        for item_index in path:
            try:
                if item_index in data:
                    doc_range = [x for x in data.keys() if x == item_index][0].esp_range
                data = data[item_index]
            except (KeyError, IndexError, TypeError, AttributeError):
                return doc_range
            if isinstance(data, core.ID):
                data = data.id
            if isinstance(data, ESPHomeDataBase) and data.esp_range is not None:
                doc_range = data.esp_range
            elif isinstance(data, dict):
                platform_item = data.get("platform")
                if (
                    isinstance(platform_item, ESPHomeDataBase)
                    and platform_item.esp_range is not None
                ):
                    doc_range = platform_item.esp_range

        return doc_range

    def get_nested_item(
        self, path: ConfigPathType, raise_error: bool = False
    ) -> ConfigFragmentType:
        data = self
        for item_index in path:
            try:
                data = data[item_index]
            except (KeyError, IndexError, TypeError):
                if raise_error:
                    raise
                return {}
        return data

    def get_deepest_path(self, path):
        # type: (ConfigPath) -> ConfigPath
        """Return the path that is the deepest reachable by following path."""
        data = self
        part = []
        for item_index in path:
            try:
                data = data[item_index]
            except (KeyError, IndexError, TypeError):
                return part
            part.append(item_index)
        return part

    def get_path_for_id(self, id: core.ID):
        """Return the config fragment where the given ID is declared."""
        for declared_id, path in self.declare_ids:
            if declared_id.id == str(id):
                return path
        raise KeyError(f"ID {id} not found in configuration")

    def get_config_for_path(self, path: ConfigPathType) -> ConfigFragmentType:
        return self.get_nested_item(path, raise_error=True)

    @property
    def data(self):
        """Return temporary data used by final validation functions."""
        return self._data


def iter_ids(config, path=None):
    path = path or []
    if isinstance(config, core.ID):
        yield config, path
    elif isinstance(config, core.Lambda):
        for id in config.requires_ids:
            yield id, path
    elif isinstance(config, list):
        for i, item in enumerate(config):
            yield from iter_ids(item, path + [i])
    elif isinstance(config, dict):
        for key, value in config.items():
            yield from iter_ids(value, path + [key])


def do_id_pass(result):  # type: (Config) -> None
    from esphome.cpp_generator import MockObjClass
    from esphome.cpp_types import Component

    searching_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
    for id, path in iter_ids(result):
        if id.is_declaration:
            if id.id is not None:
                # Look for duplicate definitions
                match = next((v for v in result.declare_ids if v[0].id == id.id), None)
                if match is not None:
                    opath = "->".join(str(v) for v in match[1])
                    result.add_str_error(f"ID {id.id} redefined! Check {opath}", path)
                    continue
            result.declare_ids.append((id, path))
        else:
            searching_ids.append((id, path))
    # Resolve default ids after manual IDs
    for id, _ in result.declare_ids:
        id.resolve([v[0].id for v in result.declare_ids])
        if isinstance(id.type, MockObjClass) and id.type.inherits_from(Component):
            CORE.component_ids.add(id.id)

    # Check searched IDs
    for id, path in searching_ids:
        if id.id is not None:
            # manually declared
            match = next((v[0] for v in result.declare_ids if v[0].id == id.id), None)
            if match is None or not match.is_manual:
                # No declared ID with this name
                import difflib

                error = (
                    "Couldn't find ID '{}'. Please check you have defined "
                    "an ID with that name in your configuration.".format(id.id)
                )
                # Find candidates
                matches = difflib.get_close_matches(
                    id.id, [v[0].id for v in result.declare_ids if v[0].is_manual]
                )
                if matches:
                    matches_s = ", ".join(f'"{x}"' for x in matches)
                    error += f" These IDs look similar: {matches_s}."
                result.add_str_error(error, path)
                continue
            if not isinstance(match.type, MockObjClass) or not isinstance(
                id.type, MockObjClass
            ):
                continue
            if not match.type.inherits_from(id.type):
                result.add_str_error(
                    "ID '{}' of type {} doesn't inherit from {}. Please "
                    "double check your ID is pointing to the correct value"
                    "".format(id.id, match.type, id.type),
                    path,
                )

        if id.id is None and id.type is not None:
            matches = []
            for v in result.declare_ids:
                if v[0] is None or not isinstance(v[0].type, MockObjClass):
                    continue
                inherits = v[0].type.inherits_from(id.type)
                if inherits:
                    matches.append(v[0])

            if len(matches) == 0:
                result.add_str_error(
                    f"Couldn't find any component that can be used for '{id.type}'. Are you missing a hub declaration?",
                    path,
                )
            elif len(matches) == 1:
                id.id = matches[0].id
            elif len(matches) > 1:
                if str(id.type) == "time::RealTimeClock":
                    id.id = matches[0].id
                else:
                    manual_declared_count = sum(1 for m in matches if m.is_manual)
                    if manual_declared_count > 0:
                        ids = ", ".join([f"'{m.id}'" for m in matches if m.is_manual])
                        result.add_str_error(
                            f"Too many candidates found for '{path[-1]}' type '{id.type}' {'Some are' if manual_declared_count > 1 else 'One is'} {ids}",
                            path,
                        )
                    else:
                        result.add_str_error(
                            f"Too many candidates found for '{path[-1]}' type '{id.type}' You must assign an explicit ID to the parent component you want to use.",
                            path,
                        )


def recursive_check_replaceme(value):
    if isinstance(value, list):
        return cv.Schema([recursive_check_replaceme])(value)
    if isinstance(value, dict):
        return cv.Schema({cv.valid: recursive_check_replaceme})(value)
    if isinstance(value, ESPForceValue):
        pass
    if isinstance(value, str) and value == "REPLACEME":
        raise cv.Invalid(
            "Found 'REPLACEME' in configuration, this is most likely an error. "
            "Please make sure you have replaced all fields from the sample "
            "configuration.\n"
            "If you want to use the literal REPLACEME string, "
            'please use "!force REPLACEME"'
        )
    return value


def validate_config(config, command_line_substitutions):
    result = Config()

    loader.clear_component_meta_finders()
    loader.install_custom_components_meta_finder()

    # 0. Load packages
    if CONF_PACKAGES in config:
        from esphome.components.packages import do_packages_pass

        result.add_output_path([CONF_PACKAGES], CONF_PACKAGES)
        try:
            config = do_packages_pass(config)
        except vol.Invalid as err:
            result.update(config)
            result.add_error(err)
            return result

    # 1. Load substitutions
    if CONF_SUBSTITUTIONS in config:
        from esphome.components import substitutions

        result[CONF_SUBSTITUTIONS] = {
            **config[CONF_SUBSTITUTIONS],
            **command_line_substitutions,
        }
        result.add_output_path([CONF_SUBSTITUTIONS], CONF_SUBSTITUTIONS)
        try:
            substitutions.do_substitution_pass(config, command_line_substitutions)
        except vol.Invalid as err:
            result.add_error(err)
            return result

    # 1.1. Check for REPLACEME special value
    try:
        recursive_check_replaceme(config)
    except vol.Invalid as err:
        result.add_error(err)

    # 1.2. Load external_components
    if CONF_EXTERNAL_COMPONENTS in config:
        from esphome.components.external_components import do_external_components_pass

        result.add_output_path([CONF_EXTERNAL_COMPONENTS], CONF_EXTERNAL_COMPONENTS)
        try:
            do_external_components_pass(config)
        except vol.Invalid as err:
            result.update(config)
            result.add_error(err)
            return result

    if "esphomeyaml" in config:
        _LOGGER.warning(
            "The esphomeyaml section has been renamed to esphome in 1.11.0. "
            "Please replace 'esphomeyaml:' in your configuration with 'esphome:'."
        )
        config[CONF_ESPHOME] = config.pop("esphomeyaml")

    if CONF_ESPHOME not in config:
        result.add_str_error(
            "'esphome' section missing from configuration. Please make sure "
            "your configuration has an 'esphome:' line in it.",
            [],
        )
        return result

    # 2. Load partial core config
    result[CONF_ESPHOME] = config[CONF_ESPHOME]
    result.add_output_path([CONF_ESPHOME], CONF_ESPHOME)
    try:
        core_config.preload_core_config(config)
    except vol.Invalid as err:
        result.add_error(err)
        return result
    # Remove temporary esphome config path again, it will be reloaded later
    result.remove_output_path([CONF_ESPHOME], CONF_ESPHOME)

    # 3. Load components.
    # Load components (also AUTO_LOAD) and set output paths of result
    # Queue of items to load, FIFO
    load_queue = collections.deque()
    for domain, conf in config.items():
        load_queue.append((domain, conf))

    # List of items to enter next stage
    check_queue = (
        []
    )  # type: List[Tuple[ConfigPath, str, ConfigType, ComponentManifest]]

    # This step handles:
    # - Adding output path
    # - Auto Load
    # - Loading configs into result

    while load_queue:
        domain, conf = load_queue.popleft()
        if domain.startswith("."):
            # Ignore top-level keys starting with a dot
            continue
        result.add_output_path([domain], domain)
        result[domain] = conf
        component = get_component(domain)
        path = [domain]
        if component is None:
            result.add_str_error(f"Component not found: {domain}", path)
            continue
        CORE.loaded_integrations.add(domain)

        # Process AUTO_LOAD
        for load in component.auto_load:
            if load not in config:
                load_conf = core.AutoLoad()
                config[load] = load_conf
                load_queue.append((load, load_conf))

        if not component.is_platform_component:
            check_queue.append(([domain], domain, conf, component))
            continue

        # This is a platform component, proceed to reading platform entries
        # Remove this is as an output path
        result.remove_output_path([domain], domain)

        # Ensure conf is a list
        if not conf:
            result[domain] = conf = []
        elif not isinstance(conf, list):
            result[domain] = conf = [conf]

        for i, p_config in enumerate(conf):
            path = [domain, i]
            # Construct temporary unknown output path
            p_domain = f"{domain}.unknown"
            result.add_output_path(path, p_domain)
            result[domain][i] = p_config
            if not isinstance(p_config, dict):
                result.add_str_error("Platform schemas must be key-value pairs.", path)
                continue
            p_name = p_config.get("platform")
            if p_name is None:
                result.add_str_error("No platform specified! See 'platform' key.", path)
                continue
            # Remove temp output path and construct new one
            result.remove_output_path(path, p_domain)
            p_domain = f"{domain}.{p_name}"
            result.add_output_path(path, p_domain)
            # Try Load platform
            platform = get_platform(domain, p_name)
            if platform is None:
                result.add_str_error(f"Platform not found: '{p_domain}'", path)
                continue
            CORE.loaded_integrations.add(p_name)

            # Process AUTO_LOAD
            for load in platform.auto_load:
                if load not in config:
                    load_conf = core.AutoLoad()
                    config[load] = load_conf
                    load_queue.append((load, load_conf))

            check_queue.append((path, p_domain, p_config, platform))

    # 4. Validate component metadata, including
    # - Transformation (nullable, multi conf)
    # - Dependencies
    # - Conflicts
    # - Supported ESP Platform

    # List of items to proceed to next stage
    validate_queue = []  # type: List[Tuple[ConfigPath, ConfigType, ComponentManifest]]
    for path, domain, conf, comp in check_queue:
        if conf is None:
            result[domain] = conf = {}

        success = True
        for dependency in comp.dependencies:
            if dependency not in config:
                result.add_str_error(
                    "Component {} requires component {}" "".format(domain, dependency),
                    path,
                )
                success = False
        if not success:
            continue

        success = True
        for conflict in comp.conflicts_with:
            if conflict in config:
                result.add_str_error(
                    "Component {} cannot be used together with component {}"
                    "".format(domain, conflict),
                    path,
                )
                success = False
        if not success:
            continue

        if CORE.esp_platform not in comp.esp_platforms:
            result.add_str_error(
                "Component {} doesn't support {}.".format(domain, CORE.esp_platform),
                path,
            )
            continue

        if (
            not comp.is_platform_component
            and comp.config_schema is None
            and not isinstance(conf, core.AutoLoad)
        ):
            result.add_str_error(
                "Component {} cannot be loaded via YAML "
                "(no CONFIG_SCHEMA).".format(domain),
                path,
            )
            continue

        if comp.multi_conf:
            if not isinstance(conf, list):
                result[domain] = conf = [conf]
            if not isinstance(comp.multi_conf, bool) and len(conf) > comp.multi_conf:
                result.add_str_error(
                    "Component {} supports a maximum of {} "
                    "entries ({} found).".format(domain, comp.multi_conf, len(conf)),
                    path,
                )
                continue
            for i, part_conf in enumerate(conf):
                validate_queue.append((path + [i], part_conf, comp))
            continue

        validate_queue.append((path, conf, comp))

    # 5. Validate configuration schema
    for path, conf, comp in validate_queue:
        if comp.config_schema is None:
            continue
        with result.catch_error(path):
            if comp.is_platform:
                # Remove 'platform' key for validation
                input_conf = OrderedDict(conf)
                platform_val = input_conf.pop("platform")
                validated = comp.config_schema(input_conf)
                # Ensure result is OrderedDict so we can call move_to_end
                if not isinstance(validated, OrderedDict):
                    validated = OrderedDict(validated)
                validated["platform"] = platform_val
                validated.move_to_end("platform", last=False)
                result.set_by_path(path, validated)
            else:
                validated = comp.config_schema(conf)
                result.set_by_path(path, validated)

    # 6. If no validation errors, check IDs
    if not result.errors:
        # Only parse IDs if no validation error. Otherwise
        # user gets confusing messages
        do_id_pass(result)

    # 7. Final validation
    if not result.errors:
        # Inter - components validation
        token = fv.full_config.set(result)

        for path, _, comp in validate_queue:
            if comp.final_validate_schema is None:
                continue
            conf = result.get_nested_item(path)
            with result.catch_error(path):
                comp.final_validate_schema(conf)

        fv.full_config.reset(token)

    return result


def _nested_getitem(data, path):
    for item_index in path:
        try:
            data = data[item_index]
        except (KeyError, IndexError, TypeError):
            return None
    return data


def humanize_error(config, validation_error):
    validation_error = str(validation_error)
    m = re.match(
        r"^(.*?)\s*(?:for dictionary value )?@ data\[.*$", validation_error, re.DOTALL
    )
    if m is not None:
        validation_error = m.group(1)
    validation_error = validation_error.strip()
    if not validation_error.endswith("."):
        validation_error += "."
    return validation_error


def _get_parent_name(path, config):
    if not path:
        return "<root>"
    for domain_path, domain in config.output_paths:
        if _path_begins_with(path, domain_path):
            if len(path) > len(domain_path):
                # Sub-item
                break
            return domain
    return path[-1]


def _format_vol_invalid(ex, config):
    # type: (vol.Invalid, Config) -> str
    message = ""

    paren = _get_parent_name(ex.path[:-1], config)

    if isinstance(ex, ExtraKeysInvalid):
        if ex.candidates:
            message += "[{}] is an invalid option for [{}]. Did you mean {}?".format(
                ex.path[-1], paren, ", ".join(f"[{x}]" for x in ex.candidates)
            )
        else:
            message += "[{}] is an invalid option for [{}]. Please check the indentation.".format(
                ex.path[-1], paren
            )
    elif "extra keys not allowed" in str(ex):
        message += "[{}] is an invalid option for [{}].".format(ex.path[-1], paren)
    elif isinstance(ex, vol.RequiredFieldInvalid):
        if ex.msg == "required key not provided":
            message += "'{}' is a required option for [{}].".format(ex.path[-1], paren)
        else:
            # Required has set a custom error message
            message += ex.msg
    else:
        message += humanize_error(config, ex)

    return message


class InvalidYAMLError(EsphomeError):
    def __init__(self, base_exc):
        try:
            base = str(base_exc)
        except UnicodeDecodeError:
            base = repr(base_exc)
        message = f"Invalid YAML syntax:\n\n{base}"
        super().__init__(message)
        self.base_exc = base_exc


def _load_config(command_line_substitutions):
    try:
        config = yaml_util.load_yaml(CORE.config_path)
    except EsphomeError as e:
        raise InvalidYAMLError(e) from e
    CORE.raw_config = config

    try:
        result = validate_config(config, command_line_substitutions)
    except EsphomeError:
        raise
    except Exception:
        _LOGGER.error("Unexpected exception while reading configuration:")
        raise

    return result


def load_config(command_line_substitutions):
    try:
        return _load_config(command_line_substitutions)
    except vol.Invalid as err:
        raise EsphomeError(f"Error while parsing config: {err}") from err


def line_info(config, path, highlight=True):
    """Display line config source."""
    if not highlight:
        return None
    obj = config.get_deepest_document_range_for_path(path)
    if obj:
        mark = obj.start_mark
        source = "[source {}:{}]".format(mark.document, mark.line + 1)
        return color(Fore.CYAN, source)
    return "None"


def _print_on_next_line(obj):
    if isinstance(obj, (list, tuple, dict)):
        return True
    if isinstance(obj, str):
        return len(obj) > 80
    if isinstance(obj, core.Lambda):
        return len(obj.value) > 80
    return False


def dump_dict(config, path, at_root=True):
    # type: (Config, ConfigPath, bool) -> Tuple[str, bool]
    conf = config.get_nested_item(path)
    ret = ""
    multiline = False

    if at_root:
        error = config.get_error_for_path(path)
        if error is not None:
            ret += (
                "\n" + color(Fore.BOLD_RED, _format_vol_invalid(error, config)) + "\n"
            )

    if isinstance(conf, (list, tuple)):
        multiline = True
        if not conf:
            ret += "[]"
            multiline = False

        for i in range(len(conf)):
            path_ = path + [i]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += (
                    "\n"
                    + color(Fore.BOLD_RED, _format_vol_invalid(error, config))
                    + "\n"
                )

            sep = "- "
            if config.is_in_error_path(path_):
                sep = color(Fore.RED, sep)
            msg, _ = dump_dict(config, path_, at_root=False)
            msg = indent(msg)
            inf = line_info(config, path_, highlight=config.is_in_error_path(path_))
            if inf is not None:
                msg = inf + "\n" + msg
            elif msg:
                msg = msg[2:]
            ret += sep + msg + "\n"
    elif isinstance(conf, dict):
        multiline = True
        if not conf:
            ret += "{}"
            multiline = False

        for k in conf.keys():
            path_ = path + [k]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += (
                    "\n"
                    + color(Fore.BOLD_RED, _format_vol_invalid(error, config))
                    + "\n"
                )

            st = f"{k}: "
            if config.is_in_error_path(path_):
                st = color(Fore.RED, st)
            msg, m = dump_dict(config, path_, at_root=False)

            inf = line_info(config, path_, highlight=config.is_in_error_path(path_))
            if m:
                msg = "\n" + indent(msg)

            if inf is not None:
                if m:
                    msg = " " + inf + msg
                else:
                    msg = msg + " " + inf
            ret += st + msg + "\n"
    elif isinstance(conf, str):
        if is_secret(conf):
            conf = "!secret {}".format(is_secret(conf))
        if not conf:
            conf += "''"

        if len(conf) > 80:
            conf = "|-\n" + indent(conf)
        error = config.get_error_for_path(path)
        col = Fore.BOLD_RED if error else Fore.KEEP
        ret += color(col, str(conf))
    elif isinstance(conf, core.Lambda):
        if is_secret(conf):
            conf = "!secret {}".format(is_secret(conf))

        conf = "!lambda |-\n" + indent(str(conf.value))
        error = config.get_error_for_path(path)
        col = Fore.BOLD_RED if error else Fore.KEEP
        ret += color(col, conf)
    elif conf is None:
        pass
    else:
        error = config.get_error_for_path(path)
        col = Fore.BOLD_RED if error else Fore.KEEP
        ret += color(col, str(conf))
        multiline = "\n" in ret

    return ret, multiline


def strip_default_ids(config):
    if isinstance(config, list):
        to_remove = []
        for i, x in enumerate(config):
            x = config[i] = strip_default_ids(x)
            if (isinstance(x, core.ID) and not x.is_manual) or isinstance(
                x, core.AutoLoad
            ):
                to_remove.append(x)
        for x in to_remove:
            config.remove(x)
    elif isinstance(config, dict):
        to_remove = []
        for k, v in config.items():
            v = config[k] = strip_default_ids(v)
            if (isinstance(v, core.ID) and not v.is_manual) or isinstance(
                v, core.AutoLoad
            ):
                to_remove.append(k)
        for k in to_remove:
            config.pop(k)
    return config


def read_config(command_line_substitutions):
    _LOGGER.info("Reading configuration %s...", CORE.config_path)
    try:
        res = load_config(command_line_substitutions)
    except EsphomeError as err:
        _LOGGER.error("Error while reading config: %s", err)
        return None
    if res.errors:
        if not CORE.verbose:
            res = strip_default_ids(res)

        safe_print(color(Fore.BOLD_RED, "Failed config"))
        safe_print("")
        for path, domain in res.output_paths:
            if not res.is_in_error_path(path):
                continue

            errstr = color(Fore.BOLD_RED, f"{domain}:")
            errline = line_info(res, path)
            if errline:
                errstr += " " + errline
            safe_print(errstr)
            safe_print(indent(dump_dict(res, path)[0]))
        return None
    return OrderedDict(res)

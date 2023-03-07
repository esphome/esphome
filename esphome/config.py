import abc
import functools
import heapq
import logging
import re

from typing import Optional, Union

from contextlib import contextmanager

import voluptuous as vol

from esphome import core, yaml_util, loader
import esphome.core.config as core_config
from esphome.const import (
    CONF_ESPHOME,
    CONF_ID,
    CONF_PLATFORM,
    CONF_PACKAGES,
    CONF_SUBSTITUTIONS,
    CONF_EXTERNAL_COMPONENTS,
    TARGET_PLATFORMS,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import indent
from esphome.util import safe_print, OrderedDict

from esphome.config_helpers import Extend
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
                p_name = f"{domain}.{p_config[CONF_PLATFORM]}"
                platform = get_platform(domain, p_config[CONF_PLATFORM])
                yield p_name, platform, p_config


ConfigPath = list[Union[str, int]]


def _path_begins_with(path: ConfigPath, other: ConfigPath) -> bool:
    if len(path) < len(other):
        return False
    return path[: len(other)] == other


@functools.total_ordering
class _ValidationStepTask:
    def __init__(self, priority: float, id_number: int, step: "ConfigValidationStep"):
        self.priority = priority
        self.id_number = id_number
        self.step = step

    @property
    def _cmp_tuple(self) -> tuple[float, int]:
        return (-self.priority, self.id_number)

    def __eq__(self, other):
        return self._cmp_tuple == other._cmp_tuple

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        return self._cmp_tuple < other._cmp_tuple


class Config(OrderedDict, fv.FinalValidateConfig):
    def __init__(self):
        super().__init__()
        # A list of voluptuous errors
        self.errors: list[vol.Invalid] = []
        # A list of paths that should be fully outputted
        # The values will be the paths to all "domain", for example (['logger'], 'logger')
        # or (['sensor', 'ultrasonic'], 'sensor.ultrasonic')
        self.output_paths: list[tuple[ConfigPath, str]] = []
        # A list of components ids with the config path
        self.declare_ids: list[tuple[core.ID, ConfigPath]] = []
        self._data = {}
        # Store pending validation tasks (in heap order)
        self._validation_tasks: list[_ValidationStepTask] = []
        # ID to ensure stable order for keys with equal priority
        self._validation_tasks_id = 0

    def add_error(self, error: vol.Invalid) -> None:
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

    def add_validation_step(self, step: "ConfigValidationStep"):
        id_num = self._validation_tasks_id
        self._validation_tasks_id += 1
        heapq.heappush(
            self._validation_tasks, _ValidationStepTask(step.priority, id_num, step)
        )

    def run_validation_steps(self):
        while self._validation_tasks:
            task = heapq.heappop(self._validation_tasks)
            task.step.run(self)

    @contextmanager
    def catch_error(self, path=None):
        path = path or []
        try:
            yield
        except vol.Invalid as e:
            e.prepend(path)
            self.add_error(e)

    def add_str_error(self, message: str, path: ConfigPath) -> None:
        self.add_error(vol.Invalid(message, path))

    def add_output_path(self, path: ConfigPath, domain: str) -> None:
        self.output_paths.append((path, domain))

    def remove_output_path(self, path: ConfigPath, domain: str) -> None:
        self.output_paths.remove((path, domain))

    def is_in_error_path(self, path: ConfigPath) -> bool:
        for err in self.errors:
            if _path_begins_with(err.path, path):
                return True
        return False

    def set_by_path(self, path, value):
        conf = self
        for key in path[:-1]:
            conf = conf[key]
        conf[path[-1]] = value

    def get_error_for_path(self, path: ConfigPath) -> Optional[vol.Invalid]:
        for err in self.errors:
            if self.get_deepest_path(err.path) == path:
                self.errors.remove(err)
                return err
        return None

    def get_deepest_document_range_for_path(
        self, path: ConfigPath, get_key: bool = False
    ) -> Optional[ESPHomeDataBase]:
        data = self
        doc_range = None
        for index, path_item in enumerate(path):
            try:
                if path_item in data:
                    key_data = [x for x in data.keys() if x == path_item][0]
                    if isinstance(key_data, ESPHomeDataBase):
                        doc_range = key_data.esp_range
                        if get_key and index == len(path) - 1:
                            return doc_range
                data = data[path_item]
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

    def get_deepest_path(self, path: ConfigPath) -> ConfigPath:
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
            if isinstance(key, core.ID):
                yield key, path
            yield from iter_ids(value, path + [key])


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


class ConfigValidationStep(abc.ABC):
    """A step to for the validation phase."""

    # Priority of this step, higher means run earlier
    priority: float = 0.0

    @abc.abstractmethod
    def run(self, result: Config) -> None:
        ...


class LoadValidationStep(ConfigValidationStep):
    """Load step, this step is called once for each domain config fragment.

    Responsibilities:
    - Load component code
    - Ensure all AUTO_LOADs are added
    - Set output paths of result
    """

    def __init__(self, domain: str, conf: ConfigType):
        self.domain = domain
        self.conf = conf

    def run(self, result: Config) -> None:
        if self.domain.startswith("."):
            # Ignore top-level keys starting with a dot
            return
        result.add_output_path([self.domain], self.domain)
        result[self.domain] = self.conf
        component = get_component(self.domain)
        path = [self.domain]
        if component is None:
            result.add_str_error(f"Component not found: {self.domain}", path)
            return
        CORE.loaded_integrations.add(self.domain)

        # Process AUTO_LOAD
        for load in component.auto_load:
            if load not in result:
                result.add_validation_step(AutoLoadValidationStep(load))

        if not component.is_platform_component:
            result.add_validation_step(
                MetadataValidationStep([self.domain], self.domain, self.conf, component)
            )
            return

        # This is a platform component, proceed to reading platform entries
        # Remove this is as an output path
        result.remove_output_path([self.domain], self.domain)

        # Ensure conf is a list
        if not self.conf:
            result[self.domain] = self.conf = []
        elif not isinstance(self.conf, list):
            result[self.domain] = self.conf = [self.conf]

        for i, p_config in enumerate(self.conf):
            path = [self.domain, i]
            # Construct temporary unknown output path
            p_domain = f"{self.domain}.unknown"
            result.add_output_path(path, p_domain)
            result[self.domain][i] = p_config
            if not isinstance(p_config, dict):
                result.add_str_error("Platform schemas must be key-value pairs.", path)
                continue
            p_name = p_config.get("platform")
            if p_name is None:
                p_id = p_config.get(CONF_ID)
                if isinstance(p_id, Extend):
                    result.add_str_error(
                        f"Source for extension of ID '{p_id.value}' was not found.",
                        path + [CONF_ID],
                    )
                    continue
                result.add_str_error("No platform specified! See 'platform' key.", path)
                continue
            # Remove temp output path and construct new one
            result.remove_output_path(path, p_domain)
            p_domain = f"{self.domain}.{p_name}"
            result.add_output_path(path, p_domain)
            # Try Load platform
            platform = get_platform(self.domain, p_name)
            if platform is None:
                result.add_str_error(f"Platform not found: '{p_domain}'", path)
                continue
            CORE.loaded_integrations.add(p_name)

            # Process AUTO_LOAD
            for load in platform.auto_load:
                if load not in result:
                    result.add_validation_step(AutoLoadValidationStep(load))

            result.add_validation_step(
                MetadataValidationStep(path, p_domain, p_config, platform)
            )


class AutoLoadValidationStep(ConfigValidationStep):
    """Auto load step. This step is used to automatically load components if
    a component requested that with AUTO_LOAD.
    """

    # Only load after all regular loads have taken place
    priority = -1.0

    def __init__(self, domain: str):
        self.domain = domain

    def run(self, result: Config) -> None:
        if self.domain in result:
            # already loaded
            return
        result.add_validation_step(LoadValidationStep(self.domain, core.AutoLoad()))


class MetadataValidationStep(ConfigValidationStep):
    """Validate component metadata

    Responsibilties:
     - Config transformation (nullable, multi conf)
     - Check dependencies
     - Check conflicts
     - Check supported target platforms
    """

    # All components need to be loaded first to ensure dependency check works
    priority = -2.0

    def __init__(
        self,
        path: ConfigPath,
        domain: str,
        conf: ConfigType,
        component: ComponentManifest,
    ) -> None:
        self.path = path
        self.domain = domain
        self.conf = conf
        self.comp = component

    def run(self, result: Config) -> None:
        if self.conf is None:
            result[self.domain] = self.conf = {}

        success = True
        for dependency in self.comp.dependencies:
            if dependency not in result:
                result.add_str_error(
                    f"Component {self.domain} requires component {dependency}",
                    self.path,
                )
                success = False
        if not success:
            return

        success = True
        for conflict in self.comp.conflicts_with:
            if conflict in result:
                result.add_str_error(
                    f"Component {self.domain} cannot be used together with component {conflict}",
                    self.path,
                )
                success = False
        if not success:
            return

        if (
            not self.comp.is_platform_component
            and self.comp.config_schema is None
            and not isinstance(self.conf, core.AutoLoad)
        ):
            result.add_str_error(
                f"Component {self.domain} cannot be loaded via YAML "
                "(no CONFIG_SCHEMA).",
                self.path,
            )
            return

        if self.comp.multi_conf:
            if not isinstance(self.conf, list):
                result[self.domain] = self.conf = [self.conf]
            if (
                not isinstance(self.comp.multi_conf, bool)
                and len(self.conf) > self.comp.multi_conf
            ):
                result.add_str_error(
                    f"Component {self.domain} supports a maximum of {self.comp.multi_conf} "
                    f"entries ({len(self.conf)} found).",
                    self.path,
                )
                return
            for i, part_conf in enumerate(self.conf):
                result.add_validation_step(
                    SchemaValidationStep(
                        self.domain, self.path + [i], part_conf, self.comp
                    )
                )
            return

        result.add_validation_step(
            SchemaValidationStep(self.domain, self.path, self.conf, self.comp)
        )


class SchemaValidationStep(ConfigValidationStep):
    """Schema validation step.

    During this step all CONFIG_SCHEMAs are checked against the configs.
    """

    def __init__(
        self, domain: str, path: ConfigPath, conf: ConfigType, comp: ComponentManifest
    ):
        self.path = path
        self.conf = conf
        self.comp = comp

    def run(self, result: Config) -> None:
        if self.comp.config_schema is None:
            return
        with result.catch_error(self.path):
            if self.comp.is_platform:
                # Remove 'platform' key for validation
                input_conf = OrderedDict(self.conf)
                platform_val = input_conf.pop("platform")
                schema = cv.Schema(self.comp.config_schema)
                validated = schema(input_conf)
                # Ensure result is OrderedDict so we can call move_to_end
                if not isinstance(validated, OrderedDict):
                    validated = OrderedDict(validated)
                validated["platform"] = platform_val
                validated.move_to_end("platform", last=False)
                result.set_by_path(self.path, validated)
            else:
                schema = cv.Schema(self.comp.config_schema)
                validated = schema(self.conf)
                result.set_by_path(self.path, validated)

        result.add_validation_step(FinalValidateValidationStep(self.path, self.comp))


class IDPassValidationStep(ConfigValidationStep):
    """ID Pass step.

    During this step all ID references are checked.

    If an automatic ID reference is used, a fitting declared ID is automatically searched.
    Also checks duplicate ID names, and that referenced IDs are declared.
    """

    # Has to happen after all schemas validated
    priority = -10.0

    def __init__(self) -> None:
        pass

    def run(self, result: Config) -> None:
        from esphome.cpp_generator import MockObjClass
        from esphome.cpp_types import Component

        if result.errors:
            # If result already has errors, skip this step
            # Otherwise the user will get a bunch of missing ID warnings
            # because the component that did not validate doesn't have any IDs set
            return

        searching_ids: list[tuple[core.ID, ConfigPath]] = []
        for id, path in iter_ids(result):
            if id.is_declaration:
                if id.id is not None:
                    # Look for duplicate definitions
                    match = next(
                        (v for v in result.declare_ids if v[0].id == id.id), None
                    )
                    if match is not None:
                        opath = "->".join(str(v) for v in match[1])
                        result.add_str_error(
                            f"ID {id.id} redefined! Check {opath}", path
                        )
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
                match = next(
                    (v[0] for v in result.declare_ids if v[0].id == id.id), None
                )
                if match is None or not match.is_manual:
                    # No declared ID with this name
                    import difflib

                    error = (
                        f"Couldn't find ID '{id.id}'. Please check you have defined "
                        "an ID with that name in your configuration."
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
                        f"ID '{id.id}' of type {match.type} doesn't inherit from {id.type}. "
                        "Please double check your ID is pointing to the correct value",
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
                            ids = ", ".join(
                                [f"'{m.id}'" for m in matches if m.is_manual]
                            )
                            result.add_str_error(
                                f"Too many candidates found for '{path[-1]}' type '{id.type}' {'Some are' if manual_declared_count > 1 else 'One is'} {ids}",
                                path,
                            )
                        else:
                            result.add_str_error(
                                f"Too many candidates found for '{path[-1]}' type '{id.type}' You must assign an explicit ID to the parent component you want to use.",
                                path,
                            )


class FinalValidateValidationStep(ConfigValidationStep):
    """Run final_validate_schema for all components."""

    # Has to happen after ID pass validated
    priority = -20.0

    def __init__(self, path: ConfigPath, comp: ComponentManifest) -> None:
        self.path = path
        self.comp = comp

    def run(self, result: Config) -> None:
        if result.errors:
            # If result already has errors, skip this step
            return

        if self.comp.final_validate_schema is None:
            return

        token = fv.full_config.set(result)

        conf = result.get_nested_item(self.path)
        with result.catch_error(self.path):
            self.comp.final_validate_schema(conf)

        fv.full_config.reset(token)


def validate_config(config, command_line_substitutions) -> Config:
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

    CORE.raw_config = config

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
            substitutions.do_substitution_pass(config, command_line_substitutions)
        except vol.Invalid as err:
            result.add_error(err)
            return result

    CORE.raw_config = config

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
        core_config.preload_core_config(config, result)
    except vol.Invalid as err:
        result.add_error(err)
        return result
    # Remove temporary esphome config path again, it will be reloaded later
    result.remove_output_path([CONF_ESPHOME], CONF_ESPHOME)

    # First run platform validation steps
    for key in TARGET_PLATFORMS:
        if key in config:
            result.add_validation_step(LoadValidationStep(key, config[key]))
    result.run_validation_steps()

    if result.errors:
        # do not try to validate further as we don't know what the target is
        return result

    for domain, conf in config.items():
        result.add_validation_step(LoadValidationStep(domain, conf))
    result.add_validation_step(IDPassValidationStep())

    result.run_validation_steps()

    return result


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


def _format_vol_invalid(ex: vol.Invalid, config: Config) -> str:
    message = ""

    paren = _get_parent_name(ex.path[:-1], config)

    if isinstance(ex, ExtraKeysInvalid):
        if ex.candidates:
            message += f"[{ex.path[-1]}] is an invalid option for [{paren}]. Did you mean {', '.join(f'[{x}]' for x in ex.candidates)}?"
        else:
            message += f"[{ex.path[-1]}] is an invalid option for [{paren}]. Please check the indentation."
    elif "extra keys not allowed" in str(ex):
        message += f"[{ex.path[-1]}] is an invalid option for [{paren}]."
    elif isinstance(ex, vol.RequiredFieldInvalid):
        if ex.msg == "required key not provided":
            message += f"'{ex.path[-1]}' is a required option for [{paren}]."
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
        source = f"[source {mark.document}:{mark.line + 1}]"
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


def dump_dict(
    config: Config, path: ConfigPath, at_root: bool = True
) -> tuple[str, bool]:
    conf = config.get_nested_item(path)
    ret = ""
    multiline = False

    if at_root:
        error = config.get_error_for_path(path)
        if error is not None:
            ret += f"\n{color(Fore.BOLD_RED, _format_vol_invalid(error, config))}\n"

    if isinstance(conf, (list, tuple)):
        multiline = True
        if not conf:
            ret += "[]"
            multiline = False

        for i in range(len(conf)):
            path_ = path + [i]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += f"\n{color(Fore.BOLD_RED, _format_vol_invalid(error, config))}\n"

            sep = "- "
            if config.is_in_error_path(path_):
                sep = color(Fore.RED, sep)
            msg, _ = dump_dict(config, path_, at_root=False)
            msg = indent(msg)
            inf = line_info(config, path_, highlight=config.is_in_error_path(path_))
            if inf is not None:
                msg = f"{inf}\n{msg}"
            elif msg:
                msg = msg[2:]
            ret += f"{sep + msg}\n"
    elif isinstance(conf, dict):
        multiline = True
        if not conf:
            ret += "{}"
            multiline = False

        for k in conf.keys():
            path_ = path + [k]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += f"\n{color(Fore.BOLD_RED, _format_vol_invalid(error, config))}\n"

            st = f"{k}: "
            if config.is_in_error_path(path_):
                st = color(Fore.RED, st)
            msg, m = dump_dict(config, path_, at_root=False)

            inf = line_info(config, path_, highlight=config.is_in_error_path(path_))
            if m:
                msg = f"\n{indent(msg)}"

            if inf is not None:
                if m:
                    msg = f" {inf}{msg}"
                else:
                    msg = f"{msg} {inf}"
            ret += f"{st + msg}\n"
    elif isinstance(conf, str):
        if is_secret(conf):
            conf = f"!secret {is_secret(conf)}"
        if not conf:
            conf += "''"

        if len(conf) > 80:
            conf = f"|-\n{indent(conf)}"
        error = config.get_error_for_path(path)
        col = Fore.BOLD_RED if error else Fore.KEEP
        ret += color(col, str(conf))
    elif isinstance(conf, core.Lambda):
        if is_secret(conf):
            conf = f"!secret {is_secret(conf)}"

        conf = f"!lambda |-\n{indent(str(conf.value))}"
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
                errstr += f" {errline}"
            safe_print(errstr)
            safe_print(indent(dump_dict(res, path)[0]))

        for err in res.errors:
            safe_print(color(Fore.BOLD_RED, err.msg))
            safe_print("")

        return None
    return OrderedDict(res)

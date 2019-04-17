from __future__ import print_function

import collections
import importlib
import logging
import re
import os.path

# pylint: disable=unused-import, wrong-import-order
from typing import List, Optional, Tuple, Union  # noqa

import voluptuous as vol

from esphome import core, core_config, yaml_util
from esphome.components import substitutions
from esphome.components.substitutions import CONF_SUBSTITUTIONS
from esphome.const import CONF_ESPHOME, CONF_PLATFORM, ESP_PLATFORMS
from esphome.core import CORE, ConfigType, EsphomeError  # noqa
from esphome.helpers import color, indent
from esphome.py_compat import text_type
from esphome.util import safe_print, OrderedDict

# pylint: disable=unused-import, wrong-import-order
from typing import List, Optional, Tuple, Union  # noqa
from esphome.core import ConfigType  # noqa
from esphome.yaml_util import is_secret
from esphome.voluptuous_schema import ExtraKeysInvalid

_LOGGER = logging.getLogger(__name__)

_COMPONENT_CACHE = {}


class ComponentManifest(object):
    def __init__(self, module, base_components_path, is_core=False, is_platform=False):
        self.module = module
        self._is_core = is_core
        self.is_platform = is_platform
        self.base_components_path = base_components_path

    @property
    def is_platform_component(self):
        return getattr(self.module, 'IS_PLATFORM_COMPONENT', False)

    @property
    def config_schema(self):
        return getattr(self.module, 'CONFIG_SCHEMA', None)

    @property
    def is_multi_conf(self):
        return getattr(self.module, 'MULTI_CONF', False)

    @property
    def to_code(self):
        return getattr(self.module, 'to_code', None)

    @property
    def esp_platforms(self):
        return getattr(self.module, 'ESP_PLATFORMS', ESP_PLATFORMS)

    @property
    def dependencies(self):
        return getattr(self.module, 'DEPENDENCIES', [])

    @property
    def conflicts_with(self):
        return getattr(self.module, 'CONFLICTS_WITH', [])

    @property
    def auto_load(self):
        return getattr(self.module, 'AUTO_LOAD', [])

    @property
    def to_code_priority(self):
        return getattr(self.module, 'TO_CODE_PRIORITY', [])

    def _get_flags_set(self, name, config):
        if not hasattr(self.module, name):
            return set()
        obj = getattr(self.module, name)
        if callable(obj):
            obj = obj(config)
        if obj is None:
            return set()
        if not isinstance(obj, (list, tuple, set)):
            obj = [obj]
        return set(obj)

    @property
    def source_files(self):
        if self._is_core:
            core_p = os.path.abspath(os.path.join(os.path.dirname(__file__), 'core'))
            source_files = core.find_source_files(os.path.join(core_p, 'dummy'))
            ret = {}
            for f in source_files:
                ret['esphome/core/{}'.format(f)] = os.path.join(core_p, f)
            return ret

        source_files = core.find_source_files(self.module.__file__)
        ret = {}
        # Make paths absolute
        directory = os.path.abspath(os.path.dirname(self.module.__file__))
        for x in source_files:
            full_file = os.path.join(directory, x)
            rel = os.path.relpath(full_file, self.base_components_path)
            # Always use / for C++ include names
            rel = rel.replace(os.sep, '/')
            target_file = 'esphome/components/{}'.format(rel)
            ret[target_file] = full_file
        return ret


CORE_COMPONENTS_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), 'components'))


def _lookup_module(domain, is_platform):
    if domain in _COMPONENT_CACHE:
        return _COMPONENT_CACHE[domain]

    path = 'esphome.components.{}'.format(domain)
    try:
        module = importlib.import_module(path)
    except ImportError:
        return None
    except Exception:  # pylint: disable=broad-except
        import traceback
        _LOGGER.error("Unable to load component %s:", domain)
        traceback.print_exc()
        return None
    else:
        manif = ComponentManifest(module, CORE_COMPONENTS_PATH, is_platform=is_platform)
        _COMPONENT_CACHE[domain] = manif
        return manif


def get_component(domain):
    assert '.' not in domain
    return _lookup_module(domain, False)


def get_platform(domain, platform):
    full = '{}.{}'.format(platform, domain)
    return _lookup_module(full, True)


_COMPONENT_CACHE['esphome'] = ComponentManifest(
    core_config, CORE_COMPONENTS_PATH, is_core=True, is_platform=False,
)


def iter_components(config):
    for domain, conf in config.items():
        component = get_component(domain)
        if component.is_multi_conf:
            for conf_ in conf:
                yield domain, component, conf_
        else:
            yield domain, component, conf
        if component.is_platform_component:
            for p_config in conf:
                p_name = u"{}.{}".format(domain, p_config[CONF_PLATFORM])
                platform = get_platform(domain, p_config[CONF_PLATFORM])
                yield p_name, platform, p_config


ConfigPath = List[Union[str, int]]


def _path_begins_with(path, other):  # type: (ConfigPath, ConfigPath) -> bool
    if len(path) < len(other):
        return False
    return path[:len(other)] == other


class Config(OrderedDict):
    def __init__(self):
        super(Config, self).__init__()
        # A list of voluptuous errors
        self.errors = []  # type: List[vol.Invalid]
        # A list of paths that should be fully outputted
        # The values will be the paths to all "domain", for example (['logger'], 'logger')
        # or (['sensor', 'ultrasonic'], 'sensor.ultrasonic')
        self.output_paths = []  # type: List[Tuple[ConfigPath, unicode]]

    def add_error(self, error):
        # type: (vol.Invalid) -> None
        if isinstance(error, vol.MultipleInvalid):
            for err in error.errors:
                self.add_error(err)
            return
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
        # type: (basestring, ConfigPath) -> None
        self.add_error(vol.Invalid(message, path))

    def add_output_path(self, path, domain):
        # type: (ConfigPath, unicode) -> None
        self.output_paths.append((path, domain))

    def remove_output_path(self, path, domain):
        # type: (ConfigPath, unicode) -> None
        self.output_paths.remove((path, domain))

    def is_in_error_path(self, path):
        # type: (ConfigPath) -> bool
        for err in self.errors:
            if _path_begins_with(err.path, path):
                return True
        return False

    def get_error_for_path(self, path):
        # type: (ConfigPath) -> Optional[vol.Invalid]
        for err in self.errors:
            if self.get_deepest_path(err.path) == path:
                return err
        return None

    def get_nested_item(self, path):
        # type: (ConfigPath) -> ConfigType
        data = self
        for item_index in path:
            try:
                data = data[item_index]
            except (KeyError, IndexError, TypeError):
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


def iter_ids(config, path=None):
    path = path or []
    if isinstance(config, core.ID):
        yield config, path
    elif isinstance(config, core.Lambda):
        for id in config.requires_ids:
            yield id, path
    elif isinstance(config, list):
        for i, item in enumerate(config):
            for result in iter_ids(item, path + [i]):
                yield result
    elif isinstance(config, dict):
        for key, value in config.items():
            for result in iter_ids(value, path + [key]):
                yield result


def do_id_pass(result):  # type: (Config) -> None
    from esphome.cpp_generator import MockObjClass

    declare_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
    searching_ids = []  # type: List[Tuple[core.ID, ConfigPath]]
    for id, path in iter_ids(result):
        if id.is_declaration:
            if id.id is not None and any(v[0].id == id.id for v in declare_ids):
                result.add_str_error(u"ID {} redefined!".format(id.id), path)
                continue
            declare_ids.append((id, path))
        else:
            searching_ids.append((id, path))
    # Resolve default ids after manual IDs
    for id, _ in declare_ids:
        id.resolve([v[0].id for v in declare_ids])

    # Check searched IDs
    for id, path in searching_ids:
        if id.id is not None:
            # manually declared
            match = next((v[0] for v in declare_ids if v[0].id == id.id), None)
            if match is None:
                # No declared ID with this name
                result.add_str_error("Couldn't find ID '{}'".format(id.id), path)
                continue
            if not isinstance(match.type, MockObjClass) or not isinstance(id.type, MockObjClass):
                continue
            if not match.type.inherits_from(id.type):
                result.add_str_error("ID '{}' of type {} doesn't inherit from {}. Please "
                                     "double check your ID is pointing to the correct value"
                                     "".format(id.id, match.type, id.type), path)

        if id.id is None and id.type is not None:
            for v in declare_ids:
                if v[0] is None or not isinstance(v[0].type, MockObjClass):
                    continue
                inherits = v[0].type.inherits_from(id.type)
                if inherits:
                    id.id = v[0].id
                    break
            else:
                result.add_str_error("Couldn't resolve ID for type '{}'".format(id.type), path)


def validate_config(config):
    result = Config()

    # 1. Load substitutions
    if CONF_SUBSTITUTIONS in config:
        result[CONF_SUBSTITUTIONS] = config[CONF_SUBSTITUTIONS]
        result.add_output_path([CONF_SUBSTITUTIONS], CONF_SUBSTITUTIONS)
        try:
            substitutions.do_substitution_pass(config)
        except vol.Invalid as err:
            result.add_error(err)
            return result

    # 2. Load partial core config
    result[CONF_ESPHOME] = config[CONF_ESPHOME]
    result.add_output_path([CONF_ESPHOME], CONF_ESPHOME)
    try:
        core_config.preload_core_config(config)
    except vol.Invalid as err:
        result.add_error(err)
        return result

    # 3. Load components. When a component cannot be loaded, it is added to 'skip_paths'
    # a list of paths for which no further voluptuous validation will be performed
    skip_paths = list()  # type: List[ConfigPath]

    # Step 1: Load everything
    result.add_domain([CONF_ESPHOME], CONF_ESPHOME)
    result[CONF_ESPHOME] = config[CONF_ESPHOME]
    config_queue = collections.deque()
    for domain, conf in config.items():
        config_queue.append((domain, conf))

    while config_queue:
        domain, conf = config_queue.popleft()
        domain = str(domain)
        if domain == CONF_ESPHOME or domain.startswith(u'.'):
            # Ignore top-level keys starting with a dot
            # Also ignore 'esphome' it is handled separately
            skip_paths.append([domain])
            continue
        result.add_output_path([domain], domain)
        if conf is None:
            conf = {}
        result[domain] = conf
        component = get_component(domain)
        if component is None:
            result.add_str_error(u"Component not found: {}".format(domain), [domain])
            skip_paths.append([domain])
            continue

        if component.is_multi_conf and not isinstance(conf, list):
            result[domain] = conf = [conf]

        success = True
        for dependency in component.dependencies:
            if dependency not in config:
                result.add_str_error(u"Component {} requires component {}"
                                     u"".format(domain, dependency), [domain])
                success = False
        if not success:
            skip_paths.append([domain])
            continue

        success = True
        for conflict in component.conflicts_with:
            if conflict in config:
                result.add_str_error(u"Component {} cannot be used together with component {}"
                                     u"".format(domain, conflict), [domain])
                success = False
        if not success:
            skip_paths.append([domain])
            continue

        for load in component.auto_load:
            if load not in config:
                conf = core.AutoLoad()
                config[load] = conf
                config_queue.append((load, conf))

        if CORE.esp_platform not in component.esp_platforms:
            result.add_str_error(u"Component {} doesn't support {}.".format(domain, CORE.esp_platform),
                                 [domain])
            skip_paths.append([domain])
            continue

        if not component.is_platform_component:
            if component.config_schema is None and not isinstance(conf, core.AutoLoad):
                result.add_error(u"Component {} cannot be loaded via YAML (no CONFIG_SCHEMA)."
                                 u"".format(domain), [domain])
                skip_paths.append([domain])
            continue

        result.remove_output_path([domain], domain)

        if not isinstance(conf, list) and conf:
            result[domain] = conf = [conf]

        for i, p_config in enumerate(conf):
            if not isinstance(p_config, dict):
                result.add_str_error(u"Platform schemas must have 'platform:' key", [domain, i])
                skip_paths.append([domain, i])
                continue
            p_name = p_config.get('platform')
            if p_name is None:
                result.add_str_error(u"No platform specified for {}".format(domain), [domain, i])
                skip_paths.append([domain, i])
                continue
            p_domain = u'{}.{}'.format(domain, p_name)
            result.add_output_path([domain, i], p_domain)
            platform = get_platform(domain, p_name)
            if platform is None:
                result.add_str_error(u"Platform not found: '{}'".format(p_domain), [domain, i])
                skip_paths.append([domain, i])
                continue

            success = True
            for dependency in platform.dependencies:
                if dependency not in config:
                    result.add_str_error(u"Platform {} requires component {}"
                                         u"".format(p_domain, dependency), [domain, i])
                    success = False
            if not success:
                skip_paths.append([domain, i])
                continue

            success = True
            for conflict in platform.conflicts_with:
                if conflict in config:
                    result.add_str_error(u"Platform {} cannot be used together with component {}"
                                         u"".format(p_domain, conflict), [domain, i])
                    success = False
            if not success:
                skip_paths.append([domain, i])
                continue

            for load in platform.auto_load:
                if load not in config:
                    conf = core.AutoLoad()
                    config[load] = conf
                    config_queue.append((load, conf))

            if CORE.esp_platform not in platform.esp_platforms:
                result.add_str_error(u"Platform {} doesn't support {}."
                                     u"".format(p_domain, CORE.esp_platform), [domain, i])
                skip_paths.append([domain, i])
                continue

            if platform.config_schema is None:
                result.add_error(u"Platform {} cannot be loaded via YAML (no PLATFORM_SCHEMA)."
                                 u"".format(p_domain), [domain, i])
                skip_paths.append([domain])

    # Step 2: Validate configuration
    with result.catch_error([CONF_ESPHOME]):
        result[CONF_ESPHOME] = core_config.CONFIG_SCHEMA(result[CONF_ESPHOME])

    for domain, conf in result.items():
        domain = str(domain)
        if [domain] in skip_paths:
            continue
        component = get_component(domain)

        if not component.is_platform_component:
            if component.config_schema is None:
                continue

            if component.is_multi_conf:
                for i, conf_ in enumerate(conf):
                    with result.catch_error([domain, i]):
                        validated = component.config_schema(conf_)
                        result[domain][i] = validated
            else:
                with result.catch_error([domain]):
                    validated = component.config_schema(conf)
                    result[domain] = validated
            continue

        for i, p_config in enumerate(conf):
            if [domain, i] in skip_paths:
                continue
            p_name = p_config['platform']
            platform = get_platform(domain, p_name)

            if platform.config_schema is not None:
                # Remove 'platform' key for validation
                input_conf = OrderedDict(p_config)
                platform_val = input_conf.pop('platform')
                with result.catch_error([domain, i]):
                    p_validated = platform.config_schema(input_conf)
                    if not isinstance(p_validated, OrderedDict):
                        p_validated = OrderedDict(p_validated)
                    p_validated['platform'] = platform_val
                    p_validated.move_to_end('platform', last=False)
                    result[domain][i] = p_validated

    # 5. If no validation errors, check IDs
    if not result.errors:
        # Only parse IDs if no validation error. Otherwise
        # user gets confusing messages
        do_id_pass(result)
    return result


def _nested_getitem(data, path):
    for item_index in path:
        try:
            data = data[item_index]
        except (KeyError, IndexError, TypeError):
            return None
    return data


def humanize_error(config, validation_error):
    offending_item_summary = _nested_getitem(config, validation_error.path)
    if isinstance(offending_item_summary, dict):
        offending_item_summary = None
    validation_error = text_type(validation_error)
    m = re.match(r'^(.*?)\s*(?:for dictionary value )?@ data\[.*$', validation_error)
    if m is not None:
        validation_error = m.group(1)
    validation_error = validation_error.strip()
    if not validation_error.endswith(u'.'):
        validation_error += u'.'
    if offending_item_summary is None or is_secret(offending_item_summary):
        return validation_error

    return u"{} Got '{}'".format(validation_error, offending_item_summary)


def _get_parent_name(path, config):
    if not path:
        return '<root>'
    for domain_path, domain in config.output_paths:
        if _path_begins_with(path, domain_path):
            if len(path) > len(domain_path):
                # Sub-item
                break
            return domain
    return path[-1]


def _format_vol_invalid(ex, config):
    # type: (vol.Invalid, Config) -> unicode
    message = u''

    paren = _get_parent_name(ex.path[:-1], config)

    if isinstance(ex, ExtraKeysInvalid):
        if ex.candidates:
            message += u'[{}] is an invalid option for [{}]. Did you mean {}?'.format(
                ex.path[-1], paren, u', '.join(u'[{}]'.format(x) for x in ex.candidates))
        else:
            message += u'[{}] is an invalid option for [{}]. Please check the indentation.'.format(
                ex.path[-1], paren)
    elif u'extra keys not allowed' in ex.error_message:
        message += u'[{}] is an invalid option for [{}].'.format(ex.path[-1], paren)
    elif u'required key not provided' in ex.error_message:
        message += u"'{}' is a required option for [{}].".format(ex.path[-1], paren)
    else:
        message += humanize_error(config, ex)

    return message


class InvalidYAMLError(EsphomeError):
    def __init__(self, path, base_exc):
        message = u"Invalid YAML at {}. Please see YAML syntax reference or use an " \
                  u"online YAML syntax validator. ({})".format(path, base_exc)
        super(EsphomeError, self).__init__(message)
        self.path = path
        self.base_exc = base_exc


def _load_config():
    try:
        config = yaml_util.load_yaml(CORE.config_path)
    except EsphomeError as e:
        raise InvalidYAMLError(CORE.config_path, e)
    CORE.raw_config = config

    try:
        result = validate_config(config)
    except EsphomeError:
        raise
    except Exception:
        _LOGGER.error(u"Unexpected exception while reading configuration:")
        raise

    return result


def load_config():
    try:
        return _load_config()
    except vol.Invalid as err:
        raise EsphomeError("Error while parsing config: {}".format(err))


def line_info(obj, highlight=True):
    """Display line config source."""
    if not highlight:
        return None
    if hasattr(obj, '__config_file__'):
        return color('cyan', "[source {}:{}]"
                     .format(obj.__config_file__, obj.__line__ or '?'))
    return None


def _print_on_next_line(obj):
    if isinstance(obj, (list, tuple, dict)):
        return True
    if isinstance(obj, str):
        return len(obj) > 80
    if isinstance(obj, core.Lambda):
        return len(obj.value) > 80
    return False


def dump_dict(config, path, at_root=True):
    # type: (Config, ConfigPath, bool) -> Tuple[unicode, bool]
    conf = config.get_nested_item(path)
    ret = u''
    multiline = False

    if at_root:
        error = config.get_error_for_path(path)
        if error is not None:
            ret += u'\n' + color('bold_red', _format_vol_invalid(error, config)) + u'\n'

    if isinstance(conf, (list, tuple)):
        multiline = True
        if not conf:
            ret += u'[]'
            multiline = False

        for i in range(len(conf)):
            path_ = path + [i]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += u'\n' + color('bold_red', _format_vol_invalid(error, config)) + u'\n'

            sep = u'- '
            if config.is_in_error_path(path_):
                sep = color('red', sep)
            msg, _ = dump_dict(config, path_, at_root=False)
            msg = indent(msg)
            inf = line_info(config.get_nested_item(path_), highlight=config.is_in_error_path(path_))
            if inf is not None:
                msg = inf + u'\n' + msg
            elif msg:
                msg = msg[2:]
            ret += sep + msg + u'\n'
    elif isinstance(conf, dict):
        multiline = True
        if not conf:
            ret += u'{}'
            multiline = False

        for k in conf.keys():
            path_ = path + [k]
            error = config.get_error_for_path(path_)
            if error is not None:
                ret += u'\n' + color('bold_red', _format_vol_invalid(error, config)) + u'\n'

            st = u'{}: '.format(k)
            if config.is_in_error_path(path_):
                st = color('red', st)
            msg, m = dump_dict(config, path_, at_root=False)

            inf = line_info(config.get_nested_item(path_), highlight=config.is_in_error_path(path_))
            if m:
                msg = u'\n' + indent(msg)

            if inf is not None:
                if m:
                    msg = u' ' + inf + msg
                else:
                    msg = msg + u' ' + inf
            ret += st + msg + u'\n'
    elif isinstance(conf, str):
        if is_secret(conf):
            conf = u'!secret {}'.format(is_secret(conf))
        if not conf:
            conf += u"''"

        if len(conf) > 80:
            conf = u'|-\n' + indent(conf)
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, text_type(conf))
    elif isinstance(conf, core.Lambda):
        if is_secret(conf):
            conf = u'!secret {}'.format(is_secret(conf))

        conf = u'!lambda |-\n' + indent(text_type(conf.value))
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, conf)
    elif conf is None:
        pass
    else:
        error = config.get_error_for_path(path)
        col = 'bold_red' if error else 'white'
        ret += color(col, text_type(conf))
        multiline = u'\n' in ret

    return ret, multiline


def strip_default_ids(config):
    if isinstance(config, list):
        to_remove = []
        for i, x in enumerate(config):
            x = config[i] = strip_default_ids(x)
            if (isinstance(x, core.ID) and not x.is_manual) or isinstance(x, core.AutoLoad):
                to_remove.append(x)
        for x in to_remove:
            config.remove(x)
    elif isinstance(config, dict):
        to_remove = []
        for k, v in config.items():
            v = config[k] = strip_default_ids(v)
            if (isinstance(v, core.ID) and not v.is_manual) or isinstance(v, core.AutoLoad):
                to_remove.append(k)
        for k in to_remove:
            config.pop(k)
    return config


def read_config(verbose):
    _LOGGER.info("Reading configuration...")
    try:
        res = load_config()
    except EsphomeError as err:
        _LOGGER.error(u"Error while reading config: %s", err)
        return None
    if res.errors:
        if not verbose:
            res = strip_default_ids(res)

        safe_print(color('bold_red', u"Failed config"))
        safe_print('')
        for path, domain in res.output_paths:
            if not res.is_in_error_path(path):
                continue

            safe_print(color('bold_red', u'{}:'.format(domain)) + u' ' +
                       (line_info(res.get_nested_item(path)) or u''))
            safe_print(indent(dump_dict(res, path)[0]))
        return None
    return OrderedDict(res)

import logging
import re

import voluptuous as vol

from esphome import core
import esphome.config_validation as cv
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

CONF_SUBSTITUTIONS = 'substitutions'

VALID_SUBSTITUTIONS_CHARACTERS = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ' \
                                 '0123456789_'


def validate_substitution_key(value):
    value = cv.string(value)
    if not value:
        raise vol.Invalid("Substitution key must not be empty")
    if value[0] == '$':
        value = value[1:]
    if value[0].isdigit():
        raise vol.Invalid("First character in substitutions cannot be a digit.")
    for char in value:
        if char not in VALID_SUBSTITUTIONS_CHARACTERS:
            raise vol.Invalid(
                u"Substitution must only consist of upper/lowercase characters, the underscore "
                u"and numbers. The character '{}' cannot be used".format(char))
    return value


CONFIG_SCHEMA = vol.Schema({
    validate_substitution_key: cv.string_strict,
})


def to_code(config):
    pass


VARIABLE_PROG = re.compile('\\$([{0}]+|\\{{[{0}]*\\}})'.format(VALID_SUBSTITUTIONS_CHARACTERS))


def _expand_substitutions(substitutions, value, path):
    if u'$' not in value:
        return value

    orig_value = value

    i = 0
    while True:
        m = VARIABLE_PROG.search(value, i)
        if not m:
            # Nothing more to match. Done
            break

        i, j = m.span(0)
        name = m.group(1)
        if name.startswith(u'{') and name.endswith(u'}'):
            name = name[1:-1]
        if name not in substitutions:
            _LOGGER.warning(u"Found '%s' (see %s) which looks like a substitution, but '%s' was "
                            u"not declared", orig_value, u'->'.join(str(x) for x in path), name)
            i = j
            continue

        sub = substitutions[name]
        tail = value[j:]
        value = value[:i] + sub
        i = len(value)
        value += tail
    return value


def _substitute_item(substitutions, item, path):
    if isinstance(item, list):
        for i, it in enumerate(item):
            sub = _substitute_item(substitutions, it, path + [i])
            if sub is not None:
                item[i] = sub
    elif isinstance(item, dict):
        replace_keys = []
        for k, v in item.items():
            if path or k != CONF_SUBSTITUTIONS:
                sub = _substitute_item(substitutions, k, path + [k])
                if sub is not None:
                    replace_keys.append((k, sub))
            sub = _substitute_item(substitutions, v, path + [k])
            if sub is not None:
                item[k] = sub
        for old, new in replace_keys:
            item[new] = item[old]
            del item[old]
    elif isinstance(item, str):
        sub = _expand_substitutions(substitutions, item, path)
        if sub != item:
            return sub
    elif isinstance(item, core.Lambda):
        sub = _expand_substitutions(substitutions, item.value, path)
        if sub != item:
            item.value = sub
    return None


def do_substitution_pass(config):
    if CONF_SUBSTITUTIONS not in config:
        return config

    substitutions = config[CONF_SUBSTITUTIONS]
    if not isinstance(substitutions, dict):
        raise EsphomeError(u"Substitutions must be a key to value mapping, got {}"
                           u"".format(type(substitutions)))

    key = ''
    try:
        replace_keys = []
        for key, value in substitutions.items():
            sub = validate_substitution_key(key)
            if sub != key:
                replace_keys.append((key, sub))
            substitutions[key] = cv.string_strict(value)
        for old, new in replace_keys:
            substitutions[new] = substitutions[old]
            del substitutions[old]
    except vol.Invalid as err:
        err.path.append(key)

        raise EsphomeError(u"Error while parsing substitutions: {}".format(err))

    config[CONF_SUBSTITUTIONS] = substitutions
    _substitute_item(substitutions, config, [])

    return config

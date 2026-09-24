"""Helpers to retrieve schema from voluptuous validators.

These are a helper decorators to help get schema from some
components which uses voluptuous in a way where validation
is hidden in local functions
These decorators should not modify at all what the functions
originally do.
However there is a property to further disable decorator
impact."""

# This is set to true by script/build_language_schema.py
# only, so data is collected (again functionality is not modified)
EnableSchemaExtraction = False

extended_schemas = {}
list_schemas = {}
registry_schemas = {}
hidden_schemas = {}
typed_schemas = {}

# This key is used to generate schema files of Esphome configuration.
SCHEMA_EXTRACT = object()


def schema_extractor(validator_name):
    if EnableSchemaExtraction:

        def decorator(func):
            hidden_schemas[repr(func)] = validator_name
            return func

        return decorator

    def dummy(f):
        return f

    return dummy


def schema_extractor_extended(func):
    if EnableSchemaExtraction:

        def decorate(*args, **kwargs):
            ret = func(*args, **kwargs)
            extended_schemas[repr(ret)] = args
            return ret

        return decorate

    return func


def schema_extractor_list(func):
    if EnableSchemaExtraction:

        def decorate(*args, **kwargs):
            ret = func(*args, **kwargs)
            # args length might be 2, but 2nd is always validator
            list_schemas[repr(ret)] = args
            return ret

        return decorate

    return func


def schema_extractor_registry(registry):
    if EnableSchemaExtraction:

        def decorator(func):
            registry_schemas[repr(func)] = registry
            return func

        return decorator

    def dummy(f):
        return f

    return dummy


def schema_extractor_typed(func):
    if EnableSchemaExtraction:

        def decorate(*args, **kwargs):
            ret = func(*args, **kwargs)
            typed_schemas[repr(ret)] = (args, kwargs)
            return ret

        return decorate

    return func

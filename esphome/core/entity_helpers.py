import esphome.final_validate as fv

from esphome.core import ID
from esphome.const import CONF_ID


def inherit_property_from(property_to_inherit, parent_id_property, transform=None):
    """Validator that inherits a configuration property from another entity, for use with FINAL_VALIDATE_SCHEMA.
    If a property is already set, it will not be inherited.
    Keyword arguments:
    property_to_inherit -- the name or path of the property to inherit, e.g. CONF_ICON or [CONF_SENSOR, 0, CONF_ICON]
                           (the parent must exist, otherwise nothing is done).
    parent_id_property -- the name or path of the property that holds the ID of the parent, e.g. CONF_POWER_ID or
                          [CONF_SENSOR, 1, CONF_POWER_ID].
    """

    def _walk_config(config, path):
        walk = [path] if not isinstance(path, list) else path
        for item_or_index in walk:
            config = config[item_or_index]
        return config

    def inherit_property(config):
        # Split the property into its path and name
        if not isinstance(property_to_inherit, list):
            property_path, property = [], property_to_inherit
        else:
            property_path, property = property_to_inherit[:-1], property_to_inherit[-1]

        # Check if the property to inherit is accessible
        try:
            config_part = _walk_config(config, property_path)
        except KeyError:
            return config

        # Only inherit the property if it does not exist yet
        if property not in config_part:
            fconf = fv.full_config.get()

            # Get config for the parent entity
            parent_id = _walk_config(config, parent_id_property)
            parent_path = fconf.get_path_for_id(parent_id)[:-1]
            parent_config = fconf.get_config_for_path(parent_path)

            # If parent sensor has the property set, inherit it
            if property in parent_config:
                path = fconf.get_path_for_id(config[CONF_ID])[:-1]
                this_config = _walk_config(
                    fconf.get_config_for_path(path), property_path
                )
                value = parent_config[property]
                if transform:
                    value = transform(value, config)
                this_config[property] = value

        return config

    return inherit_property


def count_id_usage(
    property_to_update,
    property_to_count,
    property_value,
    filter_duplicate: bool = False,
):
    """Validator that counts a configuration property from another entity, for use with FINAL_VALIDATE_SCHEMA.
    If a property is already set, it will not be updated.
    Keyword arguments:
    property_to_update -- The name of the property to set.
    property_to_count -- The config name of the properties to search in.
    property_value -- The core.ID value to search for.
    filter_duplicate -- Should a duplicate filter be used.
    """
    # A dict of seen id so far.
    seen_ids = {}

    def _walk_config(config, path):
        walk = [path] if not isinstance(path, list) else path
        for item_or_index in walk:
            if item_or_index not in config:
                config[item_or_index] = {}
            config = config[item_or_index]
        return config

    def _count_config_value(config, conf_name_list, conf_value):
        ret = 0

        if isinstance(config, (list, tuple)):
            for config_item in config:
                ret += _count_config_value(config_item, conf_name_list, conf_value)
            for conf_name in conf_name_list:
                # Is this a config name which I am looking for?
                if (
                    conf_name in config
                    and isinstance(config, (tuple))
                    and config[0] == conf_name
                ):
                    # Is the value a class of type ID and also of the type I am looking for?
                    if isinstance(config[1], (ID)) and config[1].type.inherits_from(
                        conf_value
                    ):
                        if filter_duplicate:
                            # Duplicate filter is active
                            if str(config[1]) not in seen_ids:
                                seen_ids[str(config[1])] = config[1]
                                ret += 1
                        else:
                            ret += 1

        elif isinstance(config, (dict)):
            for config_item in config.items():
                ret += _count_config_value(config_item, conf_name_list, conf_value)

        return ret

    def inherit_property(config):
        # Ensure `property_to_count` is a list
        property_to_count_list = (
            [property_to_count]
            if not isinstance(property_to_count, list)
            else property_to_count
        )

        # Split the property into its path and name
        if not isinstance(property_to_update, list):
            property_path, property = [], property_to_update
        else:
            property_path, property = property_to_update[:-1], property_to_update[-1]

        # Check if the property is accessible
        try:
            config_part = _walk_config(config, property_path)
        except KeyError:
            return config

        # Only update the property if it does not exist yet
        if property not in config_part:
            fconf = fv.full_config.get()

            # Perform recursive count operation
            count = _count_config_value(fconf, property_to_count_list, property_value)

            # Base component (e.g. App) does not have an `ID` and are updated directly. For other I must get the correct conf object.
            if CONF_ID in config:
                path = fconf.get_path_for_id(config[CONF_ID])[:-1]
                this_config = _walk_config(
                    fconf.get_config_for_path(path), property_path
                )
                this_config[property] = count
            else:
                config_part[property] = count
        return config

    return inherit_property

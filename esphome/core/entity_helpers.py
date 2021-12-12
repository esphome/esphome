import esphome.final_validate as fv

from esphome.const import CONF_ID


def inherit_property_from(property_to_inherit, parent_id_property):
    """Validator that inherits a configuration property from another entity, for use with FINAL_VALIDATE_SCHEMA.

    If a property is already set, it will not be inherited.

    Keyword arguments:
    property_to_inherit -- the name of the property to inherit, e.g. CONF_ICON
    parent_id_property -- the name of the property that holds the ID of the parent, e.g. CONF_POWER_ID
    """

    def inherit_property(config):
        if property_to_inherit not in config:
            fconf = fv.full_config.get()

            # Get config for the parent entity
            path = fconf.get_path_for_id(config[parent_id_property])[:-1]
            parent_config = fconf.get_config_for_path(path)

            # If parent sensor has the property set, inherit it
            if property_to_inherit in parent_config:
                path = fconf.get_path_for_id(config[CONF_ID])[:-1]
                this_config = fconf.get_config_for_path(path)
                this_config[property_to_inherit] = parent_config[property_to_inherit]

        return config

    return inherit_property

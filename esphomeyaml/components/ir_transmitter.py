import voluptuous as vol


def CONFIG_SCHEMA(config):
    raise vol.Invalid("The ir_transmitter component has been renamed to "
                      "remote_transmitter because of 433MHz signal support.")

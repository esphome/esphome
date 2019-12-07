import esphome.config_validation as cv
from esphome.components.remote_base import BINARY_SENSOR_REGISTRY
from esphome.util import OrderedDict


def show_new(value):
    from esphome import yaml_util
    for key in BINARY_SENSOR_REGISTRY:
        if key in value:
            break
    else:
        raise cv.Invalid("This platform has been removed in 1.13, please see the docs for updated "
                         "instructions.")

    val = value[key]
    args = [('platform', 'template')]
    if 'id' in value:
        args.append(('id', value['id']))
    if 'name' in value:
        args.append(('name', value['name']))
    args.append(('turn_on_action', {
        f'remote_transmitter.transmit_{key}': val
    }))

    text = yaml_util.dump([OrderedDict(args)])
    raise cv.Invalid("This platform has been removed in 1.13, please change to:\n\n{}\n\n."
                     "".format(text))


CONFIG_SCHEMA = show_new

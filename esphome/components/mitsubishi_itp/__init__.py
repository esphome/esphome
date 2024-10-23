import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@Sammy1Am", "@KazWolfe"]

mitsubishi_itp_ns = cg.esphome_ns.namespace("mitsubishi_itp")
MitsubishiUART = mitsubishi_itp_ns.class_(
    "MitsubishiUART", cg.PollingComponent, climate.Climate
)
CONF_MITSUBISHI_ITP_ID = "mitsubishi_itp_id"


def sensors_to_config_schema(sensors):
    return cv.Schema(
        {
            cv.GenerateID(CONF_MITSUBISHI_ITP_ID): cv.use_id(MitsubishiUART),
        }
    ).extend(
        {
            cv.Optional(sensor_designator): sensor_schema
            for sensor_designator, sensor_schema in sensors.items()
        }
    )


async def sensors_to_code(config, sensors, registration_function):
    mitp_component = await cg.get_variable(config[CONF_MITSUBISHI_ITP_ID])

    # Sensors

    for sensor_designator, _ in sensors.items():
        if sensor_conf := config.get(sensor_designator):
            sensor_component = cg.new_Pvariable(sensor_conf[CONF_ID])

            await registration_function(sensor_component, sensor_conf)

            cg.add(getattr(mitp_component, "register_listener")(sensor_component))

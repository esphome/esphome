import esphome.codegen as cg
from esphome.components import sensor, text_sensor, time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["time"]

cv_controller_ns = cg.esphome_ns.namespace("cv_controller")
CVController = cv_controller_ns.class_("CVController", cg.Component)

# input for the real time clock sensor
CONF_TIMEID = "time_id"  # id for the real time clock

# input for the output temperature sensor
CONF_OUTSIDE_TEMPERATURE = (
    "outside_temperature_sensor"  # outside temperature sensor HA id (string)
)

# input climate devices from HA
CONF_TEMP_SENSORS = "temperature_sensor_parameters"  # grouping of climate devices
CONF_ID_STRING = "id_string"  # climate sensors HA id (string)
CONF_ACCURACY = "high_accuracy"  # boolean true for high accuracy climate devices

# input time boxes from HA
CONF_TIME_INPUT = "time_input_boxes"  # grouping of time input boxes
CONF_INPUT_START = "start_time_id"  # start time HA id (string)
CONF_INPUT_STOP = "stop_time_id"  # stop time HA id (string)

# input for the PID controllers (float values)
CONF_CONTROL_PARAMETERS = "pid_control_parameters"  # grouping PID parameters
CONF_KP = "kp"
CONF_KP_LOW = "kp_low"
CONF_KI = "ki"
CONF_PI_MAX = "pi_max"
CONF_PI_TARGET = "pi_target"
CONF_CONTROL_BAND = "control_band"
CONF_TIME_INTERVAL = "time_interval"
CONF_START_OUTPUT = "start_output"

# output sensors
CONF_CONTROL_SENSOR = "cv_control_temperature"  # id for the control temperature sensor
CONF_CONTROL_NAME = "cv_controller_name"  # id for the active controller name
CONF_CV_STATUS = "cv_status"  # id for the actual cv status


device_schema = cv.Schema(
    {
        cv.Required(CONF_ID_STRING): cv.string,
        cv.Required(CONF_ACCURACY): cv.boolean,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CVController),
        cv.Required(CONF_TIMEID): cv.use_id(time_.RealTimeClock),
        cv.Required(CONF_OUTSIDE_TEMPERATURE): cv.string,
        cv.Required(CONF_CONTROL_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_CONTROL_NAME): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_CV_STATUS): cv.use_id(text_sensor.TextSensor),
        cv.Required(CONF_TEMP_SENSORS): cv.ensure_list(device_schema),
        cv.Required(CONF_CONTROL_PARAMETERS): cv.Schema(
            {
                cv.Required(CONF_KP): cv.float_,
                cv.Required(CONF_KP_LOW): cv.float_,
                cv.Required(CONF_KI): cv.float_,
                cv.Required(CONF_PI_MAX): cv.float_,
                cv.Required(CONF_PI_TARGET): cv.float_,
                cv.Required(CONF_CONTROL_BAND): cv.float_,
                cv.Optional(CONF_TIME_INTERVAL, default=60): cv.int_,
                cv.Optional(CONF_START_OUTPUT, default=30.0): cv.float_,
            }
        ),
        cv.Required(CONF_TIME_INPUT): cv.Schema(
            {
                cv.Required(CONF_INPUT_START): cv.string,
                cv.Required(CONF_INPUT_STOP): cv.string,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_var = await cg.get_variable(config[CONF_TIMEID])
    cg.add(var.set_time(time_var))

    cg.add(var.get_ha_outside_temp_sensor(config[CONF_OUTSIDE_TEMPERATURE]))

    for temp_device in config[CONF_TEMP_SENSORS]:
        cg.add(
            var.create_ha_temperature_device(
                temp_device[CONF_ID_STRING], temp_device[CONF_ACCURACY]
            )
        )

    sensor_var = await cg.get_variable(config[CONF_CONTROL_SENSOR])
    cg.add(var.set_control_temperature_sensor(sensor_var))

    if CONF_CONTROL_NAME in config:
        controller_name = await cg.get_variable(config[CONF_CONTROL_NAME])
        cg.add(var.set_active_controller_name(controller_name))

    if CONF_CV_STATUS in config:
        cv_status = await cg.get_variable(config[CONF_CV_STATUS])
        cg.add(var.set_cv_status(cv_status))

    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_kp_low(params[CONF_KP_LOW]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_pi_max(params[CONF_PI_MAX]))
    cg.add(var.set_pi_target(params[CONF_PI_TARGET]))
    cg.add(var.set_control_band(params[CONF_CONTROL_BAND]))
    if CONF_TIME_INTERVAL in params:
        cg.add(var.set_time_interval(params[CONF_TIME_INTERVAL]))
    if CONF_START_OUTPUT in params:
        cg.add(var.set_start_output(params[CONF_START_OUTPUT]))

    inputparams = config[CONF_TIME_INPUT]
    cg.add(var.set_start_time_id(inputparams[CONF_INPUT_START]))
    cg.add(var.set_stop_time_id(inputparams[CONF_INPUT_STOP]))

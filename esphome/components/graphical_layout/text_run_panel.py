import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import font, color, sensor, text_sensor
from esphome.components.display import display_ns
from esphome.const import (
    CONF_ID,
    CONF_FOREGROUND_COLOR,
    CONF_BACKGROUND_COLOR,
    CONF_SENSOR,
)

graphical_layout_ns = cg.esphome_ns.namespace("graphical_layout")
TextRunPanel = graphical_layout_ns.class_("TextRunPanel")
TextAlign = display_ns.enum("TextAlign", is_class=True)
TextRunBase = graphical_layout_ns.class_("TextRunBase")
TextRun = graphical_layout_ns.class_("TextRun", TextRunBase)
SensorTextRun = graphical_layout_ns.class_("SensorTextRun", TextRunBase)
TextSensorTextRun = graphical_layout_ns.class_("TextSensorTextRun", TextRunBase)
CanWrapAtCharacterArguments = graphical_layout_ns.struct("CanWrapAtCharacterArguments")
CanWrapAtCharacterArgumentsConstRef = CanWrapAtCharacterArguments.operator(
    "const"
).operator("ref")

CONF_TEXT_RUN_PANEL = "text_run_panel"
CONF_FONT = "font"
CONF_TEXT = "text"
CONF_TEXT_ALIGN = "text_align"
CONF_MAX_WIDTH = "max_width"
CONF_MIN_WIDTH = "min_width"
CONF_RUNS = "runs"
CONF_CAN_WRAP_AT_CHARACTER = "can_wrap_at_character"
CONF_DEBUG_OUTLINE_RUNS = "debug_outline_runs"
CONF_TEXT_SENSOR = "text_sensor"
CONF_TEXT_FORMATTER = "text_formatter"

TEXT_ALIGN = {
    "TOP_LEFT": TextAlign.TOP_LEFT,
    "TOP_CENTER": TextAlign.TOP_CENTER,
    "TOP_RIGHT": TextAlign.TOP_RIGHT,
    "CENTER_LEFT": TextAlign.CENTER_LEFT,
    "CENTER": TextAlign.CENTER,
    "CENTER_RIGHT": TextAlign.CENTER_RIGHT,
    "BASELINE_LEFT": TextAlign.BASELINE_LEFT,
    "BASELINE_CENTER": TextAlign.BASELINE_CENTER,
    "BASELINE_RIGHT": TextAlign.BASELINE_RIGHT,
    "BOTTOM_LEFT": TextAlign.BOTTOM_LEFT,
    "BOTTOM_CENTER": TextAlign.BOTTOM_CENTER,
    "BOTTOM_RIGHT": TextAlign.BOTTOM_RIGHT,
}

BASE_RUN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FONT): cv.use_id(font.Font),
        cv.Optional(CONF_FOREGROUND_COLOR): cv.use_id(color.ColorStruct),
        cv.Optional(CONF_BACKGROUND_COLOR): cv.use_id(color.ColorStruct),
    }
)

SENSOR_TEXT_RUN_SCHEMA = BASE_RUN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SensorTextRun),
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEXT_FORMATTER): cv.returning_lambda,
    }
)

TEXT_RUN_SCHEMA = BASE_RUN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TextRun),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_TEXT_FORMATTER): cv.returning_lambda,
    }
)

TEXT_SENSOR_TEXT_RUN_SCHEMA = BASE_RUN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TextSensorTextRun),
        cv.Required(CONF_TEXT_SENSOR): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_TEXT_FORMATTER): cv.returning_lambda,
    }
)

RUN_SCHEMA = cv.Any(
    SENSOR_TEXT_RUN_SCHEMA,
    TEXT_RUN_SCHEMA,
    TEXT_SENSOR_TEXT_RUN_SCHEMA,
)


def get_config_schema(base_item_schema, item_type_schema):
    return base_item_schema.extend(
        {
            cv.GenerateID(): cv.declare_id(TextRunPanel),
            cv.Optional(CONF_TEXT_ALIGN): cv.enum(TEXT_ALIGN, upper=True),
            cv.Required(CONF_MAX_WIDTH): cv.int_range(min=0),
            cv.Optional(CONF_MIN_WIDTH, default=0): cv.int_range(min=0),
            cv.Optional(CONF_CAN_WRAP_AT_CHARACTER): cv.returning_lambda,
            cv.Optional(CONF_DEBUG_OUTLINE_RUNS, default=False): cv.boolean,
            cv.Required(CONF_RUNS): cv.All(
                cv.ensure_list(RUN_SCHEMA), cv.Length(min=1)
            ),
        }
    )


async def config_to_layout_item(pvariable_builder, item_config, child_item_builder):
    var = await pvariable_builder(item_config)

    min_width = item_config[CONF_MIN_WIDTH]
    cg.add(var.set_min_width(min_width))

    max_width = item_config[CONF_MAX_WIDTH]
    cg.add(var.set_max_width(max_width))

    if text_align := item_config.get(CONF_TEXT_ALIGN):
        cg.add(var.set_text_align(text_align))

    if can_wrap_at_character_config := item_config.get(CONF_CAN_WRAP_AT_CHARACTER):
        can_wrap_at_character = await cg.process_lambda(
            can_wrap_at_character_config,
            [(CanWrapAtCharacterArgumentsConstRef, "args")],
            return_type=cg.bool_,
        )
        cg.add(var.set_can_wrap_at(can_wrap_at_character))

    debug_outline_runs = item_config[CONF_DEBUG_OUTLINE_RUNS]
    if debug_outline_runs:
        cg.add(var.set_debug_outline_runs(debug_outline_runs))

    for run_config in item_config[CONF_RUNS]:
        run = None
        run_font = await cg.get_variable(run_config[CONF_FONT])
        if run_sensor_config := run_config.get(CONF_SENSOR):
            sens = await cg.get_variable(run_sensor_config)
            run = cg.new_Pvariable(run_config[CONF_ID], sens, run_font)
        elif run_text_sensor_config := run_config.get(CONF_TEXT_SENSOR):
            text_sens = await cg.get_variable(run_text_sensor_config)
            run = cg.new_Pvariable(run_config[CONF_ID], text_sens, run_font)
        else:
            run_text = await cg.templatable(
                run_config[CONF_TEXT], args=[], output_type=cg.std_string
            )
            run = cg.new_Pvariable(run_config[CONF_ID], run_text, run_font)

        if run_text_formatter_config := run_config.get(CONF_TEXT_FORMATTER):
            run_text_formatter = await cg.process_lambda(
                run_text_formatter_config,
                [(cg.std_string, "it")],
                return_type=cg.std_string,
            )
            cg.add(run.set_text_formatter(run_text_formatter))

        if run_background_color_config := run_config.get(CONF_BACKGROUND_COLOR):
            run_background_color = await cg.get_variable(run_background_color_config)
            cg.add(run.set_background_color(run_background_color))

        if run_foreground_color_config := run_config.get(CONF_FOREGROUND_COLOR):
            run_foreground_color = await cg.get_variable(run_foreground_color_config)
            cg.add(run.set_foreground_color(run_foreground_color))

        cg.add(var.add_text_run(run))

    return var

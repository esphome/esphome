from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CONDITION, CONF_ID, CONF_LAMBDA, CONF_STATE
from esphome.cpp_generator import LambdaExpression, RawExpression, UnaryOpExpression
from esphome.types import ConfigType

from .. import template_ns

TemplateBinarySensor = template_ns.class_(
    "TemplateBinarySensor", cg.Component, binary_sensor.BinarySensor
)
TemplateBinarySensorLambda = template_ns.class_(
    "TemplateBinarySensorLambda", TemplateBinarySensor
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(TemplateBinarySensor)
    .extend(
        {
            cv.Exclusive(CONF_LAMBDA, CONF_CONDITION): cv.returning_lambda,
            cv.Exclusive(
                CONF_CONDITION, CONF_CONDITION
            ): automation.validate_potentially_and_condition,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _declare_constexpr_lambda(
    config: ConfigType, template_: LambdaExpression
) -> RawExpression:
    """Declare a constexpr function pointer from a captureless lambda.

    Generates: static constexpr auto name = +[]() -> optional<bool> { body };
    The unary + converts the captureless lambda to a function pointer,
    making it usable as a non-type template parameter for inlining.
    """
    func_name = f"template_binary_sensor_{config[CONF_ID]}_f"
    # UnaryOpExpression handles the +lambda → function pointer conversion
    func_ptr = UnaryOpExpression("+", template_)
    cg.add_global(
        cg.RawStatement(f"static constexpr auto {func_name} = {func_ptr};"),
    )
    return RawExpression(func_name)


async def to_code(config):
    if lamb := config.get(CONF_LAMBDA):
        template_ = await cg.process_lambda(
            lamb, [], return_type=cg.optional.template(bool), capture=""
        )
        func_expr = _declare_constexpr_lambda(config, template_)
    elif condition := config.get(CONF_CONDITION):
        condition = await automation.build_condition(
            condition, cg.TemplateArguments(), []
        )
        template_ = LambdaExpression(
            f"return {condition.check()};",
            [],
            return_type=cg.optional.template(bool),
            capture="",
        )
        func_expr = _declare_constexpr_lambda(config, template_)
    else:
        func_expr = None

    if func_expr is not None:
        # TemplateBinarySensor<func> — compiler inlines the call
        id_ = config[CONF_ID]
        id_ = id_.copy()
        id_.type = TemplateBinarySensorLambda.template(func_expr)
        var = cg.new_Pvariable(id_)
    else:
        var = cg.new_Pvariable(config[CONF_ID])

    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)


@automation.register_action(
    "binary_sensor.template.publish",
    binary_sensor.BinarySensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
    synchronous=True,
)
async def binary_sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var

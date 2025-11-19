import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS, ENTITY_CATEGORY_CONFIG, ICON_ACCOUNT

from .. import CONF_C4002_ID, C4002Component, dfrobot_c4002_ns

# 定义选择器类
C4002Select = dfrobot_c4002_ns.class_("C4002Select", select.Select, cg.Component)

# 定义操作模式选项
OPERATING_MODE_OPTIONS = ["Mode_1", "Mode_2", "Mode_3"]

# 配置模式常量
CONF_OPERATING_MODE = "operating_mode"

# 配置验证架构
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_C4002_ID): cv.use_id(C4002Component),
        cv.Required(CONF_OPERATING_MODE): select.select_schema(
            C4002Select,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_ACCOUNT,
        ).extend(
            {
                cv.Required(CONF_OPTIONS): cv.All(
                    cv.ensure_list(cv.string_strict), cv.Length(min=1, max=3)
                ),
            }
        ),
    }
)


async def to_code(config):
    # 获取父组件
    c4002_component = await cg.get_variable(config[CONF_C4002_ID])

    # 配置操作模式选择器
    if operating_mode_config := config.get(CONF_OPERATING_MODE):
        # 创建选择器实例
        operating_mode_select = await select.new_select(
            operating_mode_config, options=OPERATING_MODE_OPTIONS
        )

        # 注册父组件关系
        await cg.register_parented(operating_mode_select, config[CONF_C4002_ID])

        # 将选择器添加到父组件
        cg.add(c4002_component.set_operating_mode_select(operating_mode_select))
        # cg.add(operating_mode_select.set_options(OPERATING_MODE_OPTIONS))

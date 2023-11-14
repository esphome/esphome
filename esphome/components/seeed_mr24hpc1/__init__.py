import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_THROTTLE, CONF_TIMEOUT, CONF_PASSWORD
from esphome import automation
from esphome.automation import maybe_simple_id

DEPENDENCIES = ["uart"]
# 是相关代码库的代码所有者
CODEOWNERS = ["@limengdu"]
# 当前的组件或者平台可以在同一个配置文件中被多次配置或者定义。
MULTI_CONF = True

# 这行代码创建了一个新的名为 ld2410_ns 的命名空间。
# 该命名空间将作为 ld2410_ns 组件相关的所有类、函数和变量的前缀，确保它们不会与其他组件的名称产生冲突。
mr24hpc1_ns = cg.esphome_ns.namespace("mr24hpc1")
# 这个 LD2410Component 类将是一个定期轮询的 UART 设备
mr24hpc1Component = mr24hpc1_ns.class_("mr24hpc1Component", cg.PollingComponent, uart.UARTDevice)

CONF_MR24HPC1_ID = "mr24hpc1_id"

# 创建了一个基础模式
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(mr24hpc1Component),
    }
)

# 这段代码扩展了当前的 CONFIG_SCHEMA，添加了 UART 设备和组件的所有配置参数。
# 这意味着在 YAML 配置文件中，用户可以使用这些参数来配置这个组件。
CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)
)

# 创建了一个验证模式，用于验证一个名为 "ld2410" 的 UART 设备的配置参数。
# 这个验证模式要求设备必须有发送和接收功能，奇偶校验方式为 "NONE"，并且停止位为 1。
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "mr24hpc1",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)

# async def 关键字用于定义协程函数。
# 协程函数是一种特殊的函数，设计用来配合 Python 的 asyncio 库，支持异步 I/O 操作。
async def to_code(config):
    # 这行代码创建了一个新的 Pvariable（一个代表 C++ 变量的 Python 对象），变量的 ID 是从配置中取出的。
    var = cg.new_Pvariable(config[CONF_ID])
    # 这行代码将新创建的 Pvariable 注册为一个组件，这样 ESPHome 就能在运行时管理它。
    await cg.register_component(var, config)
    # 这行代码将新创建的 Pvariable 注册为一个设备。
    await uart.register_uart_device(var, config)

CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(mr24hpc1Component),
    }
)

    
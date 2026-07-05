import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@p1ngb4ck"]

DEPENDENCIES = ["storage"]

CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_TASK_PRIORITY = "task_priority"
CONF_MAX_PENDING = "max_pending"

storage_worker_ns = cg.esphome_ns.namespace("storage_worker")
StorageWorker = storage_worker_ns.class_("StorageWorker", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageWorker),
        # FATFS LFN + NFS/lwIP transfers both need headroom on the worker task's stack.
        cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(
            min=4096, max=32768
        ),
        # FreeRTOS priority: above idle (0), below networking tasks (typically higher).
        cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=1, max=23),
        # Fixed request pool/queue depth — sized exactly at codegen like the storage
        # registry's device count, no heap allocation per request at runtime.
        cv.Optional(CONF_MAX_PENDING, default=4): cv.int_range(min=1, max=16),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    cg.add(var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_max_pending(config[CONF_MAX_PENDING]))

    cg.add(cg.RawExpression(f"{storage_worker_ns}::global_storage_worker = {var}"))

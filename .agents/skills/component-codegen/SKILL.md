---
name: component-codegen
description: Write the Python side of an ESPHome component - __init__.py and platform files (sensor.py, switch.py, ...), DOMAIN and other metadata, to_code, hub child entities with sub_sensors and friends, pins and referenced components, hub-forwarding entities (ApplyButton, ApplySwitch), uart/i2c/spi constructor helpers, and CORE.data state. Use when creating a component or changing its code generation.
---

# Component code generation

Read https://developers.esphome.io/architecture/components/ for the component lifecycle and how a
component is registered. Schema rules and schema extraction are in the `config-validation` skill;
automations (`on_*`, actions, conditions) in the `automations` skill.

## Files

```
components/[component_name]/
├── __init__.py          # Component configuration schema and code generation
├── [component].h        # C++ header file (if needed)
├── [component].cpp      # C++ implementation (if needed)
└── [platform]/          # Platform-specific implementations
    ├── __init__.py      # Platform-specific configuration
    ├── [platform].h     # Platform C++ header
    └── [platform].cpp   # Platform C++ implementation
```

## Metadata

- `DOMAIN`: required in every component's `__init__.py`, set to the component's own name as a plain string
  (e.g. `DOMAIN = "my_component"`). Other code refers to the component through it, e.g.
  `from esphome.components.my_component import DOMAIN`. Keep it in alphabetical order with the other
  metadata constants. CI (`lint_component_domain`) fails without it.
- `DEPENDENCIES`: list of required components.
- `AUTO_LOAD`: components to automatically load. Not a way to put entities under the component's own key;
  see "Entities of a hub" below.
- `CONFLICTS_WITH`: incompatible components.
- `CODEOWNERS`: GitHub usernames responsible for maintenance.
- `MULTI_CONF`: whether multiple instances are allowed.

## Basic pattern

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_KEY, CONF_ID

DOMAIN = "my_component"
CONF_PARAM = "param"  # A constant that does not yet exist in esphome/const.py

my_component_ns = cg.esphome_ns.namespace("my_component")
MyComponent = my_component_ns.class_("MyComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MyComponent),
        cv.Required(CONF_KEY): cv.string,
        cv.Optional(CONF_PARAM, default=42): cv.int_,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_key(config[CONF_KEY]))
    cg.add(var.set_param(config[CONF_PARAM]))
```

```cpp
namespace esphome::my_component {

class MyComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_key(const char *key) { this->key_ = StringRef(key); }
  void set_param(int param) { this->param_ = param; }

 protected:
  StringRef key_;
  int param_{0};
};

}  // namespace esphome::my_component
```

When the setter takes the value unchanged, use `cg.add_optional(var.set_blah, config.get(CONF_BLAH))`; it
is not awaited, and it skips only `None`, so `0` and `False` are still set. Required, invariant
dependencies are constructor arguments to `cg.new_Pvariable()`, not setters.

## Entity platforms

**Sensor:**
```python
from esphome.components import sensor

CONFIG_SCHEMA = sensor.sensor_schema(MySensor).extend(
    cv.polling_component_schema("60s")
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
```

**Binary sensor:**
```python
from esphome.components import binary_sensor

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({...})


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
```

**Switch:**
```python
from esphome.components import switch

CONFIG_SCHEMA = switch.switch_schema().extend({...})


async def to_code(config):
    var = await switch.new_switch(config)
```

## Entities of a hub

A new component's entities go in platform files (`sensor.py`, `binary_sensor.py`, ...) that take the hub's
id, never under the hub's own key with their platforms added to `AUTO_LOAD` to make that work. Auto loading
a platform because the C++ uses its type is fine. See
https://developers.esphome.io/architecture/components/#exposing-multiple-entities.
```yaml
# Bad - entities under the hub's key
my_hub:
  connected:
    name: Connected

# Good - the hub, then one platform entry that holds its entities
my_hub:
  id: hub1
binary_sensor:
  - platform: my_hub
    my_hub_id: hub1  # optional with a single hub
    connected:
      name: Connected
```

**Optional child entities of a hub:** bind the config once with `sensor.sub_sensors(config)` (or
`sub_binary_sensors`, `sub_text_sensors`, `sub_buttons`, `sub_switches`, `sub_numbers`, `sub_selects` in
their domains), adding `parent=hub` for entities that derive from `Parented<T>`, then make one call per
key, even when there is only one. A call creates the entity only when its key is configured, passes it to
the setter and returns it (or `None`); extra arguments such as `min_value` or `options` go on the call.
Always name the setter explicitly on the object that owns it, never with `getattr` and an f-string, and
keep that variable short (`var` for the component itself, `hub` for one fetched with `cg.get_variable`) so
the calls fit on one line. Loops whose setter also takes an index, such as `set_gate_threshold(x, n)`, stay
as they are.
```python
# my_hub/sensor.py
async def to_code(config):
    hub = await cg.get_variable(config[CONF_MY_HUB_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, hub.set_humidity_sensor)


# my_hub/button.py
async def to_code(config):
    hub = await cg.get_variable(config[CONF_MY_HUB_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_RESTART, hub.set_restart_button)
```

**Hub forwarding entities need no C++ class:** a button, switch, number or select that only calls its hub
uses `button.ApplyButton` (`switch.ApplySwitch`, `number.ApplyNumber`, `select.ApplySelect`) in its schema
and `apply=` on the `sub_*` call. The body is a call on the hub with `entity` and `state`, `value` or
`index`/`option` in scope; on a switch, number or select `publish="before"` or `"after"` publishes the
state.
```python
switches = switch.sub_switches(config, parent=hub)
await switches(CONF_BLUETOOTH, None, apply="set_bluetooth(state)", publish="after")
```
Restore an owned or standalone switch in `setup()` with `sw->restore()`, or `restore(default)` to still
drive a state when the restore mode is disabled.

**Entities that each watch a register field:** one class taking register, shift, mask and match from a
Python tuple table, in typed `StaticVector`s sized by `cg.slot_counter`; not a template per bit, not a
virtual listener base (a second vptr per entity).

## Other helpers

- **Pins and referenced components:** set them with `await cg.add_pin(var.set_cs_pin, config[CONF_CS_PIN])`
  and `await cg.add_variable(var.set_bus, config[CONF_BUS_ID])`; both skip a `None` value, so pass
  `config.get(KEY)` for an optional key, and return what they created. Use `cg.register_parented` for
  `set_parent`.
- `uart.new_uart_component(config, *ctor_args)` (and `i2c.new_i2c_component`, `spi.new_spi_component`,
  `modbus.new_modbus_client_component`, `ble_client.new_ble_node_component`) constructs, registers and
  attaches in one call.
- `display.add_writer(var, config)` sets the lambda.
- `MyEnum.options("on", prefix="MY_ENUM_")` builds an enum dict.
- `button.restart_button_schema(cls)` (and `factory_reset_`, `query_`) are button presets.
- `fv.get_declaring_config(id)` finds a hub's config in final validation.

## State during code generation

Use `CORE.data` for component state that needs to persist during configuration generation, namespaced
under the component's `DOMAIN` and held in a `@dataclass`. Avoid module-level mutable globals: they
persist between compilation runs if the host process (e.g. device-builder) doesn't fork/exec, whereas
`CORE.data` is cleared between runs.

```python
from dataclasses import dataclass, field
from esphome.core import CORE

DOMAIN = "my_component"


@dataclass
class MyComponentData:
    feature_enabled: bool = False
    item_count: int = 0
    items: list[str] = field(default_factory=list)


def _get_data() -> MyComponentData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = MyComponentData()
    return CORE.data[DOMAIN]


def request_feature() -> None:
    _get_data().feature_enabled = True
```

Don't use flat keys such as `CORE.data["my_component_feature"] = True`. Some components use `TypedDict`
instead of a dataclass; both are acceptable.

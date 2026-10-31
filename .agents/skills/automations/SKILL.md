---
name: automations
description: Write ESPHome automation building blocks - triggers (on_* keys), actions and conditions. Use when adding or changing an on_* trigger, an action such as my_component.do_something, a condition, a callback registration method (add_on_*_callback), or code that calls build_callback_automation, build_automation, register_*_action or register_*_condition.
---

# Automations: triggers, actions and conditions

Automations have three building blocks: **Triggers** (fire when something happens), **Actions** (do
something), and **Conditions** (check if something is true). The helpers named here are documented in
their docstrings in `esphome/automation.py`.

## Triggers: callback method (preferred)

Use `build_callback_automation()` for simple triggers. This eliminates the need for a C++ Trigger class by
using a lightweight pointer-sized forwarder struct registered directly as a callback. No `CONF_TRIGGER_ID`
in the schema.

**Python:**
```python
from esphome import automation

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MyComponent),
        cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for conf in config.get(CONF_ON_STATE, []):
        await automation.build_callback_automation(
            var, "add_on_state_callback", [(bool, "x")], conf
        )
```

`build_callback_automation` arguments: `parent`, `callback_method` (C++ method name), `args` (template args
as `[(type, name)]` tuples), `config`, and optional `forwarder` (defaults to `TriggerForwarder<Ts...>`).

For boolean filtering (e.g. `on_press`/`on_release`), use built-in forwarders with `args=[]`:
```python
for conf_key, forwarder in (
    (CONF_ON_PRESS, automation.TriggerOnTrueForwarder),
    (CONF_ON_RELEASE, automation.TriggerOnFalseForwarder),
):
    for conf in config.get(conf_key, []):
        await automation.build_callback_automation(
            var, "add_on_state_callback", [], conf, forwarder=forwarder
        )
```

When the callback's parameters differ from the automation's arguments, or the trigger should only fire for
some values, pass `params`, `forward` and `when`; the helper then generates a capture-less lambda (stored
inline, nothing allocated). Name the parent with `automation.parent_ref(var)` and build `forward` from
`MockObj` calls, not f-strings of C++:
```python
parent = automation.parent_ref(var)
index = cg.RawExpression("index")
await automation.build_callback_automation(
    var,
    "add_on_state_callback",
    [(cg.StringRef, "x"), (cg.size_t, "i")],
    conf,
    params=[(cg.size_t, "index")],
    forward=[cg.StringRef(parent.option_at(index)), index],
)
```
`build_parent_callback_automation`, `build_trigger_callback`, `build_callback_automations` and
`build_trigger_automations` cover the other shapes; see their docstrings in `esphome/automation.py`.

**C++: no trigger class needed.** The callback registration method must be templatized to accept both
`std::function` and lightweight forwarder structs (which avoid heap allocation):
```cpp
class MyComponent : public Component {
 public:
  // Must be a template -- accepts both std::function and pointer-sized forwarder structs
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }
 protected:
  // Use CallbackManager when callbacks are always registered (e.g. core components)
  CallbackManager<void(bool)> state_callback_;
  // Use LazyCallbackManager when callbacks are often not registered -- saves 8 bytes
  // (nullptr vs empty std::vector) per instance when no callbacks are added
  // LazyCallbackManager<void(bool)> state_callback_;
};
```

Never type a registration method's parameter as `std::function`; that forces a heap allocation for every
forwarder struct:
```cpp
// Bad
void add_on_state_callback(std::function<void(bool)> &&callback) {
  this->state_callback_.add(std::move(callback));
}
```

| Type | Idle overhead (32-bit) | When to use |
|------|----------------------|-------------|
| `CallbackManager<void(Ts...)>` | 12 bytes (empty `std::vector`) | Callbacks are always or almost always registered |
| `LazyCallbackManager<void(Ts...)>` | 4 bytes (`nullptr`) | Callbacks are often not registered (common case) |

Both live in `esphome/core/helpers.h` and accept `std::function`, lambdas and forwarder structs through
their templatized `add()` method. `LazyCallbackManager` is a drop-in replacement for `CallbackManager` that defers allocation until the first
callback is added. Prefer it for entity-level callbacks where most instances have no subscribers.

## Triggers: Trigger class method

Use `build_automation()` with a `Trigger<Ts...>` subclass only when the forwarder needs **mutable state
beyond a single `Automation*` pointer** (e.g. edge detection tracking previous state, timing logic).

**Python:**
```python
TurnOnTrigger = my_ns.class_("TurnOnTrigger", automation.Trigger.template())

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TurnOnTrigger)}
        ),
    }
)


async def to_code(config):
    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
```

**C++:**
```cpp
class TurnOnTrigger : public Trigger<> {
 public:
  explicit TurnOnTrigger(MyComponent *parent) : last_on_{false} {
    parent->add_on_state_callback([this](bool state) {
      if (state && !this->last_on_)
        this->trigger();
      this->last_on_ = state;
    });
  }
 protected:
  bool last_on_;
};
```

## Actions

```cpp
template<typename... Ts> class MyAction : public Action<Ts...> {
 public:
  explicit MyAction(MyComponent *parent) : parent_(parent) {}
  void play(const Ts &...) override { this->parent_->do_something(); }
 protected:
  MyComponent *parent_;
};
```
Register it without writing a builder:
```python
automation.register_simple_action(
    "my_component.do_something", MyAction, schema, synchronous=True
)
```
The constructor receives the object named by `config[CONF_ID]`. Use `register_bare_action` for a
no-argument constructor, `register_parented_action` for a class deriving from `Parented<T>`, and the
`@automation.register_action(...)` decorator only when the builder must also set fields.

Use `synchronous=True` for actions that run to completion inside `play()` without deferring. Use
`synchronous=False` if the action may suspend/defer execution (e.g. `delay`, `wait_until`, `script.wait`)
or store trigger arguments for later use.

**Actions that only forward templatable values to their parent need no C++ class.** Register them with
`register_apply_action`; do not write a `TEMPLATABLE_VALUE` class or a builder for this shape.
```python
automation.register_apply_action(
    "my_component.set_gains",
    schema,
    automation.ApplyField(CONF_KP, "set_kp", cg.float_),
    automation.ApplyField(CONF_KI, "set_ki", cg.float_),
)
```
The `ApplyField`, `ApplyCall` and `register_apply_action` docstrings in `esphome/automation.py` cover the
rest; `cover.control` and `cover.template.publish` are in-tree examples. An action whose input is a byte
payload passes `automation.ApplyBytes(CONF_DATA, "send({})")` in the same call; a constant payload stays in
flash and a lambda's vector is forwarded as data and size. `TEMPLATABLE_VALUE` with `cg.templatable` stays
for actions whose `play()` has real logic beyond forwarding values.

Adjacent `register_apply_action` and `lambda:` actions in a list merge into one generated function and one
object, which saves 12 to 16 bytes of RAM per merged action. An action with its own C++ class ends the merged
run, so prefer `register_apply_action` wherever the action allows it. Because of this, `build_action_list` can
return fewer objects than the config has entries.

## Conditions

```cpp
template<typename... Ts> class MyCondition : public Condition<Ts...> {
 public:
  explicit MyCondition(MyComponent *parent) : parent_(parent) {}
  bool check(const Ts &...) override { return this->parent_->is_active(); }
 protected:
  MyComponent *parent_;
};
```
Register with `automation.register_simple_condition("my_component.is_active", MyCondition, schema)`;
`register_bare_condition`, `register_parented_condition` and the decorator follow the action rules.

**Conditions that only test their parent need no C++ class either.** Register them with
`register_apply_condition`; the expression is applied to the parent, and an `ApplyCall` compares against
config values.
```python
automation.register_apply_condition("my_component.is_active", schema, "is_active()")
automation.register_apply_condition(
    "my_component.state_is",
    schema,
    automation.ApplyCall("state == {}", ((CONF_STATE, cg.bool_),)),
)
```
`cover.is_open`, `rtttl.is_playing` and `component.is_idle` are in-tree examples.

import pytest

from esphome import codegen as cg
from esphome.cpp_generator import _extract_component_ns


# Test interface remains the same.
@pytest.mark.parametrize(
    "attr",
    (
        # from cpp_generator
        "Expression",
        "RawExpression",
        "RawStatement",
        "TemplateArguments",
        "StructInitializer",
        "ArrayInitializer",
        "safe_exp",
        "Statement",
        "LineComment",
        "progmem_array",
        "statement",
        "variable",
        "Pvariable",
        "new_Pvariable",
        "add",
        "add_global",
        "add_library",
        "add_build_flag",
        "add_define",
        "get_variable",
        "get_variable_with_full_id",
        "process_lambda",
        "is_template",
        "templatable",
        "MockObj",
        "MockObjClass",
        # from cpp_helpers
        "gpio_pin_expression",
        "register_component",
        "build_registry_entry",
        "build_registry_list",
        "extract_registry_entry_config",
        "register_parented",
        "global_ns",
        "void",
        "nullptr",
        "float_",
        "double",
        "bool_",
        "int_",
        "std_ns",
        "std_string",
        "std_vector",
        "uint8",
        "uint16",
        "uint32",
        "int32",
        "const_char_ptr",
        "NAN",
        "esphome_ns",
        "App",
        "EntityBase",
        "Component",
        "ComponentPtr",
        # from cpp_types
        "PollingComponent",
        "Application",
        "optional",
        "arduino_json_ns",
        "JsonObject",
        "JsonObjectConst",
        "Controller",
        "GPIOPin",
    ),
)
def test_exists(attr):
    assert hasattr(cg, attr)


@pytest.mark.parametrize(
    ("type_str", "expected"),
    (
        ("esphome::dsmr::Dsmr", "dsmr"),
        ("esphome::logger::Logger", "logger"),
        ("esphome::web_server::WebServer", "web_server"),
        ("esphome::deep_sleep::DeepSleep", "deep_sleep"),
        ("esphome::Component", "esphome"),
        ("Logger", "esphome"),
        # Template types with :: in template args must not confuse extraction
        (
            "esphome::Automation<std::optional<bool>, std::optional<bool>>",
            "esphome",
        ),
        (
            "esphome::StatelessLambdaAction<std::optional<bool>, std::optional<bool>>",
            "esphome",
        ),
        # Namespaced template type
        ("esphome::sensor::Sensor<std::string>", "sensor"),
    ),
)
def test_extract_component_ns(type_str, expected):
    assert _extract_component_ns(type_str) == expected

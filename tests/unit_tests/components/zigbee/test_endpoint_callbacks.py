"""Run the production sparse callback table with host sanitizers, without an SDK."""

from pathlib import Path
import re
import shutil
import subprocess

import pytest


@pytest.mark.parametrize("integration", [False, True])
def test_endpoint_callbacks_host(tmp_path: Path, integration: bool) -> None:
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A host C++ compiler is required")
    root = Path(__file__).resolve().parents[4]
    source = Path(__file__).with_name(
        "component_callbacks_test.cpp" if integration else "endpoint_callbacks_test.cpp"
    )
    # Stub only SDK/base-class boundaries; compile the real component registration
    # method, callback table and framework logging macros together.
    stubs = {
        "esphome/core/defines.h": """
#pragma once
#define USE_ZIGBEE
#define USE_NRF52
#define ZIGBEE_ENDPOINTS_COUNT 2
#define ESPHOME_LOG_LEVEL 1
""",
        "esphome/core/component.h": """
#pragma once
#include <cstdint>
namespace esphome {
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void dump_config() {}
  virtual void loop() {}
};
}
""",
        "esphome/core/automation.h": """
#pragma once
namespace esphome {
template<typename T> struct CallbackManager { template<typename F> void add(F &&) {} };
template<typename T> struct LazyCallbackManager { template<typename F> void add(F &&) {} };
}
""",
        "zboss_api.h": """
#pragma once
#include <stdint.h>
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef uint8_t zb_bool_t;
typedef uint8_t zb_uchar_t;
typedef uint8_t zb_bufid_t;
struct zb_zcl_configure_reporting_req_t {};
struct zb_zcl_attr_addr_info_t {};
#define ZB_ZCL_MAX_STRING_SIZE 254
""",
        "zboss_api_addons.h": "#pragma once\n",
    }
    for name, contents in stubs.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    extra_sources = []
    if integration:
        # Compile the exact production diagnostic body and tag without pulling
        # in the unrelated Zephyr stack lifecycle. Do not duplicate its logic.
        implementation = (
            root / "esphome/components/zigbee/zigbee_zephyr.cpp"
        ).read_text()
        method = re.search(
            r"^void ZigbeeComponent::log_callback_registration_error_\([^\n]+\) \{\n.*?^\}",
            implementation,
            re.MULTILINE | re.DOTALL,
        )
        tag = re.search(r"^ESPHOME_LOG_TAG\(TAG, .*?\);$", implementation, re.MULTILINE)
        assert method is not None and tag is not None
        diagnostic = tmp_path / "registration_diagnostic.cpp"
        diagnostic.write_text(
            '#include "esphome/components/zigbee/zigbee_zephyr.h"\n'
            '#include "esphome/core/log.h"\n'
            "namespace esphome::zigbee {\n" + tag[0] + "\n" + method[0] + "\n}\n"
        )
        extra_sources.append(str(diagnostic))
    binary = tmp_path / "endpoint_callbacks_test"
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer",
            "-I",
            str(tmp_path),
            "-I",
            str(root),
            str(source),
            *extra_sources,
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)

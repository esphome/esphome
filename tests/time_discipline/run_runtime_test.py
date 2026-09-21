"""Run the production C++ bridge and Rust controller with a simulated ESP clock."""

from pathlib import Path
import subprocess
import sys
import tempfile

STUBS = {
    "esphome/core/defines.h": "#define USE_HOST\n#define USE_TIME_DISCIPLINE\n",
    "esphome/core/component.h": """
#pragma once
#include <cstdint>
namespace esphome {
class Component {
 public:
  virtual void setup() {}
  virtual void update() {}
  virtual void dump_config() {}
  void mark_failed() {}
};
class PollingComponent : public Component {
 public:
  explicit PollingComponent(uint32_t = 0) {}
};
}
""",
    "esphome/core/helpers.h": """
#pragma once
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#include "defines.h"
namespace esphome {
using Mutex = std::mutex;
using LockGuard = std::lock_guard<Mutex>;
template<typename Signature> class LazyCallbackManager {
 public:
  template<typename F> void add(F &&f) { callbacks_.emplace_back(std::forward<F>(f)); }
  void call() { for (auto &f : callbacks_) f(); }
 protected:
  std::vector<std::function<Signature>> callbacks_;
};
}
""",
    "esphome/core/automation.h": """
#pragma once
namespace esphome {
template<typename... Ts> class Condition {
 public:
  virtual bool check(const Ts &...) = 0;
};
}
""",
    "esphome/core/time.h": """
#pragma once
#include <ctime>
namespace esphome {
struct ESPTime {
  int year{}, month{}, day_of_month{}, hour{}, minute{}, second{};
  time_t timestamp{};
  static ESPTime from_epoch_utc(time_t t) { ESPTime result; result.timestamp = t; return result; }
  static ESPTime from_epoch_local(time_t t) { return from_epoch_utc(t); }
  bool is_valid() const { return timestamp >= 1546300800; }
};
}
""",
    "esphome/core/log.h": """
#pragma once
#include <cstdio>
#define ESP_LOGE(tag, ...) do { (void)(tag); std::fprintf(stderr, __VA_ARGS__); } while (0)
#define ESP_LOGW(tag, ...) (void)(tag)
#define ESP_LOGD(tag, ...) (void)(tag)
#define ESP_LOGVV(tag, ...) (void)(tag)
#define ESP_LOGCONFIG(tag, ...) (void)(tag)
""",
    "esp_timer.h": "#pragma once\n#include <cstdint>\nint64_t esp_timer_get_time();\n",
    "freertos/FreeRTOS.h": "#pragma once\n",
    "freertos/task.h": "#pragma once\ninline unsigned uxTaskGetStackHighWaterMark(void *) { return 0; }\n",
}


def main() -> None:
    archive = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory() as directory:
        build = Path(directory)
        for name, content in STUBS.items():
            path = build / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
        executable = build / "runtime-test"
        subprocess.run(
            [
                "c++",
                "-std=c++20",
                "-O2",
                f"-I{build}",
                f"-I{root}",
                f"-I{root / 'esphome/components/time/statime/include'}",
                str(root / "tests/time_discipline/runtime_test.cpp"),
                str(root / "esphome/components/time/clock_discipline.cpp"),
                str(root / "esphome/components/time/real_time_clock.cpp"),
                str(archive),
                "-lpthread",
                "-ldl",
                "-lm",
                "-o",
                str(executable),
            ],
            check=True,
        )
        for drift in ("50", "-50"):
            subprocess.run([str(executable), drift], check=True)


if __name__ == "__main__":
    main()

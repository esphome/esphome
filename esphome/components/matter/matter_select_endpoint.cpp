#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
#ifdef USE_SELECT

#include "matter_select_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/select/select.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/mode-select-server/supported-modes-manager.h>

#include <cstring>
#include <utility>
#include <vector>

namespace esphome::matter {

static const char *const TAG = "matter.select";

// Meyers singleton — CHIP's SupportedModesManager is a process-wide global
// (setSupportedModesManager stashes a single pointer). We dispatch per-endpoint
// queries via an internal map that each MatterSelectEndpoint populates from
// setup(). Registration into CHIP happens once on the first select setup.
namespace {

// NOLINTBEGIN(readability-identifier-naming)  // CHIP interface overrides
class SharedModesManager : public ::chip::app::Clusters::ModeSelect::SupportedModesManager {
 public:
  static SharedModesManager &instance() {
    static SharedModesManager self;
    return self;
  }

  void register_endpoint(uint16_t ep, MatterSelectEndpoint *w) { endpoints_.emplace_back(ep, w); }

  ModeOptionsProvider getModeOptionsProvider(::chip::EndpointId endpointId) const override {
    MatterSelectEndpoint *wrapper = this->find_(static_cast<uint16_t>(endpointId));
    if (wrapper == nullptr) {
      // Consistent with the handle_*_write dispatchers in matter_component.cpp:
      // a fabric query for an endpoint we don't own is a dispatch inconsistency,
      // not routine traffic. Log at WARN so the empty SupportedModes list (which
      // renders as a select with no options) has a matching entry in the trace.
      ESP_LOGW(TAG, "SharedModesManager: no wrapper for endpoint=%u — returning empty options",
               static_cast<unsigned>(endpointId));
      return ModeOptionsProvider();
    }
    const auto &opts = wrapper->mode_options();
    return ModeOptionsProvider(opts.data(), opts.data() + opts.size());
  }

  ::chip::Protocols::InteractionModel::Status getModeOptionByMode(
      ::chip::EndpointId endpointId, uint8_t mode,
      const ::chip::app::Clusters::ModeSelect::Structs::ModeOptionStruct::Type **dataPtr) const override {
    MatterSelectEndpoint *wrapper = this->find_(static_cast<uint16_t>(endpointId));
    if (wrapper == nullptr) {
      ESP_LOGW(TAG, "SharedModesManager: no wrapper for endpoint=%u on mode lookup", static_cast<unsigned>(endpointId));
      return ::chip::Protocols::InteractionModel::Status::UnsupportedEndpoint;
    }
    const auto &opts = wrapper->mode_options();
    for (const auto &opt : opts) {
      if (opt.mode == mode) {
        *dataPtr = &opt;
        return ::chip::Protocols::InteractionModel::Status::Success;
      }
    }
    return ::chip::Protocols::InteractionModel::Status::InvalidCommand;
  }

 protected:
  SharedModesManager() = default;

 private:
  MatterSelectEndpoint *find_(uint16_t endpoint_id) const {
    for (const auto &kv : this->endpoints_) {
      if (kv.first == endpoint_id) {
        return kv.second;
      }
    }
    return nullptr;
  }

  // Linear scan is preferable to unordered_map here: select endpoints are
  // typically ≤3 per device and the lookup only fires on fabric-driven
  // mode changes (not a hot path); dropping the hash table saves ~2KB per
  // MatterSelectEndpoint's shared singleton.
  std::vector<std::pair<uint16_t, MatterSelectEndpoint *>> endpoints_;
};
// NOLINTEND(readability-identifier-naming)

}  // namespace

MatterSelectEndpoint::MatterSelectEndpoint(::esphome::select::Select *sel) : select_(sel) {}

bool MatterSelectEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for select '%s'", this->select_->get_name().c_str());
    return false;
  }

  const auto &opts = this->select_->traits.get_options();
  if (opts.size() == 0) {
    ESP_LOGW(TAG, "select '%s' has no options — skipping Matter registration", this->select_->get_name().c_str());
    return false;
  }
  if (opts.size() > 255) {
    ESP_LOGW(TAG,
             "select '%s' has %u options; Matter mode field is uint8, "
             "truncating to 255",
             this->select_->get_name().c_str(), static_cast<unsigned>(opts.size()));
  }
  size_t count = opts.size() > 255 ? 255 : opts.size();

  // Reserve first so string data pointers remain valid. CharSpan is
  // non-owning — any later resize/realloc dangles every entry.
  this->option_labels_.reserve(count);
  this->mode_options_.reserve(count);
  for (size_t i = 0; i < count; i++) {
    const char *raw = opts[i];
    this->option_labels_.emplace_back(raw ? raw : "");
    ::chip::app::Clusters::ModeSelect::Structs::ModeOptionStruct::Type entry;
    entry.label = ::chip::CharSpan::fromCharString(this->option_labels_.back().c_str());
    entry.mode = static_cast<uint8_t>(i);
    // semanticTags stays default-constructed (empty List) — optional per spec
    this->mode_options_.push_back(entry);
  }

  ::esp_matter::endpoint::mode_select::config_t config;
  const std::string &name = this->select_->get_name();
  std::strncpy(config.mode_select.description, name.c_str(), sizeof(config.mode_select.description) - 1);
  config.mode_select.description[sizeof(config.mode_select.description) - 1] = '\0';
  // standard_namespace is declared `const nullable<uint16_t>` in
  // esp_matter_cluster.h — can't be assigned. Default-constructed nullable
  // is already null, which is what we want.
  // Initial mode: use current active index if the entity already picked one,
  // otherwise start at 0.
  uint8_t initial_mode = 0;
  if (auto idx = this->select_->active_index()) {
    initial_mode = static_cast<uint8_t>(*idx > 255 ? 255 : *idx);
  }
  config.mode_select.current_mode = initial_mode;
  // Register the shared manager as CHIP's SupportedModesManager. esp-matter's
  // ModeSelectDelegateInitCB (esp_matter_delegate_callbacks.cpp:458) casts
  // this to `SupportedModesManager*` and calls
  // ModeSelect::setSupportedModesManager on cluster init. Passing the same
  // pointer for every mode_select endpoint is intentional and correct — CHIP
  // stores exactly one global.
  config.mode_select.delegate = &SharedModesManager::instance();

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::mode_select::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create mode_select endpoint for '%s'", this->select_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->select_->get_name());

  // Populate the singleton's per-endpoint lookup so getModeOptionsProvider /
  // getModeOptionByMode can find our labels when CHIP queries them.
  SharedModesManager::instance().register_endpoint(this->endpoint_id_, this);

  // Device → fabric: ESPHome fires with the new index on state change.
  this->select_->add_on_state_callback([this](size_t index) {
    if (this->applying_matter_write_) {
      ESP_LOGV(TAG, "device state callback suppressed (matter-driven change) endpoint=%u", this->endpoint_id_);
      return;
    }
    uint8_t mode = static_cast<uint8_t>(index > 255 ? 255 : index);
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u mode=%u select='%s'", this->endpoint_id_,
             static_cast<unsigned>(mode), this->select_->get_name().c_str());
    this->report_state_to_fabric_(mode);
  });

  ESP_LOGI(TAG, "registered select '%s' as Matter mode_select endpoint %u (%u options)",
           this->select_->get_name().c_str(), this->endpoint_id_, static_cast<unsigned>(count));
  return true;
}

void MatterSelectEndpoint::on_matter_current_mode_write(uint8_t mode) {
  ESP_LOGD(TAG, "matter CurrentMode write endpoint=%u mode=%u select='%s'", this->endpoint_id_,
           static_cast<unsigned>(mode), this->select_->get_name().c_str());
  if (mode >= this->mode_options_.size()) {
    ESP_LOGW(TAG, "matter CurrentMode write mode=%u out of range (max=%u) — ignoring", static_cast<unsigned>(mode),
             static_cast<unsigned>(this->mode_options_.size()));
    return;
  }
  // Runs on the CHIP task — defer the Select call and the guard onto the
  // main loop.
  MatterComponent::instance()->defer_on_main_loop([this, mode]() {
    this->applying_matter_write_ = true;
    this->select_->make_call().set_index(static_cast<size_t>(mode)).perform();
    this->applying_matter_write_ = false;
  });
}

void MatterSelectEndpoint::push_initial_state() {
  uint8_t mode = 0;
  if (auto idx = this->select_->active_index()) {
    mode = static_cast<uint8_t>(*idx > 255 ? 255 : *idx);
  }
  this->report_state_to_fabric_(mode);
}

void MatterSelectEndpoint::report_state_to_fabric_(uint8_t mode) {
  ::esp_matter_attr_val_t val = ::esp_matter_uint8(mode);
  ApplyingReportGuard applying_report_guard(this->applying_report_);
  MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, chip::app::Clusters::ModeSelect::Id,
                                                      chip::app::Clusters::ModeSelect::Attributes::CurrentMode::Id,
                                                      val);
}

}  // namespace esphome::matter

#endif  // USE_SELECT
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED

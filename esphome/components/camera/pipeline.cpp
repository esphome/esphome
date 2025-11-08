#include "pipeline.h"

namespace esphome::camera {

static const char *const TAG = "pipeline";

void Pipeline::add_link(Processor *parent, Processor *child) {
  this->links_[parent].push_back(child);
  this->parents_[child] = parent;
}

std::unordered_set<Output *> Pipeline::filter_outputs(const RequesterFlags &flags) {
  std::unordered_set<Output *> result;
  for (CameraRequester requester : flags) {
    auto it = this->outputs_.find(requester);
    if (it != this->outputs_.end()) {
      result.insert(it->second);
    } else {
      if (this->default_output_)
        result.insert(this->default_output_);
    }
  }
  return result;
}

RequesterFlags Pipeline::filter_requesters(Output *output, const RequesterFlags &flags) {
  RequesterFlags result;
  if (output == this->default_output_) {
    for (CameraRequester requester : flags) {
      auto it = this->outputs_.find(requester);
      if (it == this->outputs_.end()) {
        result.add(requester);
      }
    }
  } else {
    for (CameraRequester requester : flags) {
      auto it = this->outputs_.find(requester);
      if (it != this->outputs_.end()) {
        result.add(requester);
        return result;
      }
    }
  }
  return result;
}

bool Pipeline::configure() {
  if (!this->input_) {
    ESP_LOGE(TAG, "No input processor set!");
    return false;
  }

  if (!this->default_output_ && this->outputs_.empty()) {
    ESP_LOGE(TAG, "No output processor set!");
    return false;
  }

  if (Processor *current = this->find_cycle_()) {
    ESP_LOGE(TAG, "Cyclic link detected starting at %s!", current->get_id());
    return false;
  }

  std::unordered_set<Processor *> unlinked = this->find_unlinked_();
  if (!unlinked.empty()) {
    std::string result;
    for (Processor *p : unlinked) {
      if (!result.empty())
        result += ", ";

      result += p->get_id();
    }

    ESP_LOGE(TAG, "Unlinked processors not connected to the pipeline: %s", result.c_str());
    return false;
  }

  Processor *processor = this->input_;
  while (processor) {
    if (!processor->configure()) {
      ESP_LOGE(TAG, "%s: Processor configure failed!", processor->get_id());
      return false;
    }

    processor = this->find_next_(processor);
  }

  return true;
}

PipelineError Pipeline::process() {
  if (!this->reenter_) {
    this->reenter_ = true;
    this->current_ = this->input_;
    this->parent_ = nullptr;
    this->input_image_format_ = this->current_->get_output_image_format();
    this->input_image_ = nullptr;
    this->traversed_.clear();
  }

  ProcessorError error = PROCESSOR_ERROR_SUCCESS;
  while (this->current_ && this->reenter_) {
    if (this->input_image_format_ == IMAGE_FORMAT_RAW) {
      error = this->current_->process_pixels(&this->input_image_spec_, this->input_image_);
    } else {
      error = this->current_->process_compressed_image(this->input_image_format_, this->input_image_);
    }
    switch (error) {
      case PROCESSOR_ERROR_SUCCESS: {
        this->traversed_.insert(this->current_);
      } break;
      case PROCESSOR_ERROR_SKIP_FRAME: {
        this->reenter_ = false;
      } break;
      case PROCESSOR_ERROR_RETRY_FRAME: {
        return PIPELINE_ERROR_REENTER;
      } break;
      case PROCESSOR_ERROR_CONFIGURATION: {
        ESP_LOGE(TAG, "%s: PROCESSOR_ERROR_CONFIGURATION", this->current_->get_id());
        return PIPELINE_ERROR_CONFIGURATION;
      } break;
    }

    // Prepare input for next processor
    this->current_ = this->find_next_(this->current_);
    if (!this->current_)
      break;

    this->parent_ = this->find_parent_(this->current_);
    this->input_image_ = this->parent_->get_output_image();
    this->input_image_spec_ = *this->parent_->get_output_image_spec();
    this->input_image_format_ = this->parent_->get_output_image_format();
  }

  // Release all resources, acquired framebuffer in camera sensor...
  for (Processor *processor : this->traversed_)
    processor->release_resources();

  this->reenter_ = false;
  return error == PROCESSOR_ERROR_SUCCESS ? PIPELINE_ERROR_SUCCESS : PIPELINE_ERROR_SKIP_FRAME;
}

void Pipeline::log_config() {
  Processor *processor = this->input_;
  Processor *parent = nullptr;
  this->input_->log_config();
  for (auto &it_child : this->parents_)
    it_child.first->log_config();

  ESP_LOGCONFIG(TAG, "Pipeline:");
  while (processor) {
    processor = this->find_next_(processor);
    parent = this->find_parent_(processor);
    if (parent) {
      ESP_LOGCONFIG(TAG, "  %s -> %s", parent->get_id(), processor->get_id());
    }
  }
}

Processor *Pipeline::find_next_(Processor *current) {
  // Deep-first search.
  // Does the current processor have a direct child ?
  auto it = this->links_.find(current);
  if (it != this->links_.end() && !it->second.empty())
    return it->second.front();

  // Walk upwards and find the next sibling.
  while (true) {
    // Did we reach the root ?
    auto parent_it = this->parents_.find(current);
    if (parent_it == this->parents_.end())
      return nullptr;

    Processor *parent = parent_it->second;
    auto link_it = this->links_.find(parent);
    if (link_it == this->links_.end())
      return nullptr;

    // Get the next sibling.
    auto &children = this->links_[parent];
    auto pos = std::find(children.begin(), children.end(), current);
    auto next = pos;
    ++next;
    if (pos != children.end() && next != children.end())
      return *next;

    // Move up one node and retry.
    current = parent;
  }
}

Processor *Pipeline::find_parent_(Processor *current) {
  auto it = this->parents_.find(current);
  if (it != this->parents_.end())
    return it->second;

  return nullptr;
}

Processor *Pipeline::find_cycle_() {
  std::unordered_set<Processor *> visited;
  Processor *processor = this->input_;
  while (processor) {
    if (visited.contains(processor))
      return processor;

    visited.insert(processor);
    processor = this->find_next_(processor);
  }

  return nullptr;
}

std::unordered_set<Processor *> Pipeline::find_unlinked_() {
  std::unordered_set<Processor *> unlinked = this->all_processors_;
  Processor *processor = this->input_;
  while (processor) {
    unlinked.erase(processor);
    processor = this->find_next_(processor);
  }
  return unlinked;
}

}  // namespace esphome::camera

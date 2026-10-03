#pragma once

#include "esphome/core/defines.h"

#ifdef USE_API_WIZARD

#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/core/hal.h"
#include "proto.h"
#include "esphome/core/string_ref.h"

// ESP8266 cannot read flash like RAM, so its wizard text stays in PROGMEM and is copied out piece by piece. The
// unit tests define this on the host as well, as the host reads PROGMEM like RAM.
#if defined(USE_ESP8266) && !defined(API_WIZARD_FLASH_STRINGS)
#define API_WIZARD_FLASH_STRINGS
#endif

namespace esphome {

class EntityBase;

namespace api {

// Generated message classes (api_pb2.h) built on the fly from the rows below.
class WizardEntityFilter;
class WizardEntityField;
class WizardInputField;
class WizardPage;
class WizardInputSetRequest;

struct WizardFilterRow;
struct WizardEntityRow;
struct WizardInputRow;
struct WizardPageRow;

/// Size of the buffer holding the entity id of a wizard input. Home Assistant entity ids are at most 255 bytes.
static constexpr size_t WIZARD_ENTITY_ID_BUFFER_SIZE = 256;

#ifdef API_WIZARD_FLASH_STRINGS
/// Copy a PROGMEM string into dst (at most size - 1 bytes) and terminate it. A null source is the empty string.
/// Returns the length copied.
size_t wizard_copy_flash_string(const char *flash, char *dst, size_t size);
#endif

#ifdef USE_API_WIZARD_ENTITY_FILTERS
/// Pointer and count over a constant array. Only used for lists of strings.
template<typename T> struct WizardSpan {
  const T *data;
  size_t count;
  bool empty() const { return this->count == 0; }
  size_t size() const { return this->count; }

#ifdef API_WIZARD_FLASH_STRINGS
  /// Each string is copied out of PROGMEM when dereferenced and only valid until the iterator moves on.
  class Iterator {
   public:
    explicit Iterator(const T *entry) : entry_(entry) {}
    const char *operator*() const {
      wizard_copy_flash_string(progmem_read_ptr(this->entry_), this->buffer_, sizeof(this->buffer_));
      return this->buffer_;
    }
    Iterator &operator++() {
      this->entry_++;
      return *this;
    }
    bool operator!=(const Iterator &other) const { return this->entry_ != other.entry_; }

   protected:
    const T *entry_;
    mutable char buffer_[API_WIZARD_LIST_SCRATCH_SIZE];
  };
  Iterator begin() const { return Iterator(this->data); }
  Iterator end() const { return Iterator(this->data + this->count); }
#else
  const T *begin() const { return this->data; }
  const T *end() const { return this->data + this->count; }
#endif
};
#endif

template<typename Row> struct WizardScratchSize;

#ifdef API_WIZARD_FLASH_STRINGS
/// A message built from a row in PROGMEM. It owns a RAM copy of the row, and the strings the message refers to are
/// copied into its scratch buffer, so it lives exactly as long as the encoder needs it. The generated messages are
/// final, so it wraps one and offers what the generated encoder calls on an element.
template<typename Msg, typename Row> class WizardFlashMessage {
 public:
  explicit WizardFlashMessage(const Row *flash_row);
  WizardFlashMessage(const WizardFlashMessage &) = delete;
  WizardFlashMessage &operator=(const WizardFlashMessage &) = delete;

  static uint8_t *encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
    return Msg::encode_msg(&static_cast<const WizardFlashMessage *>(self)->msg_, buffer PROTO_ENCODE_DEBUG_ARG);
  }
  static uint32_t calc_size_msg(const void *self) {
    return Msg::calc_size_msg(&static_cast<const WizardFlashMessage *>(self)->msg_);
  }
  uint32_t calculate_size() const { return this->msg_.calculate_size(); }
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *dump_to(DumpBuffer &out) const { return this->msg_.dump_to(out); }
#endif

 protected:
  Msg msg_;
  Row row_;
  char scratch_[WizardScratchSize<Row>::VALUE];
};
#endif

/// Fill msg from row. Strings are copied into scratch when they are in PROGMEM, otherwise scratch is not used.
/// msg refers to row, so row must outlive it.
#ifdef USE_API_WIZARD_ENTITY_FILTERS
void wizard_fill(WizardEntityFilter &msg, const WizardFilterRow &row, std::span<char> scratch);
#endif
#ifdef USE_API_WIZARD_ENTITIES
void wizard_fill(WizardEntityField &msg, const WizardEntityRow &row, std::span<char> scratch);
#endif
#ifdef USE_API_WIZARD_INPUTS
void wizard_fill(WizardInputField &msg, const WizardInputRow &row, std::span<char> scratch);
#endif
void wizard_fill(WizardPage &msg, const WizardPageRow &row, std::span<char> scratch);

/// Like WizardSpan, but iterating yields a temporary message built from each row, so the wizard itself stays in
/// flash and only one message at a time exists, on the stack.
template<typename Msg, typename Row> struct WizardView {
  const Row *data;
  size_t count;

  class Iterator {
   public:
    explicit Iterator(const Row *row) : row_(row) {}
#ifdef API_WIZARD_FLASH_STRINGS
    WizardFlashMessage<Msg, Row> operator*() const { return WizardFlashMessage<Msg, Row>(this->row_); }
#else
    Msg operator*() const {
      Msg msg;
      wizard_fill(msg, *this->row_, {});
      return msg;
    }
#endif
    Iterator &operator++() {
      this->row_++;
      return *this;
    }
    bool operator!=(const Iterator &other) const { return this->row_ != other.row_; }

   protected:
    const Row *row_;
  };

  Iterator begin() const { return Iterator(this->data); }
  Iterator end() const { return Iterator(this->data + this->count); }
  bool empty() const { return this->count == 0; }
};

#ifdef USE_API_WIZARD_ENTITY_FILTERS
struct WizardFilterRow {
  const char *integration;
  WizardSpan<const char *> domain;
  WizardSpan<const char *> device_class;
  WizardSpan<const char *> supported_features;
};
#endif

#ifdef USE_API_WIZARD_ENTITIES
// Returns the entity at runtime, because entities are created in setup() and their keys are only known then.
using WizardEntityGetter = EntityBase *(*) ();

struct WizardEntityRow {
  WizardEntityGetter entity;
  const char *description;
};
#endif

#ifdef USE_API_WIZARD_INPUTS
struct WizardInputRow {
  uint32_t key;     // FNV-1 hash of the ESPHome id of the input
  char *entity_id;  // RAM buffer of WIZARD_ENTITY_ID_BUFFER_SIZE bytes, shared with the homeassistant entity
  const char *description;
#ifdef USE_API_WIZARD_ENTITY_FILTERS
  WizardView<WizardEntityFilter, WizardFilterRow> filters;
#endif
};
#endif

// What a page holds depends on what the configuration uses
struct WizardPageRow {
  const char *title;
  const char *description;
#ifdef USE_API_WIZARD_ENTITIES
  WizardView<WizardEntityField, WizardEntityRow> entities;
#endif
#ifdef USE_API_WIZARD_INPUTS
  WizardView<WizardInputField, WizardInputRow> inputs;
#endif
};

#ifdef API_WIZARD_FLASH_STRINGS
// Bytes of PROGMEM text one row copies out at a time, sized by codegen for the longest in the configuration
#ifdef USE_API_WIZARD_ENTITY_FILTERS
template<> struct WizardScratchSize<WizardFilterRow> {
  static constexpr size_t VALUE = API_WIZARD_FILTER_SCRATCH_SIZE;
};
#endif
#ifdef USE_API_WIZARD_ENTITIES
template<> struct WizardScratchSize<WizardEntityRow> { static constexpr size_t VALUE = API_WIZARD_FIELD_SCRATCH_SIZE; };
#endif
#ifdef USE_API_WIZARD_INPUTS
template<> struct WizardScratchSize<WizardInputRow> { static constexpr size_t VALUE = API_WIZARD_FIELD_SCRATCH_SIZE; };
#endif
template<> struct WizardScratchSize<WizardPageRow> { static constexpr size_t VALUE = API_WIZARD_PAGE_SCRATCH_SIZE; };

template<typename Msg, typename Row> WizardFlashMessage<Msg, Row>::WizardFlashMessage(const Row *flash_row) {
  progmem_memcpy(&this->row_, flash_row, sizeof(Row));
  wizard_fill(this->msg_, this->row_, std::span<char>(this->scratch_));
}
#endif

#ifdef USE_API_WIZARD_STANDALONE_INPUTS
/// An input of the wizard that is not tied to an entity of the device. The entity ID the user picks is only ever read
/// by lambdas, for example `id(input).entity_id()`. It lives in the same RAM buffer a linked input uses.
class WizardInput {
 public:
  explicit WizardInput(const char *entity_id) : entity_id_(entity_id) {}
  /// The Home Assistant entity ID, empty until the default or the wizard sets one.
  StringRef entity_id() const { return StringRef(this->entity_id_); }
  bool has_entity_id() const { return this->entity_id_[0] != '\0'; }

 protected:
  const char *entity_id_;
};
#endif

/// The wizard pages, defined by the generated code (components/api/__init__.py) as a constant in flash.
extern const WizardView<WizardPage, WizardPageRow> API_WIZARD_PAGES;

#ifdef USE_API_WIZARD_INPUTS
/// Load the entity ids saved by earlier wizard runs over the defaults in the input buffers. Call once, from setup(),
/// before the entities that use the buffers subscribe.
void wizard_setup();

/// Apply a WizardInputSetRequest: validate it, copy the entity id into the input's buffer and save it.
/// Returns the buffer, or nullptr when the request was ignored.
const char *wizard_set_input(const WizardInputSetRequest &msg);
#endif

}  // namespace api
}  // namespace esphome

#endif  // USE_API_WIZARD

#include "ipp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <strings.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::xerox_phaser_3020 {

ESPHOME_LOG_TAG(TAG, "xerox_phaser_3020.ipp");

// Attribute groups and value tags (RFC 8010)
static constexpr uint8_t GROUP_OPERATION = 0x01;
static constexpr uint8_t GROUP_JOB = 0x02;
static constexpr uint8_t GROUP_END = 0x03;
static constexpr uint8_t GROUP_PRINTER = 0x04;
static constexpr uint8_t VALUE_UNKNOWN = 0x12;
static constexpr uint8_t VALUE_NO_VALUE = 0x13;
static constexpr uint8_t VALUE_INTEGER = 0x21;
static constexpr uint8_t VALUE_BOOLEAN = 0x22;
static constexpr uint8_t VALUE_ENUM = 0x23;
static constexpr uint8_t VALUE_DATETIME = 0x31;
static constexpr uint8_t VALUE_RESOLUTION = 0x32;
static constexpr uint8_t VALUE_RANGE = 0x33;
static constexpr uint8_t VALUE_BEGIN_COLLECTION = 0x34;
static constexpr uint8_t VALUE_END_COLLECTION = 0x37;
static constexpr uint8_t VALUE_TEXT = 0x41;
static constexpr uint8_t VALUE_NAME = 0x42;
static constexpr uint8_t VALUE_KEYWORD = 0x44;
static constexpr uint8_t VALUE_URI = 0x45;
static constexpr uint8_t VALUE_CHARSET = 0x47;
static constexpr uint8_t VALUE_LANGUAGE = 0x48;
static constexpr uint8_t VALUE_MIME = 0x49;
static constexpr uint8_t VALUE_MEMBER = 0x4A;

// Operations
static constexpr uint16_t OP_PRINT_JOB = 0x0002;
static constexpr uint16_t OP_VALIDATE_JOB = 0x0004;
static constexpr uint16_t OP_CREATE_JOB = 0x0005;
static constexpr uint16_t OP_SEND_DOCUMENT = 0x0006;
static constexpr uint16_t OP_CANCEL_JOB = 0x0008;
static constexpr uint16_t OP_GET_JOB_ATTRIBUTES = 0x0009;
static constexpr uint16_t OP_GET_JOBS = 0x000A;
static constexpr uint16_t OP_GET_PRINTER_ATTRIBUTES = 0x000B;
static constexpr uint16_t OP_CANCEL_MY_JOBS = 0x0039;
static constexpr uint16_t OP_CLOSE_JOB = 0x003B;
static constexpr uint16_t OP_IDENTIFY_PRINTER = 0x003C;
static constexpr uint16_t OP_VALIDATE_DOCUMENT = 0x003D;
static constexpr uint16_t SUPPORTED_OPS[] = {
    OP_PRINT_JOB,      OP_VALIDATE_JOB,       OP_CREATE_JOB,       OP_SEND_DOCUMENT,
    OP_CANCEL_JOB,     OP_GET_JOB_ATTRIBUTES, OP_GET_JOBS,         OP_GET_PRINTER_ATTRIBUTES,
    OP_CANCEL_MY_JOBS, OP_CLOSE_JOB,          OP_IDENTIFY_PRINTER, OP_VALIDATE_DOCUMENT,
};

// Status codes
static constexpr uint16_t STATUS_OK = 0x0000;
static constexpr uint16_t STATUS_BAD_REQUEST = 0x0400;
static constexpr uint16_t STATUS_NOT_POSSIBLE = 0x0404;
static constexpr uint16_t STATUS_NOT_FOUND = 0x0406;
static constexpr uint16_t STATUS_FORMAT_NOT_SUPPORTED = 0x040A;
static constexpr uint16_t STATUS_COMPRESSION_NOT_SUPPORTED = 0x040F;
static constexpr uint16_t STATUS_FORMAT_ERROR = 0x0411;
static constexpr uint16_t STATUS_OPERATION_NOT_SUPPORTED = 0x0501;
static constexpr uint16_t STATUS_SERVICE_UNAVAILABLE = 0x0502;
static constexpr uint16_t STATUS_VERSION_NOT_SUPPORTED = 0x0503;
static constexpr uint16_t STATUS_DEVICE_ERROR = 0x0504;
static constexpr uint16_t STATUS_BUSY = 0x0507;

// Job and printer states
static constexpr uint8_t JOB_PENDING = 3;
static constexpr uint8_t JOB_PROCESSING = 5;
static constexpr uint8_t JOB_CANCELED = 7;
static constexpr uint8_t JOB_ABORTED = 8;
static constexpr uint8_t JOB_COMPLETED = 9;
static constexpr uint8_t PRINTER_IDLE = 3;
static constexpr uint8_t PRINTER_PROCESSING = 4;

static constexpr uint8_t QUALITY_DRAFT = 3;
static constexpr uint32_t DOCUMENT_TIMEOUT_S = 60;
static constexpr uint32_t RECEIVE_TIMEOUT_S = 60;
static constexpr size_t MAX_LEFTOVER_BODY = 16 * 1024;
static constexpr int LISTEN_BACKLOG = 4;
#ifdef MSG_NOSIGNAL
static constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
static constexpr int SEND_FLAGS = 0;
#endif

struct JobReasonText {
  const char *keyword;
  const char *message;
};
// Indexed by JobReason
static const JobReasonText JOB_REASONS[] = {
    {"none", ""},
    {"job-incoming", "Waiting for the document"},
    {"job-printing", "Printing"},
    {"printer-stopped", "Waiting for the printer to be switched on"},
    {"job-completed-successfully", "Printed"},
    {"job-canceled-by-user", "Canceled"},
    {"document-format-error", "The document could not be read"},
    {"document-unprintable-error", "The document uses a format this printer cannot print"},
    {"aborted-by-system", "The printer stopped taking data"},
    {"printer-stopped", "The printer is not connected"},
    {"aborted-by-system", "The connection to the client was lost"},
    {"aborted-by-system", "The document never arrived"},
};

static uint32_t uptime_s() { return millis() / 1000 + 1; }

static void copy_string(char *dst, size_t size, const char *src) {
  size_t n = 0;
  for (; src[n] != '\0' && n + 1 < size; n++)
    dst[n] = src[n];
  dst[n] = '\0';
}

// ---------------------------------------------------------------------------------------------------
// HTTP connection: request head parsing and the request body, plain or chunked

class HttpConnection : public ByteSource {
 public:
  explicit HttpConnection(int fd) : fd(fd) {}

  bool read_head();
  int read(uint8_t *buf, size_t len) override;
  bool send(const void *data, size_t len);
  /// Read and drop the rest of the body. Returns false when it is longer than `limit` or the read fails.
  bool skip_body(size_t limit);

  int fd;
  char method[8];
  char path[64];
  char host[96];
  uint32_t remaining{0};  // body bytes left, or bytes left in the current chunk
  bool chunked{false};
  bool expect_continue{false};
  bool keep_alive{false};
  bool body_done{true};
  bool broken{false};

 protected:
  int fill_();
  int read_raw_(uint8_t *dst, size_t len);
  bool read_line_(char *out, size_t size);

  uint16_t pos_{0};
  uint16_t len_{0};
  uint8_t buf_[1024];
};

int HttpConnection::fill_() {
  const ssize_t n = recv(this->fd, this->buf_, sizeof(this->buf_), 0);
  if (n <= 0) {
    this->broken = true;
    return -1;
  }
  this->pos_ = 0;
  this->len_ = n;
  return n;
}

int HttpConnection::read_raw_(uint8_t *dst, size_t len) {
  if (this->pos_ == this->len_) {
    // Large reads go straight into the caller's buffer
    if (len >= sizeof(this->buf_) / 2) {
      const ssize_t n = recv(this->fd, dst, len, 0);
      if (n <= 0) {
        this->broken = true;
        return -1;
      }
      return n;
    }
    if (this->fill_() < 0)
      return -1;
  }
  size_t n = this->len_ - this->pos_;
  if (n > len)
    n = len;
  memcpy(dst, this->buf_ + this->pos_, n);
  this->pos_ += n;
  return n;
}

bool HttpConnection::read_line_(char *out, size_t size) {
  size_t n = 0;
  for (;;) {
    if (this->pos_ == this->len_ && this->fill_() < 0)
      return false;
    const char c = static_cast<char>(this->buf_[this->pos_++]);
    if (c == '\n')
      break;
    if (n + 1 < size)
      out[n++] = c;
  }
  if (n > 0 && out[n - 1] == '\r')
    n--;
  out[n] = '\0';
  return true;
}

static const char *skip_spaces(const char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

bool HttpConnection::read_head() {
  char line[256];
  do {
    if (!this->read_line_(line, sizeof(line)))
      return false;
  } while (line[0] == '\0');

  const char *version = nullptr;
  char *space = strchr(line, ' ');
  if (space == nullptr)
    return false;
  *space = '\0';
  copy_string(this->method, sizeof(this->method), line);
  char *target = space + 1;
  if ((space = strchr(target, ' ')) != nullptr) {
    *space = '\0';
    version = space + 1;
  }
  copy_string(this->path, sizeof(this->path), target);
  this->keep_alive = version != nullptr && strcmp(version, "HTTP/1.1") == 0;
  this->chunked = false;
  this->expect_continue = false;
  this->host[0] = '\0';
  uint32_t content_length = 0;

  for (;;) {
    if (!this->read_line_(line, sizeof(line)))
      return false;
    if (line[0] == '\0')
      break;
    char *colon = strchr(line, ':');
    if (colon == nullptr)
      continue;
    *colon = '\0';
    const char *value = skip_spaces(colon + 1);
    if (strcasecmp(line, "Content-Length") == 0) {
      content_length = strtoul(value, nullptr, 10);
    } else if (strcasecmp(line, "Transfer-Encoding") == 0) {
      this->chunked = strcasecmp(value, "chunked") == 0;
    } else if (strcasecmp(line, "Expect") == 0) {
      this->expect_continue = strcasecmp(value, "100-continue") == 0;
    } else if (strcasecmp(line, "Connection") == 0) {
      if (strcasecmp(value, "close") == 0) {
        this->keep_alive = false;
      } else if (strcasecmp(value, "keep-alive") == 0) {
        this->keep_alive = true;
      }
    } else if (strcasecmp(line, "Host") == 0) {
      copy_string(this->host, sizeof(this->host), value);
    }
  }
  this->remaining = this->chunked ? 0 : content_length;
  this->body_done = !this->chunked && content_length == 0;
  return true;
}

int HttpConnection::read(uint8_t *buf, size_t len) {
  if (this->broken)
    return -1;
  if (this->body_done || len == 0)
    return 0;
  if (this->chunked && this->remaining == 0) {
    char line[48];
    if (!this->read_line_(line, sizeof(line)))
      return -1;
    const uint32_t size = strtoul(line, nullptr, 16);
    if (size == 0) {
      // Skip any trailer lines
      do {
        if (!this->read_line_(line, sizeof(line)))
          return -1;
      } while (line[0] != '\0');
      this->body_done = true;
      return 0;
    }
    this->remaining = size;
  }
  const int n = this->read_raw_(buf, len < this->remaining ? len : this->remaining);
  if (n < 0)
    return -1;
  this->remaining -= n;
  if (this->remaining == 0) {
    if (this->chunked) {
      char crlf[4];
      if (!this->read_line_(crlf, sizeof(crlf)))
        return -1;
    } else {
      this->body_done = true;
    }
  }
  return n;
}

bool HttpConnection::send(const void *data, size_t len) {
  const auto *p = static_cast<const uint8_t *>(data);
  while (len > 0 && !this->broken) {
    const ssize_t n = ::send(this->fd, p, len, SEND_FLAGS);
    if (n <= 0) {
      this->broken = true;
      break;
    }
    p += n;
    len -= n;
  }
  return !this->broken;
}

bool HttpConnection::skip_body(size_t limit) {
  uint8_t tmp[256];
  size_t total = 0;
  int n;
  while ((n = this->read(tmp, sizeof(tmp))) > 0) {
    total += n;
    if (total > limit)
      return false;
  }
  return n == 0;
}

// ---------------------------------------------------------------------------------------------------
// Response writer: HTTP chunked body carrying IPP or HTML

class IppWriter {
 public:
  explicit IppWriter(HttpConnection &conn) : conn_(conn) {}

  void http_head(const char *status, const char *content_type) {
    char head[160];
    const int n = snprintf(head, sizeof(head),
                           "HTTP/1.1 %s\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\nConnection: %s\r\n\r\n",
                           status, content_type, this->conn_.keep_alive ? "keep-alive" : "close");
    this->conn_.send(head, n);
  }
  void raw(const void *data, size_t len) {
    const auto *p = static_cast<const uint8_t *>(data);
    while (len > 0) {
      size_t n = sizeof(this->buf_) - this->len_;
      if (n > len)
        n = len;
      memcpy(this->buf_ + this->len_, p, n);
      this->len_ += n;
      p += n;
      len -= n;
      if (this->len_ == sizeof(this->buf_))
        this->flush_();
    }
  }
  void text(const char *s) { this->raw(s, strlen(s)); }
  void u8(uint8_t v) { this->raw(&v, 1); }
  void u16(uint16_t v) {
    const uint8_t b[2] = {static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
    this->raw(b, 2);
  }
  void u32(uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8),
                          static_cast<uint8_t>(v)};
    this->raw(b, 4);
  }
  /// Attribute header; a null name adds another value to the previous attribute
  void attr(uint8_t tag, const char *name, uint16_t value_len) {
    this->u8(tag);
    const uint16_t name_len = name != nullptr ? strlen(name) : 0;
    this->u16(name_len);
    this->raw(name, name_len);
    this->u16(value_len);
  }
  void string(uint8_t tag, const char *name, const char *value) {
    const uint16_t len = strlen(value);
    this->attr(tag, name, len);
    this->raw(value, len);
  }
  void integer(uint8_t tag, const char *name, int32_t value) {
    this->attr(tag, name, 4);
    this->u32(value);
  }
  void boolean(const char *name, bool value) {
    this->attr(VALUE_BOOLEAN, name, 1);
    this->u8(value ? 1 : 0);
  }
  void range(const char *name, int32_t low, int32_t high) {
    this->attr(VALUE_RANGE, name, 8);
    this->u32(low);
    this->u32(high);
  }
  void resolution(const char *name, int32_t dpi) {
    this->attr(VALUE_RESOLUTION, name, 9);
    this->u32(dpi);
    this->u32(dpi);
    this->u8(3);  // dots per inch
  }
  void out_of_band(uint8_t tag, const char *name) { this->attr(tag, name, 0); }
  void begin_collection(const char *name) { this->attr(VALUE_BEGIN_COLLECTION, name, 0); }
  void member(const char *name) { this->string(VALUE_MEMBER, nullptr, name); }
  void end_collection() { this->attr(VALUE_END_COLLECTION, nullptr, 0); }
  void date_time(const char *name, time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    const uint16_t year = tm.tm_year + 1900;
    const uint8_t v[11] = {static_cast<uint8_t>(year >> 8),
                           static_cast<uint8_t>(year),
                           static_cast<uint8_t>(tm.tm_mon + 1),
                           static_cast<uint8_t>(tm.tm_mday),
                           static_cast<uint8_t>(tm.tm_hour),
                           static_cast<uint8_t>(tm.tm_min),
                           static_cast<uint8_t>(tm.tm_sec),
                           0,
                           '+',
                           0,
                           0};
    this->attr(VALUE_DATETIME, name, sizeof(v));
    this->raw(v, sizeof(v));
  }
  void finish() {
    this->flush_();
    this->conn_.send("0\r\n\r\n", 5);
  }

 protected:
  void flush_() {
    if (this->len_ == 0)
      return;
    char size[12];
    const int n = snprintf(size, sizeof(size), "%x\r\n", this->len_);
    this->conn_.send(size, n);
    this->conn_.send(this->buf_, this->len_);
    this->conn_.send("\r\n", 2);
    this->len_ = 0;
  }

  HttpConnection &conn_;
  uint16_t len_{0};
  uint8_t buf_[512];
};

// ---------------------------------------------------------------------------------------------------
// IPP request

struct IppRequest {
  /// Whether the client asked for this attribute. Without requested-attributes, `all_by_default`
  /// decides, except that job-id and job-uri are always wanted.
  bool wants(const char *name, bool all_by_default = true) const {
    if (!this->has_requested)
      return all_by_default || strcmp(name, "job-id") == 0 || strcmp(name, "job-uri") == 0;
    if (this->requested_all)
      return true;
    for (uint16_t i = 0; i < this->requested_len; i += strlen(this->requested + i) + 1) {
      if (strcmp(this->requested + i, name) == 0)
        return true;
    }
    return false;
  }

  uint32_t request_id{0};
  int32_t job_id{0};
  int32_t limit{0};
  int32_t copies{1};
  int32_t quality{0};
  int32_t media_x{0};
  int32_t media_y{0};
  uint16_t operation{0};
  uint16_t requested_len{0};
  uint16_t ranges[MAX_PAGE_RANGES][2];
  uint8_t num_ranges{0};
  uint8_t major{0};
  uint8_t minor{0};
  bool has_charset{false};
  bool has_language{false};
  bool has_requested{false};
  bool has_target{false};  // printer-uri or job-uri
  bool requested_all{false};
  bool last_document{true};
  bool parse_ok{false};
  char uri[128];  // this printer's URI as the client reaches it
  char format[64];
  char compression[16];
  char user[32];
  char job_name[48];
  char which_jobs[16];
  char media[40];
  char requested[384];
};

static int32_t get_int(const uint8_t *v, uint16_t len) {
  if (len != 4)
    return 0;
  return static_cast<int32_t>((uint32_t{v[0]} << 24) | (uint32_t{v[1]} << 16) | (uint32_t{v[2]} << 8) | v[3]);
}

static bool read_exact(ByteSource *src, uint8_t *dst, size_t len) {
  while (len > 0) {
    const int n = src->read(dst, len);
    if (n <= 0)
      return false;
    dst += n;
    len -= n;
  }
  return true;
}

// Read `len` bytes into a NUL-terminated buffer of `size` bytes, dropping what does not fit
static bool read_text(ByteSource *src, char *dst, size_t size, uint16_t len) {
  const uint16_t keep = len < size - 1 ? len : size - 1;
  if (!read_exact(src, reinterpret_cast<uint8_t *>(dst), keep))
    return false;
  dst[keep] = '\0';
  uint8_t tmp[64];
  for (uint16_t left = len - keep; left > 0;) {
    const uint16_t n = left < sizeof(tmp) ? left : sizeof(tmp);
    if (!read_exact(src, tmp, n))
      return false;
    left -= n;
  }
  return true;
}

static void apply_attribute(IppRequest &req, const char *name, uint8_t tag, const char *value, uint16_t len) {
  const auto *v = reinterpret_cast<const uint8_t *>(value);
  if (strcmp(name, "requested-attributes") == 0) {
    req.has_requested = true;
    if (strcmp(value, "all") == 0 || strcmp(value, "printer-description") == 0 || strcmp(value, "job-template") == 0 ||
        strcmp(value, "job-description") == 0 || strcmp(value, "job-status") == 0 ||
        strcmp(value, "printer-status") == 0) {
      req.requested_all = true;
    } else if (size_t{req.requested_len} + len + 1 <= sizeof(req.requested)) {
      memcpy(req.requested + req.requested_len, value, len + 1);
      req.requested_len += len + 1;
    }
  } else if (strcmp(name, "document-format") == 0) {
    copy_string(req.format, sizeof(req.format), value);
  } else if (strcmp(name, "compression") == 0) {
    copy_string(req.compression, sizeof(req.compression), value);
  } else if (strcmp(name, "requesting-user-name") == 0) {
    copy_string(req.user, sizeof(req.user), value);
  } else if (strcmp(name, "job-name") == 0 || (strcmp(name, "document-name") == 0 && req.job_name[0] == '\0')) {
    copy_string(req.job_name, sizeof(req.job_name), value);
  } else if (strcmp(name, "job-id") == 0) {
    req.job_id = get_int(v, len);
  } else if (strcmp(name, "printer-uri") == 0) {
    req.has_target = true;
  } else if (strcmp(name, "job-uri") == 0) {
    req.has_target = true;
    const char *slash = strrchr(value, '/');
    if (slash != nullptr)
      req.job_id = atoi(slash + 1);
  } else if (strcmp(name, "which-jobs") == 0) {
    copy_string(req.which_jobs, sizeof(req.which_jobs), value);
  } else if (strcmp(name, "limit") == 0) {
    req.limit = get_int(v, len);
  } else if (strcmp(name, "last-document") == 0) {
    req.last_document = len == 1 && v[0] != 0;
  } else if (strcmp(name, "copies") == 0) {
    req.copies = get_int(v, len);
  } else if (strcmp(name, "print-quality") == 0) {
    req.quality = get_int(v, len);
  } else if (strcmp(name, "media") == 0) {
    copy_string(req.media, sizeof(req.media), value);
  } else if (strcmp(name, "page-ranges") == 0 && tag == VALUE_RANGE && len == 8 && req.num_ranges < MAX_PAGE_RANGES) {
    const int32_t low = get_int(v, 4);
    const int32_t high = get_int(v + 4, 4);
    if (low >= 1 && high >= low) {
      req.ranges[req.num_ranges][0] = low > 65535 ? 65535 : low;
      req.ranges[req.num_ranges][1] = high > 65535 ? 65535 : high;
      req.num_ranges++;
    }
  }
}

/// Read the IPP message up to the end of its attributes; the document, if any, follows in `src`.
/// Returns false when the data ends early or is malformed.
static bool parse_request(ByteSource *src, IppRequest &req) {
  uint8_t head[8];
  if (!read_exact(src, head, sizeof(head)))
    return false;
  req.major = head[0];
  req.minor = head[1];
  req.operation = (head[2] << 8) | head[3];
  req.request_id = get_int(head + 4, 4);

  uint8_t group = 0;
  uint8_t operation_attrs = 0;
  char name[48] = "";
  char value[256];
  // Names of the open collections and of the member being read, for media-col/media-size
  char path[2][16] = {"", ""};
  char member[16] = "";
  uint8_t depth = 0;
  for (;;) {
    uint8_t tag;
    if (!read_exact(src, &tag, 1))
      return false;
    if (tag == GROUP_END)
      return true;
    if (tag < 0x10) {
      group = tag;
      continue;
    }
    uint8_t len_bytes[2];
    if (!read_exact(src, len_bytes, 2))
      return false;
    const uint16_t name_len = (len_bytes[0] << 8) | len_bytes[1];
    char this_name[48];
    if (!read_text(src, this_name, sizeof(this_name), name_len) || !read_exact(src, len_bytes, 2))
      return false;
    const uint16_t value_len = (len_bytes[0] << 8) | len_bytes[1];
    if (!read_text(src, value, sizeof(value), value_len))
      return false;
    if (name_len > 0) {
      copy_string(name, sizeof(name), this_name);
      if (group == GROUP_OPERATION && depth == 0) {
        operation_attrs++;
        // The first two operation attributes must be the charset and the language
        if (operation_attrs == 1)
          req.has_charset = tag == VALUE_CHARSET && strcmp(name, "attributes-charset") == 0;
        if (operation_attrs == 2)
          req.has_language = tag == VALUE_LANGUAGE && strcmp(name, "attributes-natural-language") == 0;
      }
    }
    const uint16_t kept = value_len < sizeof(value) - 1 ? value_len : sizeof(value) - 1;
    if (tag == VALUE_BEGIN_COLLECTION) {
      if (depth < 2)
        copy_string(path[depth], sizeof(path[0]), depth == 0 ? name : member);
      depth++;
    } else if (tag == VALUE_END_COLLECTION) {
      if (depth > 0)
        depth--;
    } else if (tag == VALUE_MEMBER) {
      copy_string(member, sizeof(member), value);
    } else if (depth == 0) {
      apply_attribute(req, name, tag, value, kept);
    } else if (depth == 2 && tag == VALUE_INTEGER && strcmp(path[0], "media-col") == 0 &&
               strcmp(path[1], "media-size") == 0) {
      if (strcmp(member, "x-dimension") == 0)
        req.media_x = get_int(reinterpret_cast<const uint8_t *>(value), kept);
      if (strcmp(member, "y-dimension") == 0)
        req.media_y = get_int(reinterpret_cast<const uint8_t *>(value), kept);
    }
  }
}

// Write each item of a comma-separated list as one value of the attribute
static void write_list(IppWriter &w, uint8_t tag, const char *name, const char *list) {
  for (const char *item = list; *item != '\0';) {
    const char *comma = strchr(item, ',');
    const uint16_t len = comma != nullptr ? comma - item : strlen(item);
    w.attr(tag, item == list ? name : nullptr, len);
    w.raw(item, len);
    item += len;
    if (*item == ',')
      item++;
  }
}

static bool list_contains(const char *list, const char *value) {
  const size_t len = strlen(value);
  for (const char *item = list; item != nullptr; item = strchr(item, ',')) {
    if (*item == ',')
      item++;
    if (strncasecmp(item, value, len) == 0 && (item[len] == ',' || item[len] == '\0'))
      return true;
  }
  return false;
}

static void begin_response(IppWriter &w, const IppRequest &req, uint16_t status, const char *message = nullptr) {
  w.http_head("200 OK", "application/ipp");
  w.u8(req.major == 1 ? 1 : 2);
  w.u8(req.major == 1 ? 1 : 0);
  w.u16(status);
  w.u32(req.request_id);
  w.u8(GROUP_OPERATION);
  w.string(VALUE_CHARSET, "attributes-charset", "utf-8");
  w.string(VALUE_LANGUAGE, "attributes-natural-language", "en");
  if (message != nullptr)
    w.string(VALUE_TEXT, "status-message", message);
}

static void end_response(IppWriter &w) {
  w.u8(GROUP_END);
  w.finish();
}

static void write_media_col(IppWriter &w, const char *name, const Paper &paper) {
  w.begin_collection(name);
  w.member("media-size");
  w.begin_collection(nullptr);
  w.member("x-dimension");
  w.integer(VALUE_INTEGER, nullptr, paper.width_hmm);
  w.member("y-dimension");
  w.integer(VALUE_INTEGER, nullptr, paper.height_hmm);
  w.end_collection();
  static const char *const MARGINS[] = {"media-bottom-margin", "media-left-margin", "media-right-margin",
                                        "media-top-margin"};
  for (const char *margin : MARGINS) {
    w.member(margin);
    w.integer(VALUE_INTEGER, nullptr, HARD_MARGIN_HMM);
  }
  w.member("media-source");
  w.string(VALUE_KEYWORD, nullptr, "main");
  w.member("media-type");
  w.string(VALUE_KEYWORD, nullptr, "stationery");
  w.end_collection();
}

static bool clock_is_set(time_t now) { return now > 1577836800; }  // after 2020-01-01

// ---------------------------------------------------------------------------------------------------
// Data for the printer: the PJL header goes out with the first byte, so an empty document sends nothing

class JobSink : public OutputSink {
 public:
  JobSink(PrinterPort *port, const PjlSettings &pjl, const char *user, const char *name)
      : port_(port), pjl_(pjl), user_(user), name_(name) {}

  bool write(const uint8_t *data, size_t len) override {
    if (!this->started_) {
      this->started_ = true;
      uint32_t date = 0;
      const time_t now = time(nullptr);
      if (clock_is_set(now)) {
        struct tm tm;
        gmtime_r(&now, &tm);
        date = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
      }
      if (!write_pjl_header(this->port_, this->pjl_, this->user_, this->name_, date))
        return false;
    }
    return this->port_->write(data, len);
  }
  bool started() const { return this->started_; }

 protected:
  PrinterPort *port_;
  const PjlSettings &pjl_;
  const char *user_;
  const char *name_;
  bool started_{false};
};

// ---------------------------------------------------------------------------------------------------
// Server

void IppServer::setup(const ServerSettings &settings, PrinterPort *port, uint8_t *band, uint8_t *scratch, uint8_t *line,
                      uint8_t *work) {
  this->settings_ = settings;
  this->port_ = port;
  this->encoder_.set_buffers(band, scratch);
  this->raster_.set_buffers(line, work);
}

int IppServer::listen_socket() {
  int fd;
#ifdef IPV6_V6ONLY
  // Dual-stack IPv6 where the network stack has it, IPv4 otherwise
  fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (fd >= 0) {
    int off = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(this->settings_.port);
    addr.sin6_addr = in6addr_any;
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0 && listen(fd, LISTEN_BACKLOG) == 0)
      return fd;
    close(fd);
  }
#endif
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(this->settings_.port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0 || listen(fd, LISTEN_BACKLOG) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

void IppServer::serve(int fd) {
  struct timeval timeout{};
  timeout.tv_sec = RECEIVE_TIMEOUT_S;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  int on = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

  HttpConnection conn(fd);
  while (conn.read_head()) {
    if (strcmp(conn.method, "POST") == 0) {
      this->handle_ipp_(conn);
    } else if (strcmp(conn.method, "GET") == 0) {
      this->handle_status_page_(conn);
    } else {
      conn.keep_alive = false;
      static const char RESPONSE[] =
          "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      conn.send(RESPONSE, sizeof(RESPONSE) - 1);
    }
    if (conn.broken || !conn.keep_alive || !conn.skip_body(MAX_LEFTOVER_BODY))
      break;
  }
  if (!conn.broken && !conn.body_done) {
    // Read what the client is still sending for a while, so closing does not reset the connection
    // and destroy the response before the client reads it
    shutdown(fd, SHUT_WR);
    timeout.tv_sec = 2;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    uint8_t tmp[256];
    for (uint32_t i = 0; i < 4096 && recv(fd, tmp, sizeof(tmp), 0) > 0; i++) {
    }
  }
  close(fd);
}

void IppServer::handle_status_page_(HttpConnection &conn) {
  IppWriter w(conn);
  if (strcmp(conn.path, "/") != 0) {
    w.http_head("404 Not Found", "text/plain");
    w.text("Not found\n");
    w.finish();
    return;
  }
  w.http_head("200 OK", "text/html; charset=utf-8");
  auto escaped = [&w](const char *s) {
    for (; *s != '\0'; s++) {
      switch (*s) {
        case '<':
          w.text("&lt;");
          break;
        case '>':
          w.text("&gt;");
          break;
        case '&':
          w.text("&amp;");
          break;
        case '"':
          w.text("&quot;");
          break;
        default:
          w.raw(s, 1);
      }
    }
  };
  w.text("<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width\">"
         "<title>");
  escaped(this->settings_.name);
  w.text("</title><style>body{font-family:sans-serif;margin:1em}td,th{padding:2px 8px;text-align:left}</style>"
         "</head><body><h1>");
  escaped(this->settings_.name);
  w.text("</h1><p>");
  const bool connected = this->port_->is_connected();
  w.text(!connected                      ? "Printer not connected"
         : this->is_printing()           ? "Printing"
         : this->port_->is_paper_empty() ? "Out of paper"
                                         : "Ready");
  w.text("</p><table><tr><th>Job</th><th>Name</th><th>User</th><th>State</th><th>Pages</th></tr>");
  static const char *const STATES[] = {"pending", "held", "printing", "stopped", "canceled", "aborted", "completed"};
  Job jobs[MAX_JOBS];
  {
    LockGuard guard(this->lock_);
    memcpy(jobs, this->jobs_, sizeof(jobs));
  }
  for (uint32_t shown = 0, last = UINT32_MAX; shown < MAX_JOBS; shown++) {
    // Newest first
    int best = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
      if (jobs[i].id != 0 && jobs[i].id < last && (best < 0 || jobs[i].id > jobs[best].id))
        best = i;
    }
    if (best < 0)
      break;
    const Job &job = jobs[best];
    last = job.id;
    char cell[64];
    snprintf(cell, sizeof(cell), "<tr><td>%u</td><td>", static_cast<unsigned>(job.id));
    w.text(cell);
    escaped(job.name);
    w.text("</td><td>");
    escaped(job.user);
    snprintf(cell, sizeof(cell), "</td><td>%s</td><td>%u</td></tr>",
             job.state >= JOB_PENDING && job.state <= JOB_COMPLETED ? STATES[job.state - JOB_PENDING] : "", job.pages);
    w.text(cell);
  }
  w.text("</table><p>Add this printer on your phone or computer: it is found automatically, or use "
         "<code>ipp://");
  escaped(conn.host);
  w.text("/ipp/print</code></p></body></html>\n");
  w.finish();
}

void IppServer::handle_ipp_(HttpConnection &conn) {
  if (conn.expect_continue)
    conn.send("HTTP/1.1 100 Continue\r\n\r\n", 25);
  // About 1 kB; it lives on the connection task's stack while the job prints
  IppRequest req{};
  req.parse_ok = parse_request(&conn, req);
  if (conn.broken)
    return;
  if (conn.host[0] != '\0') {
    snprintf(req.uri, sizeof(req.uri), "ipp://%s/ipp/print", conn.host);
  } else {
    snprintf(req.uri, sizeof(req.uri), "ipp://localhost:%u/ipp/print", this->settings_.port);
  }

  IppWriter w(conn);
  uint16_t status = STATUS_OK;
  if (!req.parse_ok || !req.has_charset || !req.has_language || !req.has_target || req.request_id == 0) {
    status = STATUS_BAD_REQUEST;
  } else if (req.major != 1 && req.major != 2) {
    status = STATUS_VERSION_NOT_SUPPORTED;
  }
  if (status != STATUS_OK) {
    conn.keep_alive = false;
    begin_response(w, req, status);
    end_response(w);
    return;
  }
  ESP_LOGV(TAG, "IPP operation 0x%04X", req.operation);

  switch (req.operation) {
    case OP_GET_PRINTER_ATTRIBUTES:
      this->op_get_printer_attributes_(w, req, conn);
      break;
    case OP_PRINT_JOB:
      this->op_print_job_(w, req, conn);
      break;
    case OP_VALIDATE_JOB:
    case OP_VALIDATE_DOCUMENT:
      status = this->check_document_(req);
      begin_response(w, req, status);
      end_response(w);
      break;
    case OP_CREATE_JOB:
      this->op_create_job_(w, req);
      break;
    case OP_SEND_DOCUMENT:
      this->op_send_document_(w, req, conn);
      break;
    case OP_CLOSE_JOB:
      this->op_close_job_(w, req);
      break;
    case OP_CANCEL_JOB:
      this->op_cancel_job_(w, req);
      break;
    case OP_CANCEL_MY_JOBS:
      this->op_cancel_my_jobs_(w, req);
      break;
    case OP_GET_JOB_ATTRIBUTES:
      this->op_get_job_attributes_(w, req);
      break;
    case OP_GET_JOBS:
      this->op_get_jobs_(w, req);
      break;
    case OP_IDENTIFY_PRINTER:
      ESP_LOGI(TAG, "Identify-Printer request");
      begin_response(w, req, STATUS_OK);
      end_response(w);
      break;
    default:
      begin_response(w, req, STATUS_OPERATION_NOT_SUPPORTED);
      end_response(w);
      break;
  }
}

uint8_t IppServer::printer_state_() const { return this->is_printing() ? PRINTER_PROCESSING : PRINTER_IDLE; }

void IppServer::op_get_printer_attributes_(IppWriter &w, const IppRequest &req, HttpConnection &conn) {
  begin_response(w, req, STATUS_OK);
  w.u8(GROUP_PRINTER);
  const ServerSettings &s = this->settings_;
  const Paper &paper = PAPERS[s.default_paper];
  const bool connected = this->port_->is_connected();
  const bool paper_empty = connected && this->port_->is_paper_empty();
  const bool printing = this->is_printing();
  const time_t now = time(nullptr);
  auto want = [&req](const char *name) { return req.wants(name); };
  auto keyword = [&](const char *name, const char *value) {
    if (want(name))
      w.string(VALUE_KEYWORD, name, value);
  };
  auto keywords = [&](const char *name, const char *const *values, size_t count) {
    if (!want(name))
      return;
    for (size_t i = 0; i < count; i++)
      w.string(VALUE_KEYWORD, i == 0 ? name : nullptr, values[i]);
  };
  auto integer = [&](uint8_t tag, const char *name, int32_t value) {
    if (want(name))
      w.integer(tag, name, value);
  };
  auto boolean = [&](const char *name, bool value) {
    if (want(name))
      w.boolean(name, value);
  };
  auto text = [&](uint8_t tag, const char *name, const char *value) {
    if (want(name))
      w.string(tag, name, value);
  };
  char buf[128];

  text(VALUE_CHARSET, "charset-configured", "utf-8");
  text(VALUE_CHARSET, "charset-supported", "utf-8");
  boolean("color-supported", false);
  keyword("compression-supported", "none");
  integer(VALUE_INTEGER, "copies-default", 1);
  if (want("copies-supported"))
    w.range("copies-supported", 1, 99);
  text(VALUE_MIME, "document-format-default", "application/octet-stream");
  if (want("document-format-supported"))
    write_list(w, VALUE_MIME, "document-format-supported", DOCUMENT_FORMATS);
  integer(VALUE_ENUM, "finishings-default", 3);
  integer(VALUE_ENUM, "finishings-supported", 3);
  text(VALUE_LANGUAGE, "generated-natural-language-supported", "en");
  keyword("identify-actions-default", "display");
  keyword("identify-actions-supported", "display");
  keyword("ipp-features-supported", "ipp-everywhere");
  static const char *const VERSIONS[] = {"1.1", "2.0"};
  keywords("ipp-versions-supported", VERSIONS, 2);
  static const char *const CREATION[] = {"copies",           "media",         "media-col", "page-ranges",
                                         "print-color-mode", "print-quality", "sides"};
  keywords("job-creation-attributes-supported", CREATION, sizeof(CREATION) / sizeof(CREATION[0]));
  boolean("job-ids-supported", true);
  static const char *const MARGINS[] = {"media-bottom-margin-supported", "media-left-margin-supported",
                                        "media-right-margin-supported", "media-top-margin-supported"};
  for (const char *margin : MARGINS)
    integer(VALUE_INTEGER, margin, HARD_MARGIN_HMM);
  if (want("media-col-default"))
    write_media_col(w, "media-col-default", paper);
  if (want("media-col-ready"))
    write_media_col(w, "media-col-ready", paper);
  if (want("media-col-database")) {
    for (uint8_t i = 0; i < NUM_PAPERS; i++)
      write_media_col(w, i == 0 ? "media-col-database" : nullptr, PAPERS[i]);
  }
  static const char *const MEDIA_COL[] = {"media-size",         "media-bottom-margin", "media-left-margin",
                                          "media-right-margin", "media-top-margin",    "media-source",
                                          "media-type"};
  keywords("media-col-supported", MEDIA_COL, sizeof(MEDIA_COL) / sizeof(MEDIA_COL[0]));
  keyword("media-default", paper.pwg_name);
  keyword("media-ready", paper.pwg_name);
  if (want("media-supported")) {
    for (uint8_t i = 0; i < NUM_PAPERS; i++)
      w.string(VALUE_KEYWORD, i == 0 ? "media-supported" : nullptr, PAPERS[i].pwg_name);
  }
  if (want("media-size-supported")) {
    for (uint8_t i = 0; i < NUM_PAPERS; i++) {
      w.begin_collection(i == 0 ? "media-size-supported" : nullptr);
      w.member("x-dimension");
      w.integer(VALUE_INTEGER, nullptr, PAPERS[i].width_hmm);
      w.member("y-dimension");
      w.integer(VALUE_INTEGER, nullptr, PAPERS[i].height_hmm);
      w.end_collection();
    }
  }
  keyword("media-source-default", "main");
  keyword("media-source-supported", "main");
  keyword("media-type-default", "stationery");
  keyword("media-type-supported", "stationery");
  keyword("multiple-document-handling-supported", "separate-documents-uncollated-copies");
  boolean("multiple-document-jobs-supported", false);
  integer(VALUE_INTEGER, "multiple-operation-time-out", DOCUMENT_TIMEOUT_S);
  keyword("multiple-operation-time-out-action", "abort-job");
  text(VALUE_LANGUAGE, "natural-language-configured", "en");
  if (want("operations-supported")) {
    for (size_t i = 0; i < sizeof(SUPPORTED_OPS) / sizeof(SUPPORTED_OPS[0]); i++)
      w.integer(VALUE_ENUM, i == 0 ? "operations-supported" : nullptr, SUPPORTED_OPS[i]);
  }
  integer(VALUE_ENUM, "orientation-requested-default", 3);
  if (want("orientation-requested-supported")) {
    for (int32_t i = 3; i <= 6; i++)
      w.integer(VALUE_ENUM, i == 3 ? "orientation-requested-supported" : nullptr, i);
  }
  keyword("output-bin-default", "face-down");
  keyword("output-bin-supported", "face-down");
  boolean("page-ranges-supported", true);
  integer(VALUE_INTEGER, "pages-per-minute", 20);
  keyword("pdl-override-supported", "attempted");
  keyword("print-color-mode-default", "monochrome");
  static const char *const COLOR_MODES[] = {"auto", "monochrome"};
  keywords("print-color-mode-supported", COLOR_MODES, 2);
  keyword("print-content-optimize-default", "auto");
  keyword("print-content-optimize-supported", "auto");
  integer(VALUE_ENUM, "print-quality-default", 4);
  if (want("print-quality-supported")) {
    for (int32_t i = 3; i <= 5; i++)
      w.integer(VALUE_ENUM, i == 3 ? "print-quality-supported" : nullptr, i);
  }
  keyword("print-rendering-intent-default", "auto");
  keyword("print-rendering-intent-supported", "auto");
  boolean("preferred-attributes-supported", false);
  text(VALUE_TEXT, "printer-organization", "");
  text(VALUE_TEXT, "printer-organizational-unit", "");
  if (want("printer-geo-location"))
    w.out_of_band(VALUE_UNKNOWN, "printer-geo-location");
  keyword("printer-get-attributes-supported", "document-format");
  text(VALUE_TEXT, "printer-device-id", "MFG:Xerox;MDL:Phaser 3020;CMD:PWGRaster,URF;");
  text(VALUE_NAME, "printer-dns-sd-name", s.name);
  text(VALUE_TEXT, "printer-info", s.name);
  boolean("printer-is-accepting-jobs", true);
  keyword("printer-kind", "document");
  text(VALUE_TEXT, "printer-location", s.location);
  text(VALUE_TEXT, "printer-make-and-model", "Xerox Phaser 3020");
  if (want("printer-more-info")) {
    snprintf(buf, sizeof(buf), "http://%s/", conn.host);
    w.string(VALUE_URI, "printer-more-info", buf);
  }
  text(VALUE_NAME, "printer-name", s.name);
  integer(VALUE_INTEGER, "printer-config-change-time", 1);
  uint32_t state_changed;
  {
    LockGuard guard(this->lock_);
    state_changed = this->state_changed_;
  }
  // Dated back from the current time, as the clock is usually set after boot
  if (clock_is_set(now)) {
    if (want("printer-config-change-date-time"))
      w.date_time("printer-config-change-date-time", now - (uptime_s() - 1));
    if (want("printer-state-change-date-time"))
      w.date_time("printer-state-change-date-time", now - (uptime_s() - state_changed));
  }
  if (want("printer-current-time")) {
    if (clock_is_set(now)) {
      w.date_time("printer-current-time", now);
    } else {
      w.out_of_band(VALUE_UNKNOWN, "printer-current-time");
    }
  }
  if (want("printer-resolution-default"))
    w.resolution("printer-resolution-default", QPDL_DPI);
  if (want("printer-resolution-supported"))
    w.resolution("printer-resolution-supported", QPDL_DPI);
  integer(VALUE_ENUM, "printer-state", this->printer_state_());
  integer(VALUE_INTEGER, "printer-state-change-time", state_changed);
  text(VALUE_TEXT, "printer-state-message",
       !connected    ? "Printer not connected"
       : paper_empty ? "Out of paper"
       : printing    ? "Printing"
                     : "Ready");
  if (want("printer-state-reasons")) {
    // The bridge keeps taking jobs while the printer is off: they wait for it to be switched on
    const char *reason = "none";
    if (!connected) {
      reason = "offline-report";
    } else if (paper_empty) {
      reason = printing ? "media-empty-error" : "media-empty-report";
    }
    w.string(VALUE_KEYWORD, "printer-state-reasons", reason);
  }
  integer(VALUE_INTEGER, "printer-up-time", uptime_s());
  if (want("printer-uri-supported"))
    w.string(VALUE_URI, "printer-uri-supported", req.uri);
  if (want("printer-uuid")) {
    snprintf(buf, sizeof(buf), "urn:uuid:%s", s.uuid);
    w.string(VALUE_URI, "printer-uuid", buf);
  }
  if (want("pwg-raster-document-resolution-supported")) {
    w.resolution("pwg-raster-document-resolution-supported", 300);
    w.resolution(nullptr, 600);
  }
  keyword("pwg-raster-document-sheet-back", "normal");
  static const char *const PWG_TYPES[] = {"black_1", "sgray_8", "srgb_8"};
  keywords("pwg-raster-document-type-supported", PWG_TYPES, 3);
  integer(VALUE_INTEGER, "queued-job-count", printing ? 1 : 0);
  keyword("sides-default", "one-sided");
  keyword("sides-supported", "one-sided");
  keyword("uri-authentication-supported", "none");
  keyword("uri-security-supported", "none");
  if (want("urf-supported"))
    write_list(w, VALUE_KEYWORD, "urf-supported", URF_SUPPORTED);
  static const char *const WHICH_JOBS[] = {"completed", "not-completed", "all"};
  keywords("which-jobs-supported", WHICH_JOBS, 3);
  end_response(w);
}

uint16_t IppServer::check_document_(const IppRequest &req) const {
  if (req.compression[0] != '\0' && strcmp(req.compression, "none") != 0)
    return STATUS_COMPRESSION_NOT_SUPPORTED;
  if (req.format[0] == '\0' || list_contains(DOCUMENT_FORMATS, req.format))
    return STATUS_OK;
  return STATUS_FORMAT_NOT_SUPPORTED;
}

int IppServer::new_job_(const IppRequest &req, uint8_t state) {
  LockGuard guard(this->lock_);
  if (this->active_ >= 0)
    return -1;
  // A free slot, or else the oldest one
  int slot = 0;
  for (int i = 0; i < MAX_JOBS; i++) {
    if (this->jobs_[i].id == 0) {
      slot = i;
      break;
    }
    if (this->jobs_[i].id < this->jobs_[slot].id)
      slot = i;
  }
  Job &job = this->jobs_[slot];
  job = Job{};
  job.id = this->next_id_++;
  job.created = uptime_s();
  job.state = state;
  job.reason = JobReason::JOB_REASON_INCOMING;
  job.copies = req.copies < 1 ? 1 : req.copies > 99 ? 99 : req.copies;
  copy_string(job.name, sizeof(job.name), req.job_name[0] != '\0' ? req.job_name : "Untitled");
  copy_string(job.user, sizeof(job.user), req.user[0] != '\0' ? req.user : "anonymous");
  this->active_ = slot;
  this->doc_deadline_ = job.created + DOCUMENT_TIMEOUT_S;
  this->cancel_.store(false);
  this->pages_.store(0);
  this->printing_.store(true);
  this->state_changed_ = job.created;
  return slot;
}

void IppServer::finish_job_(int slot, uint8_t state, JobReason reason) {
  LockGuard guard(this->lock_);
  Job &job = this->jobs_[slot];
  job.state = state;
  job.reason = reason;
  job.completed = uptime_s();
  job.pages = this->pages_.load();
  if (this->active_ == slot) {
    this->active_ = -1;
    this->printing_.store(false);
  }
  this->state_changed_ = job.completed;
  if (state == JOB_COMPLETED) {
    ESP_LOGI(TAG, "Job %u printed: %u page(s)", static_cast<unsigned>(job.id), job.pages);
  } else {
    ESP_LOGW(TAG, "Job %u %s after %u page(s): %s", static_cast<unsigned>(job.id),
             state == JOB_CANCELED ? LOG_STR_LITERAL("canceled") : LOG_STR_LITERAL("aborted"), job.pages,
             JOB_REASONS[static_cast<uint8_t>(reason)].message);
  }
}

uint16_t IppServer::print_document_(int slot, IppRequest &req, HttpConnection &conn) {
  RasterJob raster_job;
  raster_job.cancel = &this->cancel_;
  raster_job.pages_done = &this->pages_;
  raster_job.default_paper = this->settings_.default_paper;
  raster_job.num_ranges = req.num_ranges;
  memcpy(raster_job.ranges, req.ranges, sizeof(req.ranges));
  raster_job.requested_paper = req.media[0] != '\0' ? find_paper_by_name(req.media) : PAPER_NONE;
  if (raster_job.requested_paper == PAPER_NONE && req.media_x > 0)
    raster_job.requested_paper = find_paper_by_hmm(req.media_x, req.media_y);
  char user[sizeof(Job::user)];
  char name[sizeof(Job::name)];
  uint32_t job_id;
  {
    LockGuard guard(this->lock_);
    Job &job = this->jobs_[slot];
    job.state = JOB_PROCESSING;
    job.processing = uptime_s();
    job.reason = JobReason::JOB_REASON_PRINTING;
    job.has_document = true;
    raster_job.copies = job.copies;
    job_id = job.id;
    memcpy(user, job.user, sizeof(user));
    memcpy(name, job.name, sizeof(name));
  }

  // A printer that is switched off is not on the USB bus: wait for it
  for (uint32_t waited = 0; !this->port_->is_connected(); waited++) {
    if (waited == 0) {
      ESP_LOGI(TAG, "Job waiting for the printer to be connected or switched on");
      LockGuard guard(this->lock_);
      this->jobs_[slot].reason = JobReason::JOB_REASON_WAITING_FOR_PRINTER;
    }
    if (this->cancel_.load()) {
      this->finish_job_(slot, JOB_CANCELED, JobReason::JOB_REASON_CANCELED);
      return STATUS_OK;
    }
    if (waited >= this->settings_.printer_wait_s * 10u) {
      this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_PRINTER_MISSING);
      return STATUS_SERVICE_UNAVAILABLE;
    }
    delay(100);  // NOLINT: runs on a connection task, not the main loop
  }
  {
    LockGuard guard(this->lock_);
    this->jobs_[slot].reason = JobReason::JOB_REASON_PRINTING;
  }
  if (!this->port_->begin_job(&this->cancel_)) {
    this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_PRINTER_ERROR);
    return STATUS_DEVICE_ERROR;
  }
  ESP_LOGI(TAG, "Printing job %u '%s' from %s", static_cast<unsigned>(job_id), name, user);

  PjlSettings pjl = this->settings_.pjl;
  if (req.quality == QUALITY_DRAFT)
    pjl.toner_save = TonerSave::TONER_SAVE_ON;
  JobSink sink(this->port_, pjl, user, name);
  const RasterResult result = this->raster_.print(&conn, &this->encoder_, &sink, raster_job);
  bool delivered = true;
  if (sink.started() && result != RasterResult::RASTER_WRITE_ERROR)
    delivered = write_pjl_footer(&sink);
  if (result == RasterResult::RASTER_CANCELED || this->cancel_.load()) {
    this->port_->abort_job();
  } else {
    delivered = this->port_->end_job() && delivered;
    if (!delivered && this->cancel_.load())
      this->port_->abort_job();
  }

  switch (result) {
    case RasterResult::RASTER_OK:
      break;
    case RasterResult::RASTER_NO_DATA:
      // Send-Document may carry no data, just closing the job
      if (req.operation == OP_SEND_DOCUMENT)
        break;
      this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_FORMAT_ERROR);
      return STATUS_FORMAT_ERROR;
    case RasterResult::RASTER_BAD_FORMAT:
      this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_FORMAT_ERROR);
      return STATUS_FORMAT_ERROR;
    case RasterResult::RASTER_UNSUPPORTED:
      this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_UNSUPPORTED);
      return STATUS_FORMAT_ERROR;
    case RasterResult::RASTER_READ_ERROR:
      this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_CLIENT_GONE);
      return STATUS_BAD_REQUEST;
    case RasterResult::RASTER_WRITE_ERROR:
    case RasterResult::RASTER_CANCELED:
      delivered = false;
      break;
  }
  if (this->cancel_.load()) {
    this->finish_job_(slot, JOB_CANCELED, JobReason::JOB_REASON_CANCELED);
    return STATUS_OK;
  }
  if (!delivered) {
    this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_PRINTER_ERROR);
    return STATUS_DEVICE_ERROR;
  }
  if (req.operation == OP_SEND_DOCUMENT && !req.last_document) {
    // More documents may follow
    LockGuard guard(this->lock_);
    this->jobs_[slot].state = JOB_PENDING;
    this->jobs_[slot].reason = JobReason::JOB_REASON_INCOMING;
    this->doc_deadline_ = uptime_s() + DOCUMENT_TIMEOUT_S;
    return STATUS_OK;
  }
  this->finish_job_(slot, JOB_COMPLETED, JobReason::JOB_REASON_COMPLETED);
  return STATUS_OK;
}

void IppServer::op_print_job_(IppWriter &w, IppRequest &req, HttpConnection &conn) {
  uint16_t status = this->check_document_(req);
  if (status != STATUS_OK) {
    conn.keep_alive = false;
    begin_response(w, req, status);
    end_response(w);
    return;
  }
  const int slot = this->new_job_(req, JOB_PENDING);
  if (slot < 0) {
    conn.keep_alive = false;
    begin_response(w, req, STATUS_BUSY, "The printer is busy with another job");
    end_response(w);
    return;
  }
  status = this->print_document_(slot, req, conn);
  if (conn.broken)
    return;
  if (!conn.body_done)
    conn.keep_alive = false;
  begin_response(w, req, status);
  w.u8(GROUP_JOB);
  this->write_job_status_(w, slot, req.uri);
  end_response(w);
}

void IppServer::op_create_job_(IppWriter &w, IppRequest &req) {
  const uint16_t status = this->check_document_(req);
  if (status != STATUS_OK) {
    begin_response(w, req, status);
    end_response(w);
    return;
  }
  const int slot = this->new_job_(req, JOB_PENDING);
  if (slot < 0) {
    begin_response(w, req, STATUS_BUSY, "The printer is busy with another job");
    end_response(w);
    return;
  }
  begin_response(w, req, STATUS_OK);
  w.u8(GROUP_JOB);
  this->write_job_status_(w, slot, req.uri);
  end_response(w);
}

void IppServer::op_send_document_(IppWriter &w, IppRequest &req, HttpConnection &conn) {
  const int slot = this->find_job_(req);
  uint16_t status = slot < 0 ? STATUS_NOT_FOUND : this->check_document_(req);
  if (status == STATUS_OK) {
    LockGuard guard(this->lock_);
    if (this->active_ != slot || this->jobs_[slot].state != JOB_PENDING)
      status = STATUS_NOT_POSSIBLE;
  }
  if (status != STATUS_OK) {
    conn.keep_alive = false;
    begin_response(w, req, status);
    end_response(w);
    return;
  }
  status = this->print_document_(slot, req, conn);
  if (conn.broken)
    return;
  if (!conn.body_done)
    conn.keep_alive = false;
  begin_response(w, req, status);
  w.u8(GROUP_JOB);
  this->write_job_status_(w, slot, req.uri);
  end_response(w);
}

void IppServer::op_close_job_(IppWriter &w, IppRequest &req) {
  const int slot = this->find_job_(req);
  uint16_t status = slot < 0 ? STATUS_NOT_FOUND : STATUS_OK;
  bool close = false;
  if (status == STATUS_OK) {
    LockGuard guard(this->lock_);
    if (this->active_ == slot && this->jobs_[slot].state == JOB_PENDING) {
      close = true;
    } else if (this->jobs_[slot].state < JOB_CANCELED) {
      status = STATUS_NOT_POSSIBLE;
    }
  }
  if (close)
    this->finish_job_(slot, JOB_COMPLETED, JobReason::JOB_REASON_COMPLETED);
  begin_response(w, req, status);
  end_response(w);
}

void IppServer::op_cancel_job_(IppWriter &w, IppRequest &req) {
  const int slot = this->find_job_(req);
  uint16_t status = slot < 0 ? STATUS_NOT_FOUND : STATUS_OK;
  bool waiting = false;
  if (status == STATUS_OK) {
    LockGuard guard(this->lock_);
    if (this->jobs_[slot].state >= JOB_CANCELED) {
      status = STATUS_NOT_POSSIBLE;
    } else if (this->jobs_[slot].state == JOB_PENDING) {
      waiting = true;  // no document is being printed, so nothing else will finish the job
    } else {
      this->cancel_.store(true);
    }
  }
  if (waiting)
    this->finish_job_(slot, JOB_CANCELED, JobReason::JOB_REASON_CANCELED);
  begin_response(w, req, status, status == STATUS_NOT_POSSIBLE ? "The job has already finished" : nullptr);
  end_response(w);
}

void IppServer::op_cancel_my_jobs_(IppWriter &w, IppRequest &req) {
  int waiting = -1;
  {
    LockGuard guard(this->lock_);
    if (this->active_ >= 0) {
      if (this->jobs_[this->active_].state == JOB_PENDING) {
        waiting = this->active_;
      } else {
        this->cancel_.store(true);
      }
    }
  }
  if (waiting >= 0)
    this->finish_job_(waiting, JOB_CANCELED, JobReason::JOB_REASON_CANCELED);
  begin_response(w, req, STATUS_OK);
  end_response(w);
}

int IppServer::find_job_(const IppRequest &req) const {
  if (req.job_id <= 0)
    return -1;
  LockGuard guard(this->lock_);
  for (int i = 0; i < MAX_JOBS; i++) {
    if (this->jobs_[i].id == static_cast<uint32_t>(req.job_id))
      return i;
  }
  return -1;
}

void IppServer::op_get_job_attributes_(IppWriter &w, IppRequest &req) {
  const int slot = this->find_job_(req);
  if (slot < 0) {
    begin_response(w, req, STATUS_NOT_FOUND);
    end_response(w);
    return;
  }
  begin_response(w, req, STATUS_OK);
  w.u8(GROUP_JOB);
  this->write_job_(w, req, slot, req.uri);
  end_response(w);
}

void IppServer::op_get_jobs_(IppWriter &w, IppRequest &req) {
  const bool completed = strcmp(req.which_jobs, "completed") == 0;
  const bool all = strcmp(req.which_jobs, "all") == 0;
  if (req.which_jobs[0] != '\0' && !completed && !all && strcmp(req.which_jobs, "not-completed") != 0) {
    begin_response(w, req, STATUS_OK);  // other which-jobs values match nothing here
    end_response(w);
    return;
  }
  begin_response(w, req, STATUS_OK);
  Job jobs[MAX_JOBS];
  {
    LockGuard guard(this->lock_);
    memcpy(jobs, this->jobs_, sizeof(jobs));
  }
  // Get-Jobs returns only job-id and job-uri unless other attributes are asked for
  if (!req.has_requested) {
    static constexpr char DEFAULT_ATTRIBUTES[] = "job-id\0job-uri";
    req.has_requested = true;
    memcpy(req.requested, DEFAULT_ATTRIBUTES, sizeof(DEFAULT_ATTRIBUTES));
    req.requested_len = sizeof(DEFAULT_ATTRIBUTES);
  }
  int32_t listed = 0;
  for (uint32_t last = UINT32_MAX;;) {
    int best = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
      if (jobs[i].id != 0 && jobs[i].id < last && (best < 0 || jobs[i].id > jobs[best].id))
        best = i;
    }
    if (best < 0 || (req.limit > 0 && listed >= req.limit))
      break;
    last = jobs[best].id;
    const bool finished = jobs[best].state >= JOB_CANCELED;
    if (!all && finished != completed)
      continue;
    w.u8(GROUP_JOB);
    this->write_job_(w, req, best, req.uri);
    listed++;
  }
  end_response(w);
}

void IppServer::write_job_status_(IppWriter &w, int slot, const char *uri_base) {
  Job job;
  {
    LockGuard guard(this->lock_);
    job = this->jobs_[slot];
  }
  char uri[160];
  snprintf(uri, sizeof(uri), "%s/%u", uri_base, static_cast<unsigned>(job.id));
  w.integer(VALUE_INTEGER, "job-id", job.id);
  w.string(VALUE_URI, "job-uri", uri);
  w.integer(VALUE_ENUM, "job-state", job.state);
  w.string(VALUE_KEYWORD, "job-state-reasons", JOB_REASONS[static_cast<uint8_t>(job.reason)].keyword);
  if (JOB_REASONS[static_cast<uint8_t>(job.reason)].message[0] != '\0')
    w.string(VALUE_TEXT, "job-state-message", JOB_REASONS[static_cast<uint8_t>(job.reason)].message);
}

void IppServer::write_job_(IppWriter &w, const IppRequest &req, int slot, const char *uri_base) {
  Job job;
  bool active;
  {
    LockGuard guard(this->lock_);
    job = this->jobs_[slot];
    active = this->active_ == slot;
  }
  if (active)
    job.pages = this->pages_.load();
  const JobReasonText &reason = JOB_REASONS[static_cast<uint8_t>(job.reason)];
  char uri[160];
  auto want = [&req](const char *name) { return req.wants(name); };
  auto time_attr = [&](const char *name, uint32_t t) {
    if (!want(name))
      return;
    if (t != 0) {
      w.integer(VALUE_INTEGER, name, t);
    } else {
      w.out_of_band(VALUE_NO_VALUE, name);
    }
  };
  if (want("job-id"))
    w.integer(VALUE_INTEGER, "job-id", job.id);
  if (want("job-uri")) {
    snprintf(uri, sizeof(uri), "%s/%u", uri_base, static_cast<unsigned>(job.id));
    w.string(VALUE_URI, "job-uri", uri);
  }
  if (want("job-printer-uri"))
    w.string(VALUE_URI, "job-printer-uri", uri_base);
  if (want("job-name"))
    w.string(VALUE_NAME, "job-name", job.name);
  if (want("job-originating-user-name"))
    w.string(VALUE_NAME, "job-originating-user-name", job.user);
  if (want("job-state"))
    w.integer(VALUE_ENUM, "job-state", job.state);
  if (want("job-state-reasons"))
    w.string(VALUE_KEYWORD, "job-state-reasons", reason.keyword);
  if (want("job-state-message"))
    w.string(VALUE_TEXT, "job-state-message", reason.message);
  if (want("job-printer-up-time"))
    w.integer(VALUE_INTEGER, "job-printer-up-time", uptime_s());
  time_attr("time-at-creation", job.created);
  time_attr("time-at-processing", job.processing);
  time_attr("time-at-completed", job.completed);
  if (want("copies"))
    w.integer(VALUE_INTEGER, "copies", job.copies);
  if (want("number-of-documents"))
    w.integer(VALUE_INTEGER, "number-of-documents", job.has_document ? 1 : 0);
  if (want("job-impressions-completed"))
    w.integer(VALUE_INTEGER, "job-impressions-completed", job.pages);
  if (want("job-media-sheets-completed"))
    w.integer(VALUE_INTEGER, "job-media-sheets-completed", job.pages * job.copies);
}

void IppServer::check_timeouts() {
  int slot = -1;
  {
    LockGuard guard(this->lock_);
    if (this->active_ >= 0 && this->jobs_[this->active_].state == JOB_PENDING &&
        static_cast<int32_t>(uptime_s() - this->doc_deadline_) > 0)
      slot = this->active_;
  }
  if (slot >= 0)
    this->finish_job_(slot, JOB_ABORTED, JobReason::JOB_REASON_TIMED_OUT);
}

}  // namespace esphome::xerox_phaser_3020

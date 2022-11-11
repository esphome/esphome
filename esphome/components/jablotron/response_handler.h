#pragma once
#include "jablotron_device.h"
#include "string_view.h"

namespace esphome {
namespace jablotron {

class ResponseHandler {
 public:
  virtual ~ResponseHandler() = default;
  virtual bool invoke(StringView response) const = 0;

  bool is_last_response() const;

 protected:
  void set_is_last_response_(bool value);

 private:
  bool is_last_response_ = false;
};

class ResponseHandlerError : public ResponseHandler {
 public:
  ResponseHandlerError();
  bool invoke(StringView response) const final;
};

class ResponseHandlerOK : public ResponseHandler {
 public:
  ResponseHandlerOK();
  bool invoke(StringView response) const final;
};

class ResponseHandlerPGState final : public ResponseHandler {
 public:
  explicit ResponseHandlerPGState(const PGDeviceVector &devices);
  bool invoke(StringView response) const final;

 private:
  const PeripheralDeviceVector &devices_;
};

class ResponseHandlerPrfState final : public ResponseHandler {
 public:
  explicit ResponseHandlerPrfState(const PeripheralDeviceVector &devices);
  bool invoke(StringView response) const final;

 private:
  const PeripheralDeviceVector &devices_;
};

class ResponseHandlerState final : public ResponseHandler {
 public:
  explicit ResponseHandlerState(const SectionDeviceVector &devices);
  bool invoke(StringView response) const final;

 private:
  const SectionDeviceVector &devices_;
};

class ResponseHandlerVer final : public ResponseHandler {
 public:
  explicit ResponseHandlerVer(const InfoDeviceVector &devices);
  bool invoke(StringView response) const final;

 private:
  const InfoDeviceVector &devices_;
};

class ResponseHandlerSectionFlag final : public ResponseHandler {
 public:
  explicit ResponseHandlerSectionFlag(const SectionFlagDeviceVector &devices);
  bool invoke(StringView response) const final;

 private:
  const SectionFlagDeviceVector &devices_;
};

}  // namespace jablotron
}  // namespace esphome

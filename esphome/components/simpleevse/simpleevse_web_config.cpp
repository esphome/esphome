#include "simpleevse_web_config.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

#ifdef USE_SIMPLEEVSE_WEB_CONFIG

void SimpleEvseHttpHandler::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == this->set_value_path && req->method() == HTTP_POST) {
    this->handleSetConfig(req);
  } else {
    this->handleIndex(req);
  }
}

void SimpleEvseHttpHandler::handleIndex(AsyncWebServerRequest *req) {
  this->defer([this, req]() {
    AsyncResponseStream *stream = req->beginResponseStream("text/html");
    stream->print(F(
        R"(<!DOCTYPE html>)"
        R"(<html lang="en"><head>)"
        R"(<meta charset=UTF-8>)"
        R"(<style>*{font-family:sans-serif;}table{border-collapse:collapse;}table td,table th{border:1px solid #dfe2e5;padding:6px 13px;}table th{font-weight:600;text-align:center;}tr:nth-child(2n){background-color: #f6f8fa;}</style>)"
        R"(<title>SimpleEvse Config</title>)"
        R"(</head><body>)"
        R"(<h1>SimpleEVSE Configuration Register</h1>)"));

    auto trans = make_unique<ModbusReadHoldingRegistersTransaction>(
        FIRST_CONFIG_REGISTER, COUNT_CONFIG_REGISTER,
        [this, req, stream](ModbusTransactionResult result, std::vector<uint16_t> regs) {
          if (result == ModbusTransactionResult::SUCCESS) {
            stream->print(F(R"(<table>)"
                            R"(<thead><tr><th>Register</th><th>Value</th><th>Update</th></thead>)"
                            R"(<tbody>)"));
            for (uint16_t i = 0; i < regs.size(); ++i) {
              stream->print(F(R"(<tr><td>)"));
              stream->print(i + FIRST_CONFIG_REGISTER);
              stream->print(F(R"(</td><td>)"));
              stream->print(regs[i]);
              stream->print(F(R"(</td><td>)"
                              R"(<form action=")"));
              stream->print(this->set_value_path);
              stream->print(F(R"(" method="post">)"
                              R"(<input type="hidden" name="register" value=")"));
              stream->print(i + FIRST_CONFIG_REGISTER);
              stream->print(F(R"(">)"
                              R"(<input type="number" min="0" max="65535" name="value">)"
                              R"(<input type="submit">)"
                              R"(</form>)"
                              R"(</td></tr>)"));
            }
            stream->print(F(R"(</table>)"
                            R"(</tbody>)"));
          } else {
            stream->print(F(R"(<p>Error requesting registers.</p>)"));
          }

          stream->print(F(R"(</body></html>)"));
          req->send(stream);
        });
    this->parent_->add_transaction(std::move(trans));
  });
}

void SimpleEvseHttpHandler::handleSetConfig(AsyncWebServerRequest *req) {
  if (!req->hasParam("register", true) || !req->hasParam("value", true)) {
    ESP_LOGW(TAG, "Request with missing register and value parameter.");
    req->send(400, "text/plain", "Missing register and/or value.");
    return;
  }

  auto reg = req->getParam("register", true)->value().toInt();
  auto val = req->getParam("value", true)->value().toInt();

  if (reg < FIRST_CONFIG_REGISTER || reg >= (FIRST_CONFIG_REGISTER + COUNT_CONFIG_REGISTER)) {
    ESP_LOGW(TAG, "Invalid register %ld.", reg);
    req->send(400, "text/plain", "Invalid register.");
    return;
  }

  if (val < 0 || val > 0xFFFF) {
    ESP_LOGW(TAG, "Invalid value %ld.", val);
    req->send(400, "text/plain", "Invalid value.");
    return;
  }
  this->defer([this, reg, val, req]() {
    auto trans =
        make_unique<ModbusWriteHoldingRegistersTransaction>(reg, val, [this, req](ModbusTransactionResult result) {
          switch (result) {
            case ModbusTransactionResult::SUCCESS: {
              AsyncWebServerResponse *response = req->beginResponse(303);  // See Other
              response->addHeader("Location", this->index_path);
              req->send(response);
              break;
            }
            case ModbusTransactionResult::EXCEPTION:
              req->send(500, "text/plain", "Error setting value.");
              break;
            case ModbusTransactionResult::TIMEOUT:
              req->send(504, "text/plain", "Timeout setting value.");
              break;
            case ModbusTransactionResult::CANCELLED:
              req->send(503, "text/plain", "Request was cancelled.");
              break;
            default:
              req->send(500, "text/plain", "Unknown error.");
              break;
          }
        });
    this->parent_->add_transaction(std::move(trans));
  });
}

#endif  // USE_SIMPLEEVSE_WEB_CONFIG

}  // namespace simpleevse
}  // namespace esphome

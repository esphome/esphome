#include "telnet_component.h"
#include "wrench.h"

#include "ESPTelnet.h"

namespace esphome {
namespace telnet {

ESPTelnet telnet;
static const char *const TAG = "telnet";
uint16_t port = 23;
WRState *w;
unsigned char *outBytes;  // compiled code is alloc'ed
int outLen;

void print(WRContext *c, const WRValue *argv, const int argn, WRValue &retVal, void *usr) {
  char buf[1024];
  for (int i = 0; i < argn; ++i) {
    printf("%s", argv[i].asString(buf, 1024));
  }
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
  ESP_LOGI(TAG, "%s connected", ip);
  telnet.println("\nWelcome " + telnet.getIP());
  telnet.println("(Use ^] + q  to disconnect.)");
}

void onTelnetDisconnect(String ip) { ESP_LOGI(TAG, "%s disconnected", ip); }

void onTelnetReconnect(String ip) { ESP_LOGI(TAG, "%s reconnected", ip); }

void onTelnetConnectionAttempt(String ip) { ESP_LOGI(TAG, "%s tried to connected", ip); }

void onTelnetInput(String str) {
  // checks for a certain command
  if (str == "ping") {
    telnet.println("> pong");
    ESP_LOGD(TAG, "- Telnet: pong");
    // disconnect the client
  } else if (str == "bye") {
    ESP_LOGI(TAG, "client disconnected");
    telnet.println("> disconnecting you...");
    telnet.disconnectClient();
  } else {
    int err = wr_compile(str.c_str(), strlen(str.c_str()), &outBytes, &outLen);  // compile it
    if (err == 0) {
      wr_run(w, outBytes, outLen);  // load and run the code!
      delete[] outBytes;            // clean up
    }
  }
}

void TelnetComponent::loop() { telnet.loop(); }

void TelnetComponent::setup() {
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetInput);

  Serial.print("- Telnet: ");
  if (telnet.begin(port)) {
    ESP_LOGI(TAG, "Telnet running");
  } else {
    ESP_LOGE(TAG, "Telnet error");
  }
  w = wr_newState();                       // create the state
  wr_registerFunction(w, "print", print);  // bind a function
}

}  // namespace telnet
}  // namespace esphome


#include "openthread_info_text_sensor.h"
#ifdef USE_OPENTHREAD
#include "esphome/core/log.h"

namespace esphome {
namespace openthread_info {

static const char *const TAG = "openthread_info";

void IPAddressOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo IPAddress", this); }
void RoleOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Role", this); }
void ChannelOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Role", this); }
void Rloc16OpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Rloc16", this); }
void ExtAddrOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo ExtAddr", this); }
void Eui64OpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Eui64", this); }
void NetworkNameOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Network Name", this); }
void NetworkKeyOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Network Key", this); }
void PanIdOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo PAN ID", this); }
void ExtPanIdOpenThreadInfo::dump_config() { LOG_TEXT_SENSOR("", "OpenThreadInfo Extended PAN ID", this); }

}  // namespace openthread_info
}  // namespace esphome
#endif

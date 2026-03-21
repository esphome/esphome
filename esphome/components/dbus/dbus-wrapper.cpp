#include "dbus-wrapper.h"
#include "dbus-print-message.h"
#include <cassert>

namespace esphome {
namespace dbus {

static const char *const TAG = "dbus-wrapper";

std::string dbus_iter_to_string(DBusMessageIter *iter) {
  std::string ret = "";
  if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
    char *value = NULL;
    dbus_message_iter_get_basic(iter, &value);
    return value;
  } else if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
    DBusMessageIter variter;
    dbus_message_iter_recurse(iter, &variter);
    return dbus_iter_to_string(&variter);
  } else if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY) {
    DBusMessageIter array_iter;
    dbus_message_iter_recurse(iter, &array_iter);
    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
      ret += dbus_iter_to_string(&array_iter);
      dbus_message_iter_next(&array_iter);
    }

  } else {
    printf("unimplemented type\n");
    print_iter(iter, false);
  }

  return ret;
}

DBusWrapper::~DBusWrapper() {
  dbus_connection_close(this->conn_);
  dbus_connection_unref(this->conn_);
}

void DBusWrapper::setup() {
  printf("======================================== DBusWrapper::setup()\n");
  //  ESP_LOGV(TAG, "%s  DBusWrapper::setup(): '%s'", prefix, (obj)->unique_id().c_str());

  DBusError err;
  // initialiset the errors
  dbus_error_init(&err);
  DBusBusType bus_type;
  if (this->system_) {
    printf("~~~~~~~~~+ DBUS_BUS_SYSTEM\n");
    bus_type = DBUS_BUS_SYSTEM;
  } else {
    printf("~~~~~~~~~+ DBUS_BUS_SESSION\n");
    bus_type = DBUS_BUS_SESSION;
  }
  // create a new connection instead of using an existing connection
  this->conn_ = dbus_bus_get_private(bus_type, &err);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "Connection Error (%s)\n", err.message);
    dbus_error_free(&err);
  }
  if (NULL == this->conn_) {
    exit(1);
  }
}

struct VariantTreeToDbusMessage {
  int indent;
  DBusMessageIter &iter_arg;

  VariantTreeToDbusMessage(int ind, DBusMessageIter &iter_arg) : indent(ind), iter_arg(iter_arg) {}

  void print_indent() const {
    for (int i = 0; i < indent; ++i)
      std::cout << "  ";
  }

  void operator()(std::monostate) const {
    print_indent();
    std::cout << "Null" << std::endl;
  }

  void operator()(int i) const {
    print_indent();
    std::cout << "Integer: " << i << std::endl;
    printf("------ add int %d\n", i);
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_INT32, &i)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
  }

  void operator()(unsigned int u) const {
    print_indent();
    std::cout << "Unsigned integer: " << u << std::endl;
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_UINT32, &u)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
  }

  void operator()(bool b) const {
    print_indent();
    std::cout << "Bool: " << b << std::endl;
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_BOOLEAN, &b)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
  }

  void operator()(double d) const {
    print_indent();
    std::cout << "Double: " << d << std::endl;
  }

  void operator()(const std::string &s) const {
    print_indent();
    std::cout << "String: \"" << s << "\"" << std::endl;
    const char *param = s.c_str();
    printf("------ add arg: [%s]\n", param);
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_STRING, &param)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
  }

  void operator()(const std::shared_ptr<ValueList> &list) const {
    print_indent();
    std::cout << "List [" << list->size() << " Elemente]:" << std::endl;
    if (indent > 0) {
      DBusMessageIter array;
      // FIXME: ins erste element reinschaun
      if (!dbus_message_iter_open_container(&iter_arg, DBUS_TYPE_ARRAY, "s", &array)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      for (const auto &item : *list) {
        std::visit(VariantTreeToDbusMessage{indent + 1, iter_arg}, item.data);
      }
      dbus_message_iter_close_container(&iter_arg, &array);
    } else {
      for (const auto &item : *list) {
        std::visit(VariantTreeToDbusMessage{indent + 1, iter_arg}, item.data);
      }
    }
  }

  void operator()(const std::shared_ptr<ValueDict> &dict) const {
    print_indent();
    std::cout << "Dict {" << dict->size() << " Einträge}:" << std::endl;

    DBusMessageIter array_iter;
    if (!dbus_message_iter_open_container(&iter_arg, DBUS_TYPE_ARRAY, "{sv}", &array_iter)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
    for (const auto &[key, value] : *dict) {
      printf("daaaaaaaaaaaaaaaaaaaaaaaaaa\n");
      print_indent();
      std::cout << "  \"" << key << "\": ";

      DBusMessageIter pairIter;
      if (!dbus_message_iter_open_container(&array_iter, 'e', NULL, &pairIter)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      const char *c_key = key.c_str();
      dbus_message_iter_append_basic(&pairIter, 's', static_cast<void *>(&c_key));

      // Wert auf gleicher Zeile ausgeben wenn primitiv
      if (std::holds_alternative<std::monostate>(value.data)) {
        std::cout << "null" << std::endl;

      } /* else if (std::holds_alternative<std::string>(value.data)) {
          std::string s=std::get<std::string>(value.data);
          std::cout << "\"" << s << "\"";

          DBusMessageIter valueIter;
          if (!dbus_message_iter_open_container(&pairIter, 'v', "s", &valueIter)) {
              fprintf(stderr, "Out Of Memory!\n");
              exit(1);
          }
          const char *c_value=s.c_str();
          dbus_message_iter_append_basic(&valueIter, 's', &c_value);
          dbus_message_iter_close_container(&pairIter, &valueIter);

      } */
      else if (std::holds_alternative<int>(value.data) || std::holds_alternative<unsigned int>(value.data) ||
               std::holds_alternative<bool>(value.data) || std::holds_alternative<double>(value.data) ||
               std::holds_alternative<std::string>(value.data)) {
        printf("daaaaaaaaaaaaaaaaaaaaaaaaaa20\n");
        char type = ' ';
        void **data = NULL;
        const char *dummy = NULL;
        if (std::holds_alternative<int>(value.data)) {
          type = 'i';
          data = (void **) &std::get<int>(value.data);
        } else if (std::holds_alternative<unsigned int>(value.data)) {
          type = 'u';
          data = (void **) &std::get<unsigned int>(value.data);
        } else if (std::holds_alternative<bool>(value.data)) {
          type = 'b';
          data = (void **) &std::get<unsigned int>(value.data);
        } else if (std::holds_alternative<double>(value.data)) {
          type = 'd';
          data = (void **) &std::get<double>(value.data);
        } else if (std::holds_alternative<std::string>(value.data)) {
          type = 's';
          dummy = (std::get<std::string>(value.data).c_str());
          data = (void **) &dummy;
        }

        DBusMessageIter valueIter;
        char stype[2] = " ";
        stype[0] = type;
        if (!dbus_message_iter_open_container(&pairIter, 'v', stype, &valueIter)) {
          fprintf(stderr, "Out Of Memory!\n");
          exit(1);
        }
        dbus_message_iter_append_basic(&valueIter, type, data);
        dbus_message_iter_close_container(&pairIter, &valueIter);

        // std::visit([&pairIter](auto&& arg) {
        std::visit(
            [](auto &&arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, std::string>) {
                std::cout << "\"" << arg << "\"";
              } else if constexpr (std::is_same_v<T, std::monostate>) {
                std::cout << "null";
              } else if constexpr (!std::is_same_v<T, std::shared_ptr<ValueList>> &&
                                   !std::is_same_v<T, std::shared_ptr<ValueDict>>) {
                std::cout << arg;
              }
              printf("visitor done\n");
            },
            value.data);

        std::cout << std::endl;
      } else {
        std::cout << std::endl;
        std::visit(VariantTreeToDbusMessage{indent + 2, array_iter}, value.data);
      }
      dbus_message_iter_close_container(&array_iter, &pairIter);
    }
    dbus_message_iter_close_container(&iter_arg, &array_iter);
  }
};

std::string DBusWrapper::send(const std::string &dbus_destination, const std::string &dbus_path,
                              const std::string &dbus_interface, const std::string &dbus_method,
                              const VariantTree &dbus_args, const std::list<std::string> &properties,
                              const std::string &property_separator) {
  printf("DBus::send [%s] destination:%s, path:%s, interface.method:%s.%s, args:[%d]\n",
         this->system_ ? "SYSTEM" : "SESSION", dbus_destination.c_str(), dbus_path.c_str(), dbus_interface.c_str(),
         dbus_method.c_str());
  dbus_args.print();
  assert(this->conn_ != NULL);

  DBusMessage *msg;
  DBusMessageIter args;
  DBusError err;
  DBusPendingCall *pending;
  dbus_uint32_t level;
  bool stat;

  // initialiset the errors
  dbus_error_init(&err);

  /*
      // request our name on the bus
      int ret = dbus_bus_request_name(conn_, "test.method.caller", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
      if (dbus_error_is_set(&err)) {
          fprintf(stderr, "Name Error (%s)\n", err.message);
          dbus_error_free(&err);
      }
      if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
         exit(1);
      }
     */

  // create a new method call and check for errors
  msg = dbus_message_new_method_call(
      dbus_destination.c_str(),  // destination
      dbus_path.c_str(),         // path
      dbus_interface.c_str(),    // interface -> bei dbus-send ist interface+'.'+method zusammengehängt
      dbus_method.c_str());      // method name "Get"
  if (NULL == msg) {
    fprintf(stderr, "Message Null\n");
    exit(1);
  }

  // append arguments
  // append arguments as list:
  // if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID)) {
  DBusMessageIter iter_arg;
  dbus_message_iter_init_append(msg, &iter_arg);
  printf("==================== VariantTreeToDbusMessage\n");
  std::visit(VariantTreeToDbusMessage{0, iter_arg}, dbus_args.data);

  /*
  std::cout << "Dict {" << dict->size() << " Einträge}:" << std::endl;
  for (const auto &it = [key, value] : *dbus_args) {
    std::cout << "  \"" << key << "\": ";
    if (*it == "bool:True" || *it == "bool:False") {
      dbus_bool_t b = false;
      printf("have type\n");
      if (it->find("True")) {
        printf("true ");
        b = 1;
      }
      if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_BOOLEAN, &b)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      continue;
    }
  if(it->rfind("uint:",0) == 0) {
    int val=std::stoi(it->substr(5));
      printf("------ add uint %d\n");
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_UINT32, &val)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      continue;
  }
  if(it->rfind("int:",0) == 0) {
    int val=std::stoi(it->substr(4));
      printf("------ add int %d\n");
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_INT32, &val)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      continue;
  }
    if(*it == "[]" ) {
      printf("------ add empty array\n");
      DBusMessageIter array;
      if (!dbus_message_iter_open_container(&iter_arg, DBUS_TYPE_ARRAY, "s", &array)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      dbus_message_iter_close_container(&iter_arg, &array);
      continue;
    }
    if(*it == "{}" ) {
      printf("------ add empty dict\n");
      DBusMessageIter dict;
      if (!dbus_message_iter_open_container(&iter_arg, DBUS_TYPE_ARRAY, "{sv}", &dict)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
      }
      dbus_message_iter_close_container(&iter_arg, &dict);
      continue;
    }

    const char *param = it->c_str();
    printf("------ add arg: [%s]\n", param);
    if (!dbus_message_iter_append_basic(&iter_arg, DBUS_TYPE_STRING, &param)) {
      fprintf(stderr, "Out Of Memory!\n");
      exit(1);
    }
  }
  */

  printf("send with reply\n");
  // send message and get a handle for a reply
  if (!dbus_connection_send_with_reply(this->conn_, msg, &pending, -1)) {  // -1 is default timeout
    fprintf(stderr, "Out Of Memory!\n");
    exit(1);
  }
  if (NULL == pending) {
    fprintf(stderr, "Pending Call Null\n");
    exit(1);
  }
  dbus_connection_flush(this->conn_);

  printf("Request Sent\n");

  // free message
  dbus_message_unref(msg);

  // block until we recieve a reply
  dbus_pending_call_block(pending);

  // get the reply message
  msg = dbus_pending_call_steal_reply(pending);
  // free the pending message handle
  dbus_pending_call_unref(pending);
  if (NULL == msg) {
    fprintf(stderr, "Reply Null\n");
    exit(1);
  }

  int message_type = dbus_message_get_type(msg);
  if (message_type == DBUS_MESSAGE_TYPE_ERROR)
    print_message(msg, false);

  // read the returned message
  std::string ret = "";

  std::list<std::string>::iterator it;

  for (auto const &it : properties) {
    if (ret.length() > 0) {
      ret += property_separator;
    }
    ret += this->get_property(msg, it);
  }

  // free reply
  dbus_message_unref(msg);

  return ret;
}

std::string DBusWrapper::get_property(DBusMessage *msg, const std::string &search_key) {
  printf("DBusTextSensor::get_property(msg=%p, search_key='%s')\n", msg, search_key.c_str());
  DBusMessageIter iter, iter_variant;
  char *value_str_ptr;
  std::string ret;

  if (!dbus_message_iter_init(msg, &iter)) {
    fprintf(stderr, "Message has no arguments!\n");
    return "Message has no arguments";
  }
  if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
    printf("get_property dbus_message_iter_get_arg_type[0] is variant\n");
    dbus_message_iter_recurse(&iter, &iter_variant);
    int type = dbus_message_iter_get_arg_type(&iter_variant);
    switch (type) {
      case DBUS_TYPE_STRING:
        dbus_message_iter_get_basic(&iter_variant, &value_str_ptr);
        printf("get_property : %s\n", value_str_ptr);
        return value_str_ptr;
        break;
      case DBUS_TYPE_INT16:
      case DBUS_TYPE_UINT16:
      case DBUS_TYPE_INT32:
      case DBUS_TYPE_UINT32:
      case DBUS_TYPE_INT64:
      case DBUS_TYPE_UINT64: {
        dbus_int64_t i = 0;
        dbus_message_iter_get_basic(&iter_variant, &i);
#ifdef DBUS_INT64_PRINTF_MODIFIER
        printf("get_property : %" DBUS_INT64_PRINTF_MODIFIER "i\n", i);
#else
        printf("get_property : %i\n", (int) i);
#endif
        return std::to_string(i);  // NOLINT << not relevant for host...
        break;
      }
      case DBUS_TYPE_BOOLEAN: {
        dbus_bool_t val;
        dbus_message_iter_get_basic(&iter_variant, &val);
        printf("boolean %s\n", val ? "true" : "false");
        return val ? "true" : "false";
        break;
      }
      case DBUS_TYPE_STRUCT:
        printf("get_property dbus_message_iter_get_arg_type[1] is struct [TODO]\n");
        break;
      case DBUS_TYPE_DICT_ENTRY:
        printf("get_property dbus_message_iter_get_arg_type[1] is dict entry [TODO]\n");
        break;
      case DBUS_TYPE_ARRAY:
        printf("get_property dbus_message_iter_get_arg_type[1] is array\n");
        DBusMessageIter array_iter;
        dbus_message_iter_recurse(&iter_variant, &array_iter);
        while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
          if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_DICT_ENTRY) {
            printf("DBUS_TYPE_DICT_ENTRY\n");
            DBusMessageIter subiter;
            dbus_message_iter_recurse(&array_iter, &subiter);
            printf("dict entry(\n");
            bool add = false;
            char *key = NULL;
            if (dbus_message_iter_get_arg_type(&subiter) == DBUS_TYPE_STRING) {
              dbus_message_iter_get_basic(&subiter, &key);
              printf("key: >>>%s<<<\n", key);
              if (search_key == key) {
                add = true;
              }
            } else {
              printf("---- KEY is not a string!!!\n");
              print_iter(&subiter, false);
            }
            dbus_message_iter_next(&subiter);
            if (add) {
              std::string value = dbus_iter_to_string(&subiter);
              printf("add      value=%s\n", value.c_str());
              return value;
            }
          } else if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT) {
            printf("DBUS_TYPE_STRUCT [TODO]\n");
          } else if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_ARRAY) {
            printf("DBUS_TYPE_ARRAY [TODO]\n");
          } else {
            printf("something else\n");
            print_iter(&array_iter, false);
          }
          dbus_message_iter_next(&array_iter);
        }
        printf("get_property dbus_message_iter_get_arg_type is array done\n");
        break;
      default:
        printf("get_property dbus_message_iter_get_arg_type error\n");
        return "error";
        break;
    }
  } else if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
    printf("get_property dbus_message_iter_get_arg_type[0] is string\n");
    dbus_message_iter_get_basic(&iter, &value_str_ptr);
    printf("get_property : %s\n", value_str_ptr);
    return value_str_ptr;
  } else if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_BOOLEAN) {
    printf("get_property dbus_message_iter_get_arg_type[0] is boolean\n");
    dbus_bool_t val;
    dbus_message_iter_get_basic(&iter, &val);
    printf("boolean %s\n", val ? "true" : "false");
    return val ? "true" : "false";
  } else if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRUCT) {
    printf("get_property dbus_message_iter_get_arg_type[0] is struct\n");
    return "struct";
  } else {
    printf("get_property dbus_message_iter_get_arg_type not variant error\n");
    return "error";
  }
  return "tag not found";
}

// https://github.com/freedesktop/dbus/blob/master/dbus/dbus-bus.c
void DBusWrapper::register_for_signal_(const std::string &dbus_properties, const std::string &dbus_path) {
  printf("DBusWrapper::register_for_signal(properties=%s, path=%s)\n", dbus_properties.c_str(), dbus_path.c_str());
  DBusError err;
  dbus_error_init(&err);

  // add a rule for which messages we want to see
  /*
   * "type='signal',
   * sender='org.freedesktop.DBus',
   * interface='org.freedesktop.DBus',
   * member='Foo',
   * path='/bar/foo',
   * destination=':452345.34'"
   */
  dbus_bus_add_match(this->conn_,
                     ("type='signal',interface='" + dbus_properties + "',path='" + dbus_path + "'").c_str(), &err);
  dbus_connection_flush(this->conn_);
  if (dbus_error_is_set(&err)) {
    fprintf(stderr, "Match Error (%s)\n", err.message);
    exit(1);
  }
  printf("Match rule sent\n");
}

/* no loop here, check for new signals
void DBus::loop() {
    // spam printf("DBusTextSensor::loop\n");

  dbus_connection_read_write(conn_, 0);
    DBusMessage* msg = dbus_connection_pop_message(conn_);
  char* sigvalue=NULL;

      // loop again if we haven't read a message
      if (NULL == msg) {
         // printf("no dbus signal\n");
         return;
      }
    printf("Got Signal with value %s\n", sigvalue);
    print_message(msg, true);
    dbus_message_unref(msg);
//   printf("DBus::loop update\n");
//   this->update(); read message???
    printf("DBus::loop done\n");
}
*/

}  // namespace dbus
}  // namespace esphome

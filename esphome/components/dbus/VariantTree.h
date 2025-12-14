#pragma once
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <memory>

namespace esphome {
namespace dbus {

// Forward-Deklaration
struct VariantTree;

// Container-Typen
using ValueList = std::vector<VariantTree>;
using ValueDict = std::map<std::string, VariantTree>;

// Variant mit rekursiver Fähigkeit
struct VariantTree {
  std::variant<std::monostate,  // Für "leere" oder null-Werte
               int, unsigned int, bool, double, std::string, std::shared_ptr<ValueList>, std::shared_ptr<ValueDict>>
      data;

  // Default-Konstruktor für leere VariantTree
  VariantTree(std::monostate) : data(std::monostate{}) {}
  // VariantTree() : data(-666) {}

  // Expliziter Konstruktor für leere Liste
  // VariantTree(EmptyListTag) : data(std::make_shared<ValueList>()) {}

  // Implizite Konstruktoren
  VariantTree(int i) : data(i) {}
  VariantTree(unsigned int i) : data(i) {}
  VariantTree(bool b) : data(b) {}
  VariantTree(double d) : data(d) {}
  VariantTree(const std::string &s) : data(s) {}
  VariantTree(const char *s) : data(std::string(s)) {}

  // Für Listen
  VariantTree(std::initializer_list<VariantTree> list) : data(std::make_shared<ValueList>(list)) {}
  VariantTree(const ValueList &list) : data(std::make_shared<ValueList>(list)) {}

  // Für Dictionaries
  /*
    VariantTree(std::initializer_list<std::pair<const std::string, VariantTree>> dict)
        : data(std::make_shared<ValueDict>(dict)) {} */
  VariantTree(const ValueDict &dict) : data(std::make_shared<ValueDict>(dict)) {}

  VariantTree() : data(std::make_shared<ValueList>()) { std::cout << "default constructor" << '\n'; }

  // Helper-Methoden
  bool is_null() const { return std::holds_alternative<std::monostate>(data); }

  bool is_list() const { return std::holds_alternative<std::shared_ptr<ValueList>>(data); }

  bool is_dict() const { return std::holds_alternative<std::shared_ptr<ValueDict>>(data); }

  void print() const;
};

// Helper-Funktionen
inline VariantTree dict(std::initializer_list<std::pair<const std::string, VariantTree>> d) { return ValueDict(d); }

inline VariantTree list(std::initializer_list<VariantTree> l) { return ValueList(l); }

// Visitor zum Ausgeben mit Einrückung
struct VariantTreePrinter {
  int indent;

  VariantTreePrinter(int ind = 0) : indent(ind) {}

  void print_indent() const {
    for (int i = 0; i < indent; ++i)
      std::cout << "  ";
  }

  void operator()(std::monostate) const {
    print_indent();
    std::cout << "Null" << '\n';
  }

  void operator()(int i) const {
    print_indent();
    std::cout << "Integer: " << i << '\n';
  }

  void operator()(unsigned int i) const {
    print_indent();
    std::cout << "Unsigned integer: " << i << '\n';
  }

  void operator()(bool b) const {
    print_indent();
    std::cout << "Bool: " << (b ? "true" : "false") << '\n';
  }

  void operator()(double d) const {
    print_indent();
    std::cout << "Double: " << d << '\n';
  }

  void operator()(const std::string &s) const {
    print_indent();
    std::cout << "String: \"" << s << "\"" << '\n';
  }

  void operator()(const std::shared_ptr<ValueList> &list) const {
    print_indent();
    std::cout << "List [" << list->size() << " Elemente]:" << '\n';

    for (const auto &item : *list) {
      std::visit(VariantTreePrinter{indent + 1}, item.data);
    }
  }

  void operator()(const std::shared_ptr<ValueDict> &dict) const {
    print_indent();
    std::cout << "Dict {" << dict->size() << " Einträge}:" << '\n';

    for (const auto &[key, value] : *dict) {
      print_indent();
      std::cout << "  \"" << key << "\": ";

      // Wert auf gleicher Zeile ausgeben wenn primitiv
      if (std::holds_alternative<std::monostate>(value.data)) {
        std::cout << "null" << '\n';
      } else if (std::holds_alternative<int>(value.data) || std::holds_alternative<unsigned int>(value.data) ||
                 std::holds_alternative<bool>(value.data) || std::holds_alternative<double>(value.data) ||
                 std::holds_alternative<std::string>(value.data)) {
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
            },
            value.data);
        std::cout << '\n';
      } else {
        std::cout << '\n';
        std::visit(VariantTreePrinter{indent + 2}, value.data);
      }
    }
  }
};

}  // namespace dbus
}  // namespace esphome

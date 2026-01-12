#include "NoHeapHelperCheck.h"

#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

namespace esphome_tidy {

class ESPHomeModule : public clang::tidy::ClangTidyModule {
 public:
  void addCheckFactories(clang::tidy::ClangTidyCheckFactories &Factories) override {
    Factories.registerCheck<NoHeapHelperCheck>("esphome-no-heap-helpers");
  }
};

}  // namespace esphome_tidy

static clang::tidy::ClangTidyModuleRegistry::Add<esphome_tidy::ESPHomeModule> X("esphome-module",
                                                                                "ESPHome custom clang-tidy checks.");

// Anchor for linking
volatile int ESPHomeModuleAnchorSource = 0;

#pragma once

#include <NewLineActionProvider.h>

#include <optional>
#include <vector>

namespace sweeteditor::qt {

class NewLineActionProviderManager {
public:
  void addProvider(NewLineActionProvider* provider);
  void removeProvider(NewLineActionProvider* provider);
  bool empty() const noexcept;

  std::optional<NewLineAction> createAction(::sweeteditor::Document& document,
                                            const EditorMetadata& metadata,
                                            const ::sweeteditor::TextPosition& position) const;

private:
  std::vector<NewLineActionProvider*> providers_;
};

} // namespace sweeteditor::qt

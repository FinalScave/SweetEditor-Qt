#include "NewLineActionProviderManager.h"

#include <algorithm>

namespace sweeteditor::qt {

void NewLineActionProviderManager::addProvider(NewLineActionProvider* provider) {
  if (provider == nullptr
      || std::find(providers_.begin(), providers_.end(), provider) != providers_.end()) {
    return;
  }

  providers_.push_back(provider);
}

void NewLineActionProviderManager::removeProvider(NewLineActionProvider* provider) {
  providers_.erase(std::remove(providers_.begin(), providers_.end(), provider), providers_.end());
}

bool NewLineActionProviderManager::empty() const noexcept {
  return providers_.empty();
}

std::optional<NewLineAction> NewLineActionProviderManager::createAction(::sweeteditor::Document& document,
                                                                        const EditorMetadata& metadata,
                                                                        const ::sweeteditor::TextPosition& position) const {
  for (NewLineActionProvider* provider : providers_) {
    if (provider == nullptr) {
      continue;
    }

    NewLineAction action = provider->createAction(document, metadata, position);
    if (action.handled) {
      return action;
    }
  }

  return std::nullopt;
}

} // namespace sweeteditor::qt

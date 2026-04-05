#include "CompletionProviderManager.h"

#include <SweetEditorWidget.h>

#include <algorithm>

namespace sweeteditor::qt {

CompletionProviderManager::CompletionProviderManager(SweetEditorWidget* editor) {
  bind(editor);
}

CompletionProviderManager::~CompletionProviderManager() {
  if (debounce_timer_ != nullptr) {
    debounce_timer_->stop();
  }
  providers_.clear();
  merged_items_.clear();
  items_updated_handler_ = {};
  dismissed_handler_ = {};
}

void CompletionProviderManager::bind(SweetEditorWidget* editor) {
  editor_ = editor;
  if (editor_ == nullptr || debounce_timer_ != nullptr) {
    return;
  }

  debounce_timer_ = new QTimer(editor_);
  debounce_timer_->setSingleShot(true);
  QObject::connect(debounce_timer_, &QTimer::timeout, editor_, [this]() {
    executeRefresh(last_trigger_kind_, last_trigger_character_);
  });
}

void CompletionProviderManager::addProvider(CompletionProvider* provider) {
  if (provider == nullptr
      || std::find(providers_.begin(), providers_.end(), provider) != providers_.end()) {
    return;
  }

  providers_.push_back(provider);
}

void CompletionProviderManager::removeProvider(CompletionProvider* provider) {
  providers_.erase(std::remove(providers_.begin(), providers_.end(), provider), providers_.end());
}

bool CompletionProviderManager::empty() const noexcept {
  return providers_.empty();
}

bool CompletionProviderManager::isTriggerCharacter(const QString& text) const {
  for (CompletionProvider* provider : providers_) {
    if (provider != nullptr && provider->isTriggerCharacter(text)) {
      return true;
    }
  }
  return false;
}

void CompletionProviderManager::triggerCompletion(CompletionTriggerKind kind, const QString& trigger_character) {
  if (providers_.empty() || debounce_timer_ == nullptr) {
    return;
  }

  last_trigger_kind_ = kind;
  last_trigger_character_ = trigger_character;
  debounce_timer_->stop();
  debounce_timer_->setInterval(kind == CompletionTriggerKind::Invoked ? 1 : 50);
  debounce_timer_->start();
}

void CompletionProviderManager::dismiss() {
  if (debounce_timer_ != nullptr) {
    debounce_timer_->stop();
  }
  ++generation_;
  merged_items_.clear();
  if (dismissed_handler_) {
    dismissed_handler_();
  }
}

void CompletionProviderManager::showItems(const QList<CompletionItem>& items) {
  if (debounce_timer_ != nullptr) {
    debounce_timer_->stop();
  }
  ++generation_;
  merged_items_ = items;
  std::sort(merged_items_.begin(), merged_items_.end(), completionItemLessThan);
  if (merged_items_.isEmpty()) {
    if (dismissed_handler_) {
      dismissed_handler_();
    }
    return;
  }

  if (items_updated_handler_) {
    items_updated_handler_(merged_items_);
  }
}

void CompletionProviderManager::setItemsUpdatedHandler(
  std::function<void(const QList<CompletionItem>&)> handler) {
  items_updated_handler_ = std::move(handler);
}

void CompletionProviderManager::setDismissedHandler(std::function<void()> handler) {
  dismissed_handler_ = std::move(handler);
}

void CompletionProviderManager::executeRefresh(CompletionTriggerKind kind, const QString& trigger_character) {
  if (providers_.empty()) {
    dismiss();
    return;
  }

  const int current_generation = ++generation_;
  merged_items_.clear();

  const std::optional<CompletionContext> context = buildContext(kind, trigger_character);
  if (!context.has_value()) {
    dismiss();
    return;
  }

  for (CompletionProvider* provider : providers_) {
    if (provider == nullptr || current_generation != generation_) {
      continue;
    }

    CompletionResult result = provider->provideCompletions(*context);
    for (const CompletionItem& item : result.items) {
      merged_items_.push_back(item);
    }
  }

  std::sort(merged_items_.begin(), merged_items_.end(), completionItemLessThan);
  if (merged_items_.isEmpty()) {
    if (dismissed_handler_) {
      dismissed_handler_();
    }
    return;
  }

  if (items_updated_handler_) {
    items_updated_handler_(merged_items_);
  }
}

std::optional<CompletionContext> CompletionProviderManager::buildContext(
  CompletionTriggerKind kind,
  const QString& trigger_character) const {
  if (editor_ == nullptr) {
    return std::nullopt;
  }

  const ::sweeteditor::Document* document = editor_->completionDocument();
  if (document == nullptr) {
    return std::nullopt;
  }

  const ::sweeteditor::TextPosition cursor = editor_->cursorPosition();
  CompletionContext context;
  context.trigger_kind = kind;
  context.trigger_character = trigger_character;
  context.cursor_position = cursor;
  context.line_text = editor_->completionLineText(cursor);
  context.word_range = editor_->completionWordRange();
  context.language_configuration = &editor_->languageConfiguration();
  context.editor_metadata = &editor_->metadata();
  context.document = document;
  return context;
}

bool CompletionProviderManager::completionItemLessThan(const CompletionItem& lhs,
                                                       const CompletionItem& rhs) {
  const QString& lhs_key = lhs.sort_key.isEmpty() ? lhs.label : lhs.sort_key;
  const QString& rhs_key = rhs.sort_key.isEmpty() ? rhs.label : rhs.sort_key;
  if (lhs_key == rhs_key) {
    return lhs.label < rhs.label;
  }
  return lhs_key < rhs_key;
}

} // namespace sweeteditor::qt

#pragma once

#include <CompletionProvider.h>

#include <QTimer>

#include <functional>
#include <optional>
#include <vector>

namespace sweeteditor::qt {

class SweetEditorWidget;

class CompletionProviderManager {
public:
  explicit CompletionProviderManager(SweetEditorWidget* editor = nullptr);
  CompletionProviderManager(const CompletionProviderManager&) = delete;
  CompletionProviderManager& operator=(const CompletionProviderManager&) = delete;
  ~CompletionProviderManager();

  void bind(SweetEditorWidget* editor);
  void addProvider(CompletionProvider* provider);
  void removeProvider(CompletionProvider* provider);
  bool empty() const noexcept;
  bool isTriggerCharacter(const QString& text) const;

  void triggerCompletion(CompletionTriggerKind kind, const QString& trigger_character);
  void dismiss();
  void showItems(const QList<CompletionItem>& items);

  void setItemsUpdatedHandler(std::function<void(const QList<CompletionItem>&)> handler);
  void setDismissedHandler(std::function<void()> handler);

private:
  void executeRefresh(CompletionTriggerKind kind, const QString& trigger_character);
  std::optional<CompletionContext> buildContext(CompletionTriggerKind kind,
                                                const QString& trigger_character) const;
  static bool completionItemLessThan(const CompletionItem& lhs, const CompletionItem& rhs);

  SweetEditorWidget* editor_ {nullptr};
  std::vector<CompletionProvider*> providers_;
  QTimer* debounce_timer_ {nullptr};
  int generation_ {0};
  QList<CompletionItem> merged_items_;
  CompletionTriggerKind last_trigger_kind_ {CompletionTriggerKind::Invoked};
  QString last_trigger_character_;
  std::function<void(const QList<CompletionItem>&)> items_updated_handler_;
  std::function<void()> dismissed_handler_;
};

} // namespace sweeteditor::qt

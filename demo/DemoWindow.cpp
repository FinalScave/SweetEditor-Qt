#include "DemoWindow.h"

#include <CompletionProvider.h>
#include <DecorationProvider.h>
#include <EditorTheme.h>
#include <NewLineActionProvider.h>
#include <SweetEditorWidget.h>

#include <sweeteditor/decoration.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <optional>
#include <algorithm>
#include <iterator>

namespace {

constexpr uint32_t kKeywordStyleId = 1;
constexpr uint32_t kCommentStyleId = 2;
constexpr uint32_t kStringStyleId = 3;

QString fromU16(const ::sweeteditor::U16String& text) {
#ifdef _WIN32
  return QString::fromWCharArray(text.data(), static_cast<qsizetype>(text.size()));
#else
  return QString::fromUtf16(
    reinterpret_cast<const char16_t*>(text.data()),
    static_cast<qsizetype>(text.size())
  );
#endif
}

QString cssColor(const QColor& color) {
  return QStringLiteral("rgba(%1, %2, %3, %4)")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
    .arg(color.alpha());
}

QString wrapModeText(::sweeteditor::WrapMode mode) {
  switch (mode) {
    case ::sweeteditor::WrapMode::NONE:
      return QStringLiteral("NONE");
    case ::sweeteditor::WrapMode::CHAR_BREAK:
      return QStringLiteral("CHAR_BREAK");
    case ::sweeteditor::WrapMode::WORD_BREAK:
      return QStringLiteral("WORD_BREAK");
  }
  return QStringLiteral("UNKNOWN");
}

class DemoDecorationProvider final : public sweeteditor::qt::DecorationProvider {
public:
  sweeteditor::qt::DecorationType capabilities() const noexcept override {
    return sweeteditor::qt::DecorationType::SYNTAX_HIGHLIGHT;
  }

  sweeteditor::qt::DecorationResult provideDecorations(
    const sweeteditor::qt::DecorationContext& context) override {
    sweeteditor::qt::DecorationResult result;
    static const QRegularExpression keyword_regex(
      QStringLiteral("\\b(?:include|int|return|std|cout|endl|auto|const|class|public|private|void)\\b")
    );
    static const QRegularExpression comment_regex(QStringLiteral("//.*$"));
    static const QRegularExpression string_regex(QStringLiteral("\"[^\"]*\""));

    if (context.document == nullptr) {
      return result;
    }

    sweeteditor::qt::DecorationLineMap<::sweeteditor::StyleSpan> syntax_spans;
    for (size_t line = 0; line < context.document->getLineCount(); ++line) {
      const QString line_text = fromU16(context.document->getLineU16Text(line));
      ::sweeteditor::Vector<::sweeteditor::StyleSpan> spans;

      auto add_matches = [&spans, &line_text](const QRegularExpression& regex, uint32_t style_id) {
        auto it = regex.globalMatch(line_text);
        while (it.hasNext()) {
          const auto match = it.next();
          spans.push_back({
            static_cast<uint32_t>(match.capturedStart()),
            static_cast<uint32_t>(match.capturedLength()),
            style_id,
          });
        }
      };

      add_matches(keyword_regex, kKeywordStyleId);
      add_matches(comment_regex, kCommentStyleId);
      add_matches(string_regex, kStringStyleId);
      syntax_spans.emplace(line, std::move(spans));
    }

    result.syntax_spans = std::move(syntax_spans);
    result.syntax_spans_mode = sweeteditor::qt::DecorationApplyMode::REPLACE_ALL;
    return result;
  }
};

class DemoCompletionProvider final : public sweeteditor::qt::CompletionProvider {
public:
  bool isTriggerCharacter(const QString& text) const override {
    return text == QStringLiteral(".") || text == QStringLiteral(":");
  }

  sweeteditor::qt::CompletionResult provideCompletions(
    const sweeteditor::qt::CompletionContext& context) override {
    const QString current_word = extractCurrentWord(context);
    const ::sweeteditor::TextRange replace_range = context.word_range.value_or(::sweeteditor::TextRange {
      context.cursor_position,
      context.cursor_position,
    });

    const QList<sweeteditor::qt::CompletionItem> completions = {
      {
        QStringLiteral("std::cout"),
        QStringLiteral("C++ output stream"),
        QStringLiteral("std::cout"),
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_PLAIN_TEXT,
        std::nullopt,
        QStringLiteral("cout"),
        QStringLiteral("a_cout"),
        sweeteditor::qt::CompletionItem::KIND_VARIABLE,
      },
      {
        QStringLiteral("std::endl"),
        QStringLiteral("Flush line ending"),
        QStringLiteral("std::endl"),
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_PLAIN_TEXT,
        std::nullopt,
        QStringLiteral("endl"),
        QStringLiteral("b_endl"),
        sweeteditor::qt::CompletionItem::KIND_VARIABLE,
      },
      {
        QStringLiteral("if"),
        QStringLiteral("Snippet"),
        QStringLiteral("if (${1:condition}) {\n    $0\n}"),
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_SNIPPET,
        std::nullopt,
        {},
        QStringLiteral("c_if"),
        sweeteditor::qt::CompletionItem::KIND_SNIPPET,
      },
      {
        QStringLiteral("main"),
        QStringLiteral("Main entry function"),
        {},
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_PLAIN_TEXT,
        sweeteditor::qt::CompletionItem::TextEdit {
          replace_range,
          QStringLiteral("main")
        },
        QStringLiteral("main"),
        QStringLiteral("d_main"),
        sweeteditor::qt::CompletionItem::KIND_FUNCTION,
      },
      {
        QStringLiteral("return"),
        QStringLiteral("Return statement"),
        QStringLiteral("return "),
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_PLAIN_TEXT,
        std::nullopt,
        QStringLiteral("return"),
        QStringLiteral("e_return"),
        sweeteditor::qt::CompletionItem::KIND_KEYWORD,
      },
      {
        QStringLiteral("class"),
        QStringLiteral("Class declaration"),
        QStringLiteral("class "),
        sweeteditor::qt::CompletionItem::INSERT_TEXT_FORMAT_PLAIN_TEXT,
        std::nullopt,
        QStringLiteral("class"),
        QStringLiteral("f_class"),
        sweeteditor::qt::CompletionItem::KIND_CLASS,
      },
    };

    sweeteditor::qt::CompletionResult result;
    for (const auto& item : completions) {
      const QString match_text = item.matchText();
      if (current_word.isEmpty() || match_text.startsWith(current_word, Qt::CaseInsensitive)
          || item.label.startsWith(current_word, Qt::CaseInsensitive)) {
        result.items.push_back(item);
      }
    }
    return result;
  }

private:
  static QString extractCurrentWord(const sweeteditor::qt::CompletionContext& context) {
    if (!context.word_range.has_value()) {
      return {};
    }

    const auto& range = *context.word_range;
    if (range.start.line != range.end.line || range.start.line != context.cursor_position.line) {
      return {};
    }

    const int start = static_cast<int>(range.start.column);
    const int length = static_cast<int>(range.end.column - range.start.column);
    if (start < 0 || length <= 0 || start + length > context.line_text.size()) {
      return {};
    }
    return context.line_text.mid(start, length);
  }
};

class DemoNewLineProvider final : public sweeteditor::qt::NewLineActionProvider {
public:
  sweeteditor::qt::NewLineAction createAction(::sweeteditor::Document& document,
                                              const sweeteditor::qt::EditorMetadata&,
                                              const ::sweeteditor::TextPosition& position) override {
    const QString line_text = fromU16(document.getLineU16Text(position.line));
    const QString prefix = line_text.left(static_cast<qsizetype>(position.column));

    QString indent;
    for (const QChar ch : prefix) {
      if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t')) {
        indent.push_back(ch);
      } else {
        break;
      }
    }

    if (!prefix.trimmed().endsWith(QLatin1Char('{'))) {
      return {};
    }

    return {
      true,
      QStringLiteral("\n") + indent + QStringLiteral("    "),
    };
  }
};

QString resolveAssetCandidate(const QString& candidate) {
  if (candidate.isEmpty()) {
    return {};
  }

  QDir dir(candidate);
  if (!dir.exists()) {
    return {};
  }
  if (dir.exists(QStringLiteral("files"))) {
    return dir.absolutePath();
  }

  const QString assets_dir = dir.filePath(QStringLiteral("assets"));
  const QDir assets(assets_dir);
  if (assets.exists() && assets.exists(QStringLiteral("files"))) {
    return assets.absolutePath();
  }
  return {};
}

} // namespace

struct DemoWindow::Private {
  DemoDecorationProvider decoration_provider;
  DemoCompletionProvider completion_provider;
  DemoNewLineProvider newline_provider;
};

DemoWindow::DemoWindow(QWidget* parent)
  : QWidget(parent)
  , d_(std::make_unique<Private>()) {
  setObjectName(QStringLiteral("demoWindow"));
  assets_root_ = resolveAssetsRoot();
  setupUi();
  setupDemoProviders();
  editor_->settings().setCurrentLineRenderMode(::sweeteditor::CurrentLineRenderMode::BORDER);
  syncTheme();
  setupFileSelector();
  resize(1120, 760);
}

DemoWindow::~DemoWindow() = default;

void DemoWindow::setupUi() {
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(0);

  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("demoToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(10, 8, 10, 8);
  toolbar_layout->setSpacing(8);

  file_combo_ = new QComboBox(toolbar);
  file_combo_->setMinimumContentsLength(18);
  file_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  QObject::connect(file_combo_,
                   QOverload<int>::of(&QComboBox::currentIndexChanged),
                   this,
                   [this](int index) {
                     if (suppress_file_selection_ || index < 0 || index >= demo_files_.size()) {
                       return;
                     }
                     loadDemoFile(demo_files_.at(index));
                   });

  auto* undo_button = new QPushButton(QStringLiteral("Undo"), toolbar);
  QObject::connect(undo_button, &QPushButton::clicked, this, [this]() {
    if (editor_->canUndo()) {
      editor_->undo();
      updateStatus(QStringLiteral("Undo"));
      return;
    }
    updateStatus(QStringLiteral("Nothing to undo"));
  });

  auto* redo_button = new QPushButton(QStringLiteral("Redo"), toolbar);
  QObject::connect(redo_button, &QPushButton::clicked, this, [this]() {
    if (editor_->canRedo()) {
      editor_->redo();
      updateStatus(QStringLiteral("Redo"));
      return;
    }
    updateStatus(QStringLiteral("Nothing to redo"));
  });

  auto* theme_button = new QPushButton(QStringLiteral("Toggle Theme"), toolbar);
  QObject::connect(theme_button, &QPushButton::clicked, this, [this]() {
    is_dark_theme_ = !is_dark_theme_;
    syncTheme();
    updateStatus(is_dark_theme_
                   ? QStringLiteral("Switched to dark theme")
                   : QStringLiteral("Switched to light theme"));
  });

  auto* wrap_button = new QPushButton(QStringLiteral("WrapMode"), toolbar);
  QObject::connect(wrap_button, &QPushButton::clicked, this, [this]() {
    cycleWrapMode();
  });

  auto* perf_button = new QPushButton(QStringLiteral("Perf"), toolbar);
  QObject::connect(perf_button, &QPushButton::clicked, this, [this]() {
    const bool enabled = !editor_->isPerfOverlayEnabled();
    editor_->setPerfOverlayEnabled(enabled);
    updateStatus(enabled ? QStringLiteral("Perf overlay enabled") : QStringLiteral("Perf overlay disabled"));
  });

  status_label_ = new QLabel(QStringLiteral("Ready"), toolbar);
  status_label_->setObjectName(QStringLiteral("statusLabel"));

  toolbar_layout->addWidget(file_combo_);
  toolbar_layout->addWidget(undo_button);
  toolbar_layout->addWidget(redo_button);
  toolbar_layout->addWidget(theme_button);
  toolbar_layout->addWidget(wrap_button);
  toolbar_layout->addWidget(perf_button);
  toolbar_layout->addStretch(1);
  toolbar_layout->addWidget(status_label_);

  editor_ = new sweeteditor::qt::SweetEditorWidget(this);
  editor_->setObjectName(QStringLiteral("demoEditor"));

  root_layout->addWidget(toolbar);
  root_layout->addWidget(editor_, 1);
}

void DemoWindow::setupDemoProviders() {
  editor_->addDecorationProvider(&d_->decoration_provider);
  editor_->addCompletionProvider(&d_->completion_provider);
  editor_->addNewLineActionProvider(&d_->newline_provider);
}

void DemoWindow::setupFileSelector() {
  demo_files_ = listDemoFiles(assets_root_);

  suppress_file_selection_ = true;
  file_combo_->clear();
  for (const QString& file_path : demo_files_) {
    file_combo_->addItem(QFileInfo(file_path).fileName());
  }
  file_combo_->setEnabled(!demo_files_.isEmpty());
  suppress_file_selection_ = false;

  if (demo_files_.isEmpty()) {
    loadDemoText(QStringLiteral("demo.cpp"), defaultSampleCode());
    updateStatus(assets_root_.isEmpty()
                   ? QStringLiteral("Assets not found, loaded fallback sample")
                   : QStringLiteral("No demo files found, loaded fallback sample"));
    suppress_file_selection_ = false;
    return;
  }

  file_combo_->setCurrentIndex(-1);
  file_combo_->setCurrentIndex(0);
  suppress_file_selection_ = false;
  loadDemoFile(demo_files_.front());
}

void DemoWindow::syncTheme() {
  const auto theme = is_dark_theme_ ? sweeteditor::qt::EditorTheme::dark()
                                    : sweeteditor::qt::EditorTheme::light();
  editor_->applyTheme(theme);
  registerTextStyles();

  setStyleSheet(QStringLiteral(
                  "#demoWindow { background: %1; }"
                  "#demoToolbar { background: %2; border-bottom: 1px solid %3; }"
                  "#demoToolbar QLabel { color: %4; }"
                  "#demoToolbar QComboBox, #demoToolbar QPushButton {"
                  "  background: %5;"
                  "  color: %4;"
                  "  border: 1px solid %3;"
                  "  border-radius: 4px;"
                  "  padding: 5px 10px;"
                  "}"
                  "#demoToolbar QPushButton:hover { border-color: %6; }"
                  "#demoToolbar QComboBox QAbstractItemView {"
                  "  background: %2;"
                  "  color: %4;"
                  "  selection-background-color: %7;"
                  "  selection-color: %4;"
                  "}"
                )
                  .arg(cssColor(theme.background))
                  .arg(cssColor(theme.gutter_background))
                  .arg(cssColor(theme.border))
                  .arg(cssColor(theme.foreground))
                  .arg(cssColor(theme.current_line))
                  .arg(cssColor(theme.accent))
                  .arg(cssColor(theme.selection)));
}

void DemoWindow::registerTextStyles() {
  const bool dark = is_dark_theme_;
  editor_->editorCore().registerTextStyle(kKeywordStyleId, {
    dark ? static_cast<int32_t>(0xFF79C0FFu) : static_cast<int32_t>(0xFF0550AEu),
    0,
    ::sweeteditor::FONT_STYLE_NORMAL
  });
  editor_->editorCore().registerTextStyle(kCommentStyleId, {
    dark ? static_cast<int32_t>(0xFF8B949Eu) : static_cast<int32_t>(0xFF6E7781u),
    0,
    ::sweeteditor::FONT_STYLE_ITALIC
  });
  editor_->editorCore().registerTextStyle(kStringStyleId, {
    dark ? static_cast<int32_t>(0xFFA5D6FFu) : static_cast<int32_t>(0xFF116329u),
    0,
    ::sweeteditor::FONT_STYLE_NORMAL
  });
  editor_->flush();
}

void DemoWindow::updateStatus(const QString& message) {
  status_label_->setText(message);
}

void DemoWindow::loadDemoFile(const QString& file_path) {
  if (editor_->loadFile(file_path)) {
    const QString file_name = QFileInfo(file_path).fileName();
    setWindowTitle(QStringLiteral("SweetEditorQt Demo - %1").arg(file_name));
    updateStatus(QStringLiteral("Loaded: %1").arg(file_name));
    return;
  }

  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    loadDemoText(QFileInfo(file_path).fileName(), defaultSampleCode(), file_path);
    updateStatus(QStringLiteral("Failed to load %1, fallback sample used").arg(QFileInfo(file_path).fileName()));
    return;
  }

  loadDemoText(QFileInfo(file_path).fileName(), QString::fromUtf8(file.readAll()), file_path);
}

void DemoWindow::loadDemoText(const QString& display_name, const QString& text, const QString& file_path) {
  editor_->loadText(normalizeNewlines(text));
  editor_->setMetadata({file_path, display_name, QStringLiteral("UTF-8")});
  setWindowTitle(QStringLiteral("SweetEditorQt Demo - %1").arg(display_name));
  updateStatus(QStringLiteral("Loaded: %1").arg(display_name));
}

void DemoWindow::cycleWrapMode() {
  const auto wrap_modes = {
    ::sweeteditor::WrapMode::NONE,
    ::sweeteditor::WrapMode::CHAR_BREAK,
    ::sweeteditor::WrapMode::WORD_BREAK,
  };

  auto it = std::find(wrap_modes.begin(), wrap_modes.end(), wrap_mode_preset_);
  if (it == wrap_modes.end() || std::next(it) == wrap_modes.end()) {
    wrap_mode_preset_ = *wrap_modes.begin();
  } else {
    wrap_mode_preset_ = *std::next(it);
  }

  editor_->settings().setWrapMode(wrap_mode_preset_);
  updateStatus(QStringLiteral("WrapMode: %1").arg(wrapModeText(wrap_mode_preset_)));
}

QString DemoWindow::normalizeNewlines(QString text) {
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  return text;
}

QString DemoWindow::resolveAssetsRoot() {
  const QString env_path = qEnvironmentVariable("SWEETEDITOR_DEMO_RES_DIR");
  if (const QString candidate = resolveAssetCandidate(env_path); !candidate.isEmpty()) {
    return candidate;
  }

  const QStringList starts = {
    QCoreApplication::applicationDirPath(),
    QDir::currentPath(),
  };
  for (const QString& start : starts) {
    QDir dir(start);
    while (dir.exists()) {
      if (const QString candidate = resolveAssetCandidate(dir.absolutePath()); !candidate.isEmpty()) {
        return candidate;
      }
      if (!dir.cdUp()) {
        break;
      }
    }
  }

  return {};
}

QStringList DemoWindow::listDemoFiles(const QString& assets_root) {
  if (assets_root.isEmpty()) {
    return {};
  }

  const QDir files_dir(QDir(assets_root).filePath(QStringLiteral("files")));
  if (!files_dir.exists()) {
    return {};
  }

  const QFileInfoList entries = files_dir.entryInfoList(
    QDir::Files | QDir::NoSymLinks,
    QDir::Name | QDir::IgnoreCase
  );

  QStringList result;
  result.reserve(entries.size());
  for (const QFileInfo& entry : entries) {
    result.push_back(entry.absoluteFilePath());
  }
  return result;
}

QString DemoWindow::defaultSampleCode() {
  return QStringLiteral(
    "#include <iostream>\n"
    "\n"
    "int main() {\n"
    "    std::cout << \"SweetEditor Qt demo\" << std::endl; // try Ctrl+Space\n"
    "    return 0;\n"
    "}\n"
  );
}

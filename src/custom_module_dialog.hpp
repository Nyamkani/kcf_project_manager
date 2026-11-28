#pragma once
#include "kcf_project/project.hpp"
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>

// Selection only: writes no files. The existing Preview / Save Settings action
// remains the single apply step in the main window.
class CustomModuleDialog : public QDialog {
    kpm::fs::path root_;
    kpm::Json build_;
    std::string element_;
    QLineEdit *module_, *sourceDirectory_;
    QPlainTextEdit *headers_, *sources_, *preview_;
    QLabel* error_;
    QPushButton* accept_;
    kpm::Json merged_;
    QString storedPath(const QString& selected) const {
        auto path = kpm::fs::canonical(kpm::fs::path(selected.toStdString()));
        auto relative = path.lexically_relative(root_);
        if (!relative.empty() && *relative.begin() != "..") return QString::fromStdString(relative.generic_string());
        return QString::fromStdString(path.generic_string()); // Explicit external selection.
    }
    QString browseStart(const QLineEdit* entry) const {
        kpm::fs::path path(entry->text().toStdString());
        if (path.empty()) return QString::fromStdString(root_.string());
        return QString::fromStdString((path.is_absolute() ? path : root_ / path).string());
    }
    static kpm::Json lines(const QPlainTextEdit* edit) {
        auto result = kpm::Json::array();
        for (const auto& line : edit->toPlainText().split('\n'))
            if (!line.isEmpty()) result.push_back(line.toStdString());
        return result;
    }
public:
    CustomModuleDialog(const kpm::fs::path& root, const kpm::Json& build,
                       const std::string& element, QWidget* parent = nullptr)
        : QDialog(parent), root_(kpm::fs::canonical(root)), build_(build), element_(element) {
        setWindowTitle("Add Custom Module"); resize(850, 700);
        auto* layout = new QVBoxLayout(this);
        auto* hint = new QLabel("Paths are relative to the project root, not the module directory.\n"
            "External directories/files must be explicitly absolute. Select actual .cpp files; no directory scan is performed.\n"
            "This adds to the draft CMake Build lists. Use Preview / Save Settings in the main window to apply.");
        hint->setWordWrap(true); layout->addWidget(hint);
        auto* form = new QFormLayout; layout->addLayout(form);
        auto directory = [&](const QString& title, const QString& example) {
            auto* row = new QHBoxLayout; auto* entry = new QLineEdit; entry->setPlaceholderText(example);
            auto* browse = new QPushButton("Browse directory…"); row->addWidget(entry); row->addWidget(browse); form->addRow(title, row);
            connect(browse, &QPushButton::clicked, this, [this, entry, title] {
                auto path = QFileDialog::getExistingDirectory(this, title, browseStart(entry));
                if (!path.isEmpty()) entry->setText(storedPath(path));
            });
            return entry;
        };
        module_ = directory("Module directory", "test_module/module1");
        sourceDirectory_ = directory("Source directory (browse boundary only)", "test_module/module1/src");
        headers_ = new QPlainTextEdit; headers_->setMaximumHeight(75); headers_->setPlaceholderText("test_module/module1/include");
        auto* headerRow = new QHBoxLayout; headerRow->addWidget(headers_); auto* headerBrowse = new QPushButton("Browse include directory…"); headerRow->addWidget(headerBrowse); form->addRow("Header include directories (one per line)", headerRow);
        connect(headerBrowse, &QPushButton::clicked, this, [this] {
            auto path = QFileDialog::getExistingDirectory(this, "Select include directory", browseStart(module_));
            if (!path.isEmpty()) headers_->appendPlainText(storedPath(path));
        });
        sources_ = new QPlainTextEdit; sources_->setMaximumHeight(120); sources_->setPlaceholderText("test_module/module1/src/module1.cpp");
        auto* sourceRow = new QHBoxLayout; sourceRow->addWidget(sources_); auto* sourceBrowse = new QPushButton("Browse .cpp files…"); sourceRow->addWidget(sourceBrowse); form->addRow("Selected actual C++ files (one per line)", sourceRow);
        connect(sourceBrowse, &QPushButton::clicked, this, [this] {
            for (const auto& path : QFileDialog::getOpenFileNames(this, "Select .cpp files", browseStart(sourceDirectory_), "C++ sources (*.cpp)"))
                sources_->appendPlainText(storedPath(path));
        });
        preview_ = new QPlainTextEdit; preview_->setReadOnly(true); layout->addWidget(new QLabel("Validated selection preview")); layout->addWidget(preview_);
        error_ = new QLabel; error_->setWordWrap(true); layout->addWidget(error_);
        auto* actions = new QHBoxLayout; layout->addLayout(actions);
        auto* validate = new QPushButton("Validate / Preview"); accept_ = new QPushButton("Add to build settings"); auto* cancel = new QPushButton("Cancel");
        actions->addWidget(validate); actions->addWidget(accept_); actions->addWidget(cancel); accept_->setEnabled(false);
        auto invalidate = [this] { accept_->setEnabled(false); preview_->clear(); error_->clear(); };
        connect(module_, &QLineEdit::textChanged, this, invalidate);
        connect(sourceDirectory_, &QLineEdit::textChanged, this, invalidate);
        connect(headers_, &QPlainTextEdit::textChanged, this, invalidate);
        connect(sources_, &QPlainTextEdit::textChanged, this, invalidate);
        connect(validate, &QPushButton::clicked, this, [this] { validateSelection(); });
        connect(accept_, &QPushButton::clicked, this, [this] { if (validateSelection()) accept(); });
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    bool validateSelection() {
        try {
            kpm::Json request{{"module_directory", module_->text().toStdString()},
                              {"source_directory", sourceDirectory_->text().toStdString()},
                              {"include_directories", lines(headers_)}, {"sources", lines(sources_)}};
            merged_ = kpm::custom_module_settings(root_, build_, request, element_);
            kpm::Json selection = {{"sources", kpm::Json::array()}, {"include_directories", kpm::Json::array()}};
            for (const auto* key : {"sources", "include_directories"})
                for (size_t i = build_.at(key).size(); i < merged_.at(key).size(); ++i) selection[key].push_back(merged_[key][i]);
            preview_->setPlainText(QString::fromStdString(selection.dump(2)));
            error_->clear(); accept_->setEnabled(true); return true;
        } catch (const std::exception& e) {
            error_->setText(QString::fromUtf8(e.what())); preview_->clear(); accept_->setEnabled(false); return false;
        }
    }
    const kpm::Json& settings() const { return merged_; }
    void setSelection(const kpm::Json& request) {
        module_->setText(QString::fromStdString(request.at("module_directory")));
        sourceDirectory_->setText(QString::fromStdString(request.at("source_directory")));
        auto fill = [](QPlainTextEdit* edit, const kpm::Json& values) {
            QStringList text; for (const auto& v : values) text << QString::fromStdString(v.get<std::string>());
            edit->setPlainText(text.join('\n'));
        };
        fill(headers_, request.at("include_directories")); fill(sources_, request.at("sources"));
    }
};

#include "piano_assist/main_window.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "piano_assist/floating_overlay_window.hpp"
#include "piano_assist/song_parser.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace piano_assist {
namespace {

constexpr std::string_view kDefaultTag = "Virtual Piano";
constexpr std::size_t kOverlayChunkSizeNoBreaks = 10;
constexpr std::size_t kOverlaySmartChunkMin = 10;
constexpr std::size_t kOverlaySmartChunkMax = 16;

int to_qt_int(const std::size_t value) {
    if (value > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

QString join_tags(const std::vector<std::string>& tags) {
    QStringList values;
    for (const std::string& tag : tags) {
        values.push_back(QString::fromStdString(tag));
    }
    return values.join(", ");
}

std::vector<std::string> parse_tags(const QString& text) {
    std::vector<std::string> tags;
    const QStringList values = text.split(',', Qt::SkipEmptyParts);
    tags.reserve(static_cast<std::size_t>(values.size()));
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            tags.push_back(trimmed.toStdString());
        }
    }
    return tags;
}

bool contains_tag(const std::vector<std::string>& tags, const std::string& target) {
    return std::find(tags.begin(), tags.end(), target) != tags.end();
}

void append_tag_to_text(QLineEdit* tags_edit, const QString& tag) {
    if (tags_edit == nullptr) {
        return;
    }

    const QString cleaned = tag.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    std::vector<std::string> tags = parse_tags(tags_edit->text());
    const std::string value = cleaned.toStdString();
    if (std::find(tags.begin(), tags.end(), value) == tags.end()) {
        tags.push_back(value);
        tags_edit->setText(join_tags(tags));
    }
}

QString mode_token_for_song(const Song& song) {
    if (song.open_brace == '(' && song.close_brace == ')') {
        return "()";
    }
    return "[]";
}

std::pair<char, char> grouping_from_token(const QString& token) {
    if (token == "()") {
        return {'(', ')'};
    }
    return {'[', ']'};
}

char sustain_from_token(const QString& token) {
    return token == "|" ? '|' : '-';
}

int chunking_mode_to_combo_index(const OverlayChunkingMode mode) {
    return mode == OverlayChunkingMode::Smart ? 1 : 0;
}

OverlayChunkingMode chunking_mode_from_combo_index(const int index) {
    return index == 1 ? OverlayChunkingMode::Smart : OverlayChunkingMode::AutoDetect;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      repository_("sheets"),
      tag_store_("sheets/song_tags.PADISCRIM"),
      settings_store_("settings.PACFG"),
      settings_(settings_store_.load()),
      keyboard_(settings_.strict_mode) {
    repository_.ensure_storage();
    input_poll_timer_.setInterval(settings_.input_poll_interval_ms);

    build_ui();
    floating_overlay_ = std::make_unique<FloatingOverlayWindow>();
    floating_overlay_->setAttribute(Qt::WA_QuitOnClose, false);
    floating_overlay_->show();

    refresh_song_list();

    strict_mode_checkbox_->setChecked(settings_.strict_mode);
    handle_overlay_toggle(overlay_checkbox_ != nullptr && overlay_checkbox_->isChecked());

    connect(&input_poll_timer_, &QTimer::timeout, this, &MainWindow::poll_input);
    input_poll_timer_.start();
}

MainWindow::~MainWindow() {
    if (floating_overlay_ != nullptr) {
        floating_overlay_->close();
    }
}

void MainWindow::build_ui() {
    setWindowTitle("SheetMaster");
    resize(1040, 760);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(16, 16, 16, 16);
    root_layout->setSpacing(12);

    auto* title = new QLabel("SONG LIST", central);
    QFont title_font = title->font();
    title_font.setPointSize(18);
    title_font.setBold(true);
    title->setFont(title_font);
    root_layout->addWidget(title);

    auto* filter_row = new QHBoxLayout();
    filter_row->setSpacing(8);

    auto* search_label = new QLabel("Search:", central);
    search_edit_ = new QLineEdit(central);
    search_edit_->setPlaceholderText("Search songs...");

    auto* tag_label = new QLabel("Tag:", central);
    tag_filter_ = new QComboBox(central);
    tag_filter_->setMinimumWidth(220);

    filter_row->addWidget(search_label);
    filter_row->addWidget(search_edit_, 1);
    filter_row->addWidget(tag_label);
    filter_row->addWidget(tag_filter_);
    root_layout->addLayout(filter_row);

    auto* content_row = new QHBoxLayout();
    content_row->setSpacing(12);

    song_table_ = new QTableWidget(central);
    song_table_->setColumnCount(2);
    song_table_->setHorizontalHeaderLabels({"Song", "Tags"});
    song_table_->horizontalHeader()->setStretchLastSection(true);
    song_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    song_table_->verticalHeader()->setVisible(false);
    song_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    song_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    song_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    song_table_->setAlternatingRowColors(true);
    song_table_->setMinimumHeight(280);

    auto* action_column = new QVBoxLayout();
    action_column->setSpacing(8);

    import_button_ = new QPushButton("Import Songs", central);
    manage_button_ = new QPushButton("Manage Songs", central);
    settings_button_ = new QPushButton("Settings", central);
    action_column->addWidget(import_button_);
    action_column->addWidget(manage_button_);
    action_column->addWidget(settings_button_);
    action_column->addStretch(1);

    content_row->addWidget(song_table_, 1);
    content_row->addLayout(action_column);
    root_layout->addLayout(content_row);

    auto* info_group = new QGroupBox("Playback", central);
    auto* info_layout = new QVBoxLayout(info_group);
    current_song_label_ = new QLabel("CURRENT SONG: None", info_group);
    duration_label_ = new QLabel("SONG DURATION: 0 / 0", info_group);
    strict_mode_checkbox_ = new QCheckBox("Strict Mode", info_group);
    overlay_checkbox_ = new QCheckBox("Show Floating Overlay", info_group);
    overlay_checkbox_->setChecked(true);
    info_layout->addWidget(current_song_label_);
    info_layout->addWidget(duration_label_);
    info_layout->addWidget(strict_mode_checkbox_);
    info_layout->addWidget(overlay_checkbox_);
    root_layout->addWidget(info_group);

    auto* key_list_label = new QLabel("Auto Scroller Keys", central);
    key_list_ = new QListWidget(central);
    key_list_->setMinimumHeight(220);
    root_layout->addWidget(key_list_label);
    root_layout->addWidget(key_list_, 1);

    setCentralWidget(central);

    connect(search_edit_, &QLineEdit::textChanged, this, &MainWindow::refresh_song_list);
    connect(tag_filter_, &QComboBox::currentTextChanged, this, &MainWindow::refresh_song_list);
    connect(song_table_, &QTableWidget::cellDoubleClicked, this, &MainWindow::handle_song_double_click);
    connect(import_button_, &QPushButton::clicked, this, &MainWindow::handle_import_songs);
    connect(manage_button_, &QPushButton::clicked, this, &MainWindow::handle_manage_songs);
    connect(settings_button_, &QPushButton::clicked, this, &MainWindow::handle_settings);
    connect(strict_mode_checkbox_, &QCheckBox::toggled, this, &MainWindow::handle_strict_mode_toggle);
    connect(overlay_checkbox_, &QCheckBox::toggled, this, &MainWindow::handle_overlay_toggle);
}

void MainWindow::repopulate_tag_filter() {
    const QString previous = tag_filter_->currentText();
    const QSignalBlocker blocker(tag_filter_);

    std::vector<std::string> tags = tag_store_.list_all_tags();
    std::sort(tags.begin(), tags.end());

    tag_filter_->clear();
    tag_filter_->addItem("All Tags");
    for (const std::string& tag : tags) {
        tag_filter_->addItem(QString::fromStdString(tag));
    }

    int index = tag_filter_->findText(previous);
    if (index < 0) {
        index = 0;
    }
    tag_filter_->setCurrentIndex(index);
}

void MainWindow::refresh_song_list() {
    const std::string search = search_edit_->text().trimmed().toStdString();

    const std::vector<Song> songs = repository_.list_songs(search);
    tag_store_.migrate_song_name_keys_to_ids(songs);
    repopulate_tag_filter();

    const std::string selected_tag = [&]() {
        const QString selected = tag_filter_->currentText().trimmed();
        if (selected.isEmpty() || selected == "All Tags") {
            return std::string{};
        }
        return selected.toStdString();
    }();

    visible_songs_.clear();
    song_table_->clearContents();
    song_table_->setRowCount(0);

    for (const Song& song : songs) {
        const std::vector<std::string> tags = tag_store_.tags_for_song(song.id);
        if (!selected_tag.empty() && !contains_tag(tags, selected_tag)) {
            continue;
        }

        const int row = song_table_->rowCount();
        song_table_->insertRow(row);
        song_table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(song.name)));
        song_table_->setItem(row, 1, new QTableWidgetItem(join_tags(tags)));
        visible_songs_.push_back(song);
    }

    if (current_song_.has_value()) {
        bool found = false;
        for (int row = 0; row < song_table_->rowCount(); ++row) {
            if (static_cast<std::size_t>(row) < visible_songs_.size() &&
                visible_songs_[static_cast<std::size_t>(row)].id == current_song_->id) {
                song_table_->selectRow(row);
                found = true;
                break;
            }
        }
        if (!found) {
            current_song_.reset();
            current_sheet_.clear();
            current_index_ = 0;
            waiting_for_release_ = false;
            paused_ = false;
            pause_key_latched_ = false;
            key_list_->clear();
        }
    }

    update_playback_labels();
}

void MainWindow::handle_song_double_click(const int row, const int /*column*/) {
    if (row < 0 || row >= static_cast<int>(visible_songs_.size())) {
        return;
    }

    select_song(visible_songs_[static_cast<std::size_t>(row)]);
}

void MainWindow::select_song(const Song& song) {
    current_song_ = song;
    current_sheet_ = repository_.load_sheet(song);
    rebuild_overlay_lines(song);
    current_index_ = 0;
    waiting_for_release_ = false;
    paused_ = false;
    pause_key_latched_ = false;

    key_list_->clear();
    for (std::size_t index = 0; index < current_sheet_.size(); ++index) {
        const QString line = QString("%1. %2")
                                 .arg(to_qt_int(index + 1))
                                 .arg(QString::fromStdString(current_sheet_[index].keys));
        key_list_->addItem(line);
    }

    if (!current_sheet_.empty()) {
        key_list_->setCurrentRow(0);
    }

    update_playback_labels();
}

void MainWindow::rebuild_overlay_lines(const Song& song) {
    overlay_lines_.clear();
    overlay_line_starts_.clear();

    const auto build_fixed_chunks = [this](const std::size_t chunk_size) {
        overlay_lines_.clear();
        overlay_line_starts_.clear();

        std::size_t start = 0;
        while (start < current_sheet_.size()) {
            const std::size_t end = std::min(start + chunk_size, current_sheet_.size());
            overlay_line_starts_.push_back(start);
            overlay_lines_.emplace_back();
            std::vector<std::string>& keys = overlay_lines_.back();
            keys.reserve(end - start);
            for (std::size_t index = start; index < end; ++index) {
                keys.push_back(current_sheet_[index].keys);
            }
            start = end;
        }
    };

    const auto build_smart_chunks = [this, &song, &build_fixed_chunks]() {
        overlay_lines_.clear();
        overlay_line_starts_.clear();

        if (current_sheet_.empty()) {
            return;
        }

        auto has_sustain = [&song](const std::string& keys) {
            return keys.find(song.sustain_indicator) != std::string::npos ||
                   keys.find('-') != std::string::npos ||
                   keys.find('|') != std::string::npos;
        };

        std::vector<std::string> current_line;
        current_line.reserve(kOverlaySmartChunkMax);
        std::size_t running_start = 0;

        const auto flush_line = [&]() {
            if (current_line.empty()) {
                return;
            }
            overlay_line_starts_.push_back(running_start);
            overlay_lines_.push_back(current_line);
            running_start += current_line.size();
            current_line.clear();
        };

        for (const NoteGroup& group : current_sheet_) {
            current_line.push_back(group.keys);
            const bool sustain = has_sustain(group.keys);

            if (current_line.size() >= kOverlaySmartChunkMax) {
                flush_line();
            } else if (current_line.size() >= kOverlaySmartChunkMin && !sustain) {
                flush_line();
            }
        }

        flush_line();
        if (overlay_lines_.empty()) {
            build_fixed_chunks(kOverlayChunkSizeNoBreaks);
        }
    };

    if (settings_.overlay_chunking_mode == OverlayChunkingMode::Smart) {
        build_smart_chunks();
        return;
    }

    const std::string raw_text = repository_.load_raw_sheet_text(song);
    const bool has_explicit_line_breaks = raw_text.find('\n') != std::string::npos ||
                                          raw_text.find('\r') != std::string::npos;
    std::istringstream input(raw_text);

    std::size_t running_index = 0;
    std::string line;
    while (std::getline(input, line)) {
        const std::vector<NoteGroup> parsed =
            parse_sheet(line + " ", song.open_brace, song.close_brace, song.sustain_indicator);
        if (parsed.empty()) {
            continue;
        }

        overlay_line_starts_.push_back(running_index);
        overlay_lines_.emplace_back();
        std::vector<std::string>& keys = overlay_lines_.back();
        keys.reserve(parsed.size());
        for (const NoteGroup& group : parsed) {
            keys.push_back(group.keys);
        }
        running_index += keys.size();
    }

    const bool mismatch = running_index != current_sheet_.size();
    if (overlay_lines_.empty() || mismatch || !has_explicit_line_breaks) {
        build_fixed_chunks(kOverlayChunkSizeNoBreaks);
    }
}

void MainWindow::update_playback_labels() {
    if (!current_song_.has_value()) {
        current_song_label_->setText("CURRENT SONG: None");
        duration_label_->setText("SONG DURATION: 0 / 0");
        update_floating_overlay();
        return;
    }

    const QString pause_suffix = paused_ ? " [PAUSED]" : "";
    current_song_label_->setText(
        QString("CURRENT SONG: %1%2")
            .arg(QString::fromStdString(current_song_->name))
            .arg(pause_suffix)
    );

    const std::size_t total = current_sheet_.size();
    const std::size_t display_current = total == 0 ? 0 : std::min(current_index_ + 1, total);
    duration_label_->setText(
        QString("SONG DURATION: %1 / %2%3")
            .arg(to_qt_int(display_current))
            .arg(to_qt_int(total))
            .arg(paused_ ? " (Enter to Resume)" : " (Enter to Pause)")
    );

    if (total > 0) {
        const int row = to_qt_int(std::min(current_index_, total - 1));
        key_list_->setCurrentRow(row);
        key_list_->scrollToItem(key_list_->item(row), QAbstractItemView::PositionAtCenter);
    }

    update_floating_overlay();
}

void MainWindow::update_floating_overlay() {
    if (floating_overlay_ == nullptr) {
        return;
    }

    const std::string song_name = current_song_.has_value() ? current_song_->name : std::string{};
    const std::size_t progress_current = current_sheet_.empty()
                                             ? 0
                                             : std::min(current_index_ + 1, current_sheet_.size());
    const std::size_t progress_total = current_sheet_.size();

    if (current_sheet_.empty() || overlay_lines_.empty()) {
        floating_overlay_->set_song_progress(
            {},
            std::nullopt,
            {},
            false,
            paused_,
            song_name,
            progress_current,
            progress_total
        );
        return;
    }

    if (current_index_ >= current_sheet_.size()) {
        floating_overlay_->set_song_progress(
            {},
            std::nullopt,
            {},
            true,
            paused_,
            song_name,
            progress_total,
            progress_total
        );
        return;
    }

    std::size_t line_index = 0;
    for (std::size_t i = 0; i < overlay_line_starts_.size(); ++i) {
        const std::size_t start = overlay_line_starts_[i];
        const std::size_t length = overlay_lines_[i].size();
        const std::size_t end = start + length;
        if (current_index_ >= start && current_index_ < end) {
            line_index = i;
            break;
        }
    }

    const std::size_t line_start = overlay_line_starts_[line_index];
    const std::size_t key_in_line = current_index_ - line_start;
    const std::vector<std::string>& current_line = overlay_lines_[line_index];
    static const std::vector<std::string> kEmptyLine;
    const std::vector<std::string>& next_line =
        (line_index + 1 < overlay_lines_.size()) ? overlay_lines_[line_index + 1] : kEmptyLine;

    floating_overlay_->set_song_progress(
        current_line,
        key_in_line,
        next_line,
        false,
        paused_,
        song_name,
        progress_current,
        progress_total
    );
}

std::optional<Song> MainWindow::selected_song_from_table() const {
    const int row = song_table_->currentRow();
    if (row < 0 || row >= static_cast<int>(visible_songs_.size())) {
        return std::nullopt;
    }
    return visible_songs_[static_cast<std::size_t>(row)];
}

void MainWindow::handle_import_songs() {
    QDialog dialog(this);
    dialog.setWindowTitle("Import Songs");
    dialog.resize(640, 420);

    auto* root = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* name_edit = new QLineEdit(&dialog);
    auto* tags_edit = new QLineEdit(&dialog);
    auto* quick_tag_combo = new QComboBox(&dialog);
    auto* add_tag_button = new QPushButton("Add", &dialog);
    auto* grouping_combo = new QComboBox(&dialog);
    auto* sustain_combo = new QComboBox(&dialog);
    auto* notes_edit = new QPlainTextEdit(&dialog);
    notes_edit->setPlaceholderText("Paste Virtual Piano notes here...");

    quick_tag_combo->addItem("Select existing tag...");
    for (const std::string& tag : tag_store_.list_all_tags()) {
        quick_tag_combo->addItem(QString::fromStdString(tag));
    }

    grouping_combo->addItem("Square Brackets [ ]", "[]");
    grouping_combo->addItem("Parentheses ( )", "()");
    sustain_combo->addItem("-");
    sustain_combo->addItem("|");

    tags_edit->setText(QString::fromStdString(std::string(kDefaultTag)));
    auto* quick_tag_row = new QWidget(&dialog);
    auto* quick_tag_layout = new QHBoxLayout(quick_tag_row);
    quick_tag_layout->setContentsMargins(0, 0, 0, 0);
    quick_tag_layout->setSpacing(6);
    quick_tag_layout->addWidget(quick_tag_combo, 1);
    quick_tag_layout->addWidget(add_tag_button);

    form->addRow("Song Name:", name_edit);
    form->addRow("Tags:", tags_edit);
    form->addRow("Quick Tag:", quick_tag_row);
    form->addRow("Grouping Mode:", grouping_combo);
    form->addRow("Sustain/Delay Indicator:", sustain_combo);
    root->addLayout(form);
    root->addWidget(notes_edit, 1);

    const auto handle_quick_tag = [tags_edit, quick_tag_combo]() {
        if (quick_tag_combo->currentIndex() <= 0) {
            return;
        }
        append_tag_to_text(tags_edit, quick_tag_combo->currentText());
    };
    connect(add_tag_button, &QPushButton::clicked, &dialog, handle_quick_tag);
    connect(quick_tag_combo, &QComboBox::activated, &dialog, [handle_quick_tag](int index) {
        if (index > 0) {
            handle_quick_tag();
        }
    });

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(button_box);
    connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString notes = notes_edit->toPlainText().trimmed();
    if (notes.isEmpty()) {
        QMessageBox::warning(this, "Import Songs", "Please paste notes before importing.");
        return;
    }

    std::string song_name = name_edit->text().trimmed().toStdString();
    if (song_name.empty()) {
        song_name = "Untitled_Song";
    }

    try {
        const auto [open_brace, close_brace] =
            grouping_from_token(grouping_combo->currentData().toString());
        const char sustain_indicator = sustain_from_token(sustain_combo->currentText());
        const std::string saved_song_id = repository_.import_song(
            song_name,
            notes.toStdString(),
            open_brace,
            close_brace,
            sustain_indicator
        );
        const std::vector<std::string> tags = parse_tags(tags_edit->text());
        tag_store_.set_tags_for_song(saved_song_id, tags);
        refresh_song_list();
    } catch (const std::exception& exception) {
        QMessageBox::critical(this, "Import Songs", QString("Failed to import song:\n%1").arg(exception.what()));
    }
}

void MainWindow::handle_manage_songs() {
    const std::optional<Song> selected_song = selected_song_from_table();
    if (!selected_song.has_value()) {
        QMessageBox::information(this, "Manage Songs", "Select a song first.");
        return;
    }

    Song song = *selected_song;
    const QString original_name = QString::fromStdString(song.name);
    const QString original_notes = QString::fromStdString(repository_.load_raw_sheet_text(song));
    const std::vector<std::string> existing_tags = tag_store_.tags_for_song(song.id);

    QDialog dialog(this);
    dialog.setWindowTitle("Manage Songs");
    dialog.resize(680, 460);

    auto* root = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* name_edit = new QLineEdit(original_name, &dialog);
    auto* tags_edit = new QLineEdit(join_tags(existing_tags), &dialog);
    auto* quick_tag_combo = new QComboBox(&dialog);
    auto* add_tag_button = new QPushButton("Add", &dialog);
    auto* grouping_combo = new QComboBox(&dialog);
    auto* sustain_combo = new QComboBox(&dialog);
    auto* notes_edit = new QPlainTextEdit(original_notes, &dialog);

    quick_tag_combo->addItem("Select existing tag...");
    for (const std::string& tag : tag_store_.list_all_tags()) {
        quick_tag_combo->addItem(QString::fromStdString(tag));
    }

    auto* quick_tag_row = new QWidget(&dialog);
    auto* quick_tag_layout = new QHBoxLayout(quick_tag_row);
    quick_tag_layout->setContentsMargins(0, 0, 0, 0);
    quick_tag_layout->setSpacing(6);
    quick_tag_layout->addWidget(quick_tag_combo, 1);
    quick_tag_layout->addWidget(add_tag_button);
    grouping_combo->addItem("Square Brackets [ ]", "[]");
    grouping_combo->addItem("Parentheses ( )", "()");
    {
        const int index = grouping_combo->findData(mode_token_for_song(song));
        grouping_combo->setCurrentIndex(index < 0 ? 0 : index);
    }

    sustain_combo->addItem("-");
    sustain_combo->addItem("|");
    sustain_combo->setCurrentText(QString(QChar(song.sustain_indicator)));

    form->addRow("Song Name:", name_edit);
    form->addRow("Tags:", tags_edit);
    form->addRow("Quick Tag:", quick_tag_row);
    form->addRow("Grouping Mode:", grouping_combo);
    form->addRow("Sustain/Delay Indicator:", sustain_combo);
    root->addLayout(form);
    root->addWidget(notes_edit, 1);

    const auto handle_quick_tag = [tags_edit, quick_tag_combo]() {
        if (quick_tag_combo->currentIndex() <= 0) {
            return;
        }
        append_tag_to_text(tags_edit, quick_tag_combo->currentText());
    };
    connect(add_tag_button, &QPushButton::clicked, &dialog, handle_quick_tag);
    connect(quick_tag_combo, &QComboBox::activated, &dialog, [handle_quick_tag](int index) {
        if (index > 0) {
            handle_quick_tag();
        }
    });

    auto* button_row = new QHBoxLayout();
    auto* save_button = new QPushButton("Save", &dialog);
    auto* delete_button = new QPushButton("Delete", &dialog);
    auto* cancel_button = new QPushButton("Cancel", &dialog);
    delete_button->setStyleSheet("QPushButton { color: #B00020; font-weight: 600; }");

    button_row->addWidget(save_button);
    button_row->addWidget(delete_button);
    button_row->addStretch(1);
    button_row->addWidget(cancel_button);
    root->addLayout(button_row);

    enum class Action { None, Save, Delete };
    Action action = Action::None;

    connect(save_button, &QPushButton::clicked, &dialog, [&]() {
        action = Action::Save;
        dialog.accept();
    });
    connect(delete_button, &QPushButton::clicked, &dialog, [&]() {
        action = Action::Delete;
        dialog.accept();
    });
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || action == Action::None) {
        return;
    }

    if (action == Action::Delete) {
        const auto choice = QMessageBox::question(
            this,
            "Delete Song",
            QString("Delete '%1'?").arg(original_name),
            QMessageBox::Yes | QMessageBox::No
        );
        if (choice != QMessageBox::Yes) {
            return;
        }

        repository_.delete_song(song);
        tag_store_.remove_song(song.id);
        if (current_song_.has_value() && current_song_->id == song.id) {
            current_song_.reset();
            current_sheet_.clear();
            current_index_ = 0;
            waiting_for_release_ = false;
            paused_ = false;
            pause_key_latched_ = false;
            key_list_->clear();
        }
        refresh_song_list();
        return;
    }

    const QString notes = notes_edit->toPlainText().trimmed();
    if (notes.isEmpty()) {
        QMessageBox::warning(this, "Manage Songs", "Song notes cannot be empty.");
        return;
    }

    std::string requested_name = name_edit->text().trimmed().toStdString();
    if (requested_name.empty()) {
        requested_name = song.name;
    }

    try {
        const auto [open_brace, close_brace] =
            grouping_from_token(grouping_combo->currentData().toString());
        song.open_brace = open_brace;
        song.close_brace = close_brace;
        song.sustain_indicator = sustain_from_token(sustain_combo->currentText());

        const std::string final_name = repository_.rename_song(song, requested_name);
        song.name = final_name;

        repository_.update_song_contents(song, notes.toStdString());
        const std::vector<std::string> tags = parse_tags(tags_edit->text());
        tag_store_.set_tags_for_song(song.id, tags);

        if (current_song_.has_value() && current_song_->id == song.id) {
            select_song(song);
        }

        refresh_song_list();
    } catch (const std::exception& exception) {
        QMessageBox::critical(this, "Manage Songs", QString("Failed to save changes:\n%1").arg(exception.what()));
    }
}

void MainWindow::handle_settings() {
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");

    auto* root = new QVBoxLayout(&dialog);
    auto* strict_checkbox = new QCheckBox("Strict Mode", &dialog);
    auto* poll_spin = new QSpinBox(&dialog);
    auto* chunking_combo = new QComboBox(&dialog);
    poll_spin->setRange(1, 100);
    poll_spin->setSuffix(" ms");
    poll_spin->setValue(settings_.input_poll_interval_ms);
    chunking_combo->addItem("Auto Detect");
    chunking_combo->addItem("Smart");
    chunking_combo->setCurrentIndex(chunking_mode_to_combo_index(settings_.overlay_chunking_mode));
    strict_checkbox->setChecked(settings_.strict_mode);

    auto* form = new QFormLayout();
    form->addRow("Playback Poll Interval:", poll_spin);
    form->addRow("Overlay Chunking:", chunking_combo);
    root->addWidget(strict_checkbox);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings_.strict_mode = strict_checkbox->isChecked();
    settings_.input_poll_interval_ms = poll_spin->value();
    settings_.overlay_chunking_mode = chunking_mode_from_combo_index(chunking_combo->currentIndex());
    strict_mode_checkbox_->setChecked(settings_.strict_mode);
    input_poll_timer_.setInterval(settings_.input_poll_interval_ms);
    settings_store_.save(settings_);

    if (current_song_.has_value()) {
        rebuild_overlay_lines(*current_song_);
    }
    update_playback_labels();
}

void MainWindow::handle_strict_mode_toggle(const bool checked) {
    settings_.strict_mode = checked;
    keyboard_.set_strict_mode(checked);
    settings_store_.save(settings_);
}

void MainWindow::handle_overlay_toggle(const bool checked) {
    if (floating_overlay_ == nullptr) {
        return;
    }

    if (checked) {
        floating_overlay_->show();
        floating_overlay_->raise();
        update_floating_overlay();
    } else {
        floating_overlay_->hide();
    }
}

void MainWindow::poll_input() {
    const bool enter_down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (enter_down && !pause_key_latched_) {
        paused_ = !paused_;
        pause_key_latched_ = true;
        waiting_for_release_ = true;
        update_playback_labels();
    } else if (!enter_down) {
        pause_key_latched_ = false;
    }

    if (!current_song_.has_value()) {
        return;
    }

    if (current_sheet_.empty() || current_index_ >= current_sheet_.size()) {
        return;
    }

    if (paused_) {
        return;
    }

    if (waiting_for_release_) {
        if (!KeyboardInput::is_any_monitored_key_down()) {
            waiting_for_release_ = false;
        }
        return;
    }

    const bool should_advance = settings_.strict_mode
                                    ? keyboard_.check_chord(current_sheet_[current_index_].keys)
                                    : KeyboardInput::is_any_monitored_key_down();

    if (should_advance) {
        ++current_index_;
        waiting_for_release_ = true;
        update_playback_labels();
    }
}

} // namespace piano_assist

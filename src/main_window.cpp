#include "piano_assist/main_window.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "piano_assist/app_info.hpp"
#include "piano_assist/floating_overlay_window.hpp"
#include "piano_assist/song_parser.hpp"
#include "piano_assist/update_utils.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSettings>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextOption>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace piano_assist {
namespace {

constexpr std::string_view kDefaultTag = "Virtual Piano";
constexpr std::size_t kOverlayChunkSizeNoBreaks = 10;
constexpr std::size_t kOverlayMaxAutoDetectedLineSize = kOverlayChunkSizeNoBreaks * 2;
constexpr std::size_t kOverlaySmartChunkMin = 10;
constexpr std::size_t kOverlaySmartChunkMax = 16;
constexpr int kOverlayHeightPx = 144;
constexpr int kOverlayBottomMarginPx = 72;
constexpr int kMainWindowGapAboveOverlayPx = 16;
constexpr int kMainWindowTopMarginPx = 24;
constexpr int kMainWindowMinStartupHeightPx = 620;
constexpr const char* kPollIntervalTooltip =
    "How often SheetMaster checks your keyboard input.\n\n"
    "Lower values:\n"
    "- Better timing for fast songs\n"
    "- More likely to register duplicate hits if input is messy\n\n"
    "Higher values:\n"
    "- More forgiving input detection\n"
    "- Slightly less timing precision on dense passages";
constexpr const char* kOverlayChunkingTooltip =
    "How notes are split into overlay lines.\n\n"
    "Auto Detect:\n"
    "- Uses saved/imported line breaks when available\n"
    "- Falls back to fixed chunks if needed\n\n"
    "Smart:\n"
    "- Tries to group continuous note runs\n"
    "- Attempts to avoid awkward breaks near sustain/delay markers";
constexpr const char* kStrictModeTooltip =
    "Strict Mode requires the exact expected key/chord before advancing.\n\n"
    "On:\n"
    "- Only correct notes advance\n"
    "- Better for accurate practice\n\n"
    "Off:\n"
    "- Any monitored key advances\n"
    "- More forgiving while learning";
constexpr int kUpdateStartupDelayMs = 2500;
constexpr qint64 kUpdateCheckIntervalSeconds = 24 * 60 * 60;
constexpr const char* kUpdateLastCheckUtcSettingKey = "updates/last_check_utc";

bool should_check_for_updates_now() {
    QSettings settings;
    const QDateTime last_checked_utc =
        settings.value(QString::fromLatin1(kUpdateLastCheckUtcSettingKey)).toDateTime();
    if (!last_checked_utc.isValid()) {
        return true;
    }
    return last_checked_utc.secsTo(QDateTime::currentDateTimeUtc()) >=
           kUpdateCheckIntervalSeconds;
}

void record_update_check_now() {
    QSettings settings;
    settings.setValue(QString::fromLatin1(kUpdateLastCheckUtcSettingKey),
                      QDateTime::currentDateTimeUtc());
}

std::vector<ReleaseAssetInfo> parse_release_assets(const QJsonArray& assets) {
    std::vector<ReleaseAssetInfo> parsed;
    parsed.reserve(static_cast<std::size_t>(assets.size()));

    for (const QJsonValue& value : assets) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject asset_object = value.toObject();
        ReleaseAssetInfo asset;
        asset.name = asset_object.value("name").toString().trimmed().toStdString();
        asset.download_url =
            asset_object.value("browser_download_url").toString().trimmed().toStdString();
        asset.digest = asset_object.value("digest").toString().trimmed().toStdString();
        parsed.push_back(std::move(asset));
    }

    return parsed;
}

class InstantToolTipFilter final : public QObject {
  public:
    explicit InstantToolTipFilter(QObject* parent = nullptr) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event != nullptr && event->type() == QEvent::Enter) {
            QWidget* widget = qobject_cast<QWidget*>(watched);
            if (widget != nullptr && !widget->toolTip().trimmed().isEmpty()) {
                QToolTip::showText(widget->mapToGlobal(widget->rect().center()), widget->toolTip(),
                                   widget);
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

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

int chunking_mode_to_combo_index(const OverlayChunkingMode mode) {
    return mode == OverlayChunkingMode::Smart ? 1 : 0;
}

OverlayChunkingMode chunking_mode_from_combo_index(const int index) {
    return index == 1 ? OverlayChunkingMode::Smart : OverlayChunkingMode::AutoDetect;
}

QString bpm_display_value(const Song& song) {
    if (!song.bpm.has_value()) {
        return "-";
    }
    return QString::number(*song.bpm);
}

QString migration_summary_text(const MigrationSummary& summary) {
    QString text;
    text += QString("Files scanned: %1\n").arg(to_qt_int(summary.scanned_files));
    text += QString("Files to rewrite: %1\n").arg(to_qt_int(summary.files_to_rewrite));
    text += QString("Legacy .txt files: %1\n").arg(to_qt_int(summary.legacy_extension_files));
    text += QString("Legacy format files: %1\n").arg(to_qt_int(summary.legacy_format_files));
    text += QString("Missing id fixes: %1\n").arg(to_qt_int(summary.missing_id));
    text += QString("Missing name fixes: %1\n").arg(to_qt_int(summary.missing_name));
    text += QString("Grouping fixes: %1\n").arg(to_qt_int(summary.normalized_grouping));
    text +=
        QString("Sustain metadata fixes: %1\n").arg(to_qt_int(summary.normalized_sustain_metadata));
    text += QString("Body | -> - normalization: %1")
                .arg(to_qt_int(summary.normalized_body_sustain_tokens));
    if (!summary.sample_changed_files.empty()) {
        text += "\n\nSample files:";
        for (const std::string& sample : summary.sample_changed_files) {
            text += QString("\n- %1").arg(QString::fromStdString(sample));
        }
    }
    return text;
}

QString tag_repair_summary_text(const TagRepairSummary& summary) {
    QString text;
    text += QString("Songs scanned: %1\n").arg(to_qt_int(summary.songs_scanned));
    text += QString("Songs missing tags: %1\n").arg(to_qt_int(summary.songs_missing_tags));
    text += QString("Songs with empty tags: %1\n").arg(to_qt_int(summary.songs_with_empty_tags));
    text += QString("Legacy name-key tag migrations: %1\n")
                .arg(to_qt_int(summary.legacy_name_keys_migrated));
    text += QString("Duplicate name-key tag entries removed: %1\n")
                .arg(to_qt_int(summary.duplicate_name_keys_removed));
    text += QString("Orphan tag entries removed: %1\n")
                .arg(to_qt_int(summary.orphan_tag_entries_removed));
    if (!summary.sample_affected_songs.empty()) {
        text += "\n\nSample affected songs/keys:";
        for (const std::string& sample : summary.sample_affected_songs) {
            text += QString("\n- %1").arg(QString::fromStdString(sample));
        }
    }
    return text;
}

QString sanitize_summary_text(const MigrationSummary& file_summary,
                              const TagRepairSummary& tag_summary) {
    return QString("Song Data\n--------\n%1\n\nTag Data\n--------\n%2")
        .arg(migration_summary_text(file_summary), tag_repair_summary_text(tag_summary));
}

QString build_export_share_string(const std::vector<Song>& songs, const SongRepository& repository,
                                  const TagStore& tag_store) {
    QJsonArray exported_songs;
    for (const Song& song : songs) {
        QJsonObject item;
        item["name"] = QString::fromStdString(song.name);
        item["grouping"] = mode_token_for_song(song);
        item["bpm"] = song.bpm.has_value() ? QJsonValue(*song.bpm) : QJsonValue(QJsonValue::Null);
        item["notes"] = QString::fromStdString(repository.load_raw_sheet_text(song));

        QJsonArray tags;
        for (const std::string& tag : tag_store.tags_for_song(song.id)) {
            tags.push_back(QString::fromStdString(tag));
        }
        item["tags"] = tags;
        exported_songs.push_back(item);
    }

    QJsonObject root;
    root["version"] = 1;
    root["songs"] = exported_songs;

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    return QString("SMX1:%1").arg(QString::fromLatin1(json.toBase64(QByteArray::Base64Encoding)));
}

bool parse_export_share_string(const QString& share_string, QJsonArray* songs_out,
                               QString* error_out) {
    if (songs_out == nullptr) {
        if (error_out != nullptr) {
            *error_out = "Internal error: invalid output container.";
        }
        return false;
    }

    const QString trimmed = share_string.trimmed();
    if (!trimmed.startsWith("SMX1:")) {
        if (error_out != nullptr) {
            *error_out = "Invalid share string prefix.";
        }
        return false;
    }

    const QByteArray encoded = trimmed.mid(5).toLatin1();
    const QByteArray decoded = QByteArray::fromBase64(encoded, QByteArray::Base64Encoding);
    if (decoded.isEmpty()) {
        if (error_out != nullptr) {
            *error_out = "Failed to decode share payload.";
        }
        return false;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(decoded, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error_out != nullptr) {
            *error_out = "Share payload is not valid JSON.";
        }
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value("version").toInt(0) != 1 || !root.value("songs").isArray()) {
        if (error_out != nullptr) {
            *error_out = "Unsupported share payload version.";
        }
        return false;
    }

    *songs_out = root.value("songs").toArray();
    return true;
}

#if defined(_WIN32)
bool is_vk_down(const int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

bool is_pause_combo_down() { return is_vk_down(VK_RETURN) && is_vk_down(VK_SHIFT); }

bool is_left_key_down() { return is_vk_down(VK_LEFT); }

bool is_right_key_down() { return is_vk_down(VK_RIGHT); }

bool is_up_key_down() { return is_vk_down(VK_UP); }

bool is_down_key_down() { return is_vk_down(VK_DOWN); }

bool is_tab_key_down() { return is_vk_down(VK_TAB); }
#else
bool is_pause_combo_down() { return false; }

bool is_left_key_down() { return false; }

bool is_right_key_down() { return false; }

bool is_up_key_down() { return false; }

bool is_down_key_down() { return false; }

bool is_tab_key_down() { return false; }
#endif

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), repository_("sheets"), tag_store_("sheets/song_tags.PADISCRIM"),
      settings_store_("settings.PACFG"), settings_(settings_store_.load()),
      keyboard_(settings_.strict_mode) {
    repository_.ensure_storage();
    input_poll_timer_.setInterval(settings_.input_poll_interval_ms);
    update_network_ = new QNetworkAccessManager(this);

    build_ui();
    floating_overlay_ = std::make_unique<FloatingOverlayWindow>();
    floating_overlay_->setAttribute(Qt::WA_QuitOnClose, false);
    connect(floating_overlay_.get(), &FloatingOverlayWindow::sheet_view_toggled, this,
            [this](const bool visible) {
                if (settings_.show_practice_sheet == visible) {
                    return;
                }
                settings_.show_practice_sheet = visible;
                settings_store_.save(settings_);
                update_playback_labels();
            });
    if (!settings_.show_sheet_tab_button && settings_.show_practice_sheet) {
        settings_.show_practice_sheet = false;
        settings_store_.save(settings_);
    }
    floating_overlay_->set_sheet_tab_visible(settings_.show_sheet_tab_button);
    floating_overlay_->set_sheet_view_visible(settings_.show_practice_sheet);
    position_window_for_overlay();

    refresh_song_list();

    strict_mode_checkbox_->setChecked(settings_.strict_mode);
    handle_overlay_toggle(overlay_checkbox_ != nullptr && overlay_checkbox_->isChecked());
    update_practice_sheet_window();

    connect(&input_poll_timer_, &QTimer::timeout, this, &MainWindow::poll_input);
    input_poll_timer_.start();
    QTimer::singleShot(kUpdateStartupDelayMs, this,
                       &MainWindow::maybe_check_for_updates_on_startup);
}

MainWindow::~MainWindow() {
    cleanup_update_download(false);
    if (floating_overlay_ != nullptr) {
        floating_overlay_->close();
    }
}

void MainWindow::build_ui() {
    setWindowTitle("SheetMaster");
    resize(1100, 760);

    auto* central = new QWidget(this);
    auto* root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(16, 16, 16, 16);
    root_layout->setSpacing(12);

    auto* title = new QLabel("Sheet Library", central);
    QFont title_font = title->font();
    title_font.setPointSize(18);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setAlignment(Qt::AlignHCenter);
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
    song_table_->setMinimumHeight(300);

    auto* action_column = new QVBoxLayout();
    action_column->setSpacing(8);

    import_button_ = new QPushButton("Import Songs", central);
    manage_button_ = new QPushButton("Manage Songs", central);
    settings_button_ = new QPushButton("Settings", central);
    check_updates_button_ = new QPushButton("Check for Updates", central);
    action_column->addWidget(import_button_);
    action_column->addWidget(manage_button_);
    action_column->addWidget(settings_button_);
    action_column->addWidget(check_updates_button_);
    action_column->addStretch(1);

    content_row->addWidget(song_table_, 1);
    content_row->addLayout(action_column);
    root_layout->addLayout(content_row);

    auto* info_group = new QGroupBox("Playback", central);
    info_group->setMaximumWidth(980);
    auto* info_layout = new QVBoxLayout(info_group);
    current_song_label_ = new QLabel("CURRENT SONG: None", info_group);
    duration_label_ = new QLabel("SONG DURATION: 0 / 0", info_group);
    details_label_ = new QLabel("", info_group);
    details_label_->setWordWrap(true);
    details_label_->setStyleSheet("QLabel { color: #6B6B6B; }");
    strict_mode_checkbox_ = new QCheckBox("Strict Mode", info_group);
    strict_mode_checkbox_->setToolTip(kStrictModeTooltip);
    overlay_checkbox_ = new QCheckBox("Show Floating Overlay", info_group);
    overlay_checkbox_->setChecked(true);
    auto* toggles_row = new QHBoxLayout();
    toggles_row->addWidget(strict_mode_checkbox_);
    toggles_row->addWidget(overlay_checkbox_);
    toggles_row->addStretch(1);
    info_layout->addWidget(current_song_label_);
    info_layout->addWidget(duration_label_);
    info_layout->addWidget(details_label_);
    info_layout->addLayout(toggles_row);
    root_layout->addWidget(info_group, 0, Qt::AlignHCenter);

    setCentralWidget(central);

    connect(search_edit_, &QLineEdit::textChanged, this, &MainWindow::refresh_song_list);
    connect(tag_filter_, &QComboBox::currentTextChanged, this, &MainWindow::refresh_song_list);
    connect(song_table_, &QTableWidget::cellDoubleClicked, this,
            &MainWindow::handle_song_double_click);
    connect(import_button_, &QPushButton::clicked, this, &MainWindow::handle_import_songs);
    connect(manage_button_, &QPushButton::clicked, this, &MainWindow::handle_manage_songs);
    connect(settings_button_, &QPushButton::clicked, this, &MainWindow::handle_settings);
    connect(check_updates_button_, &QPushButton::clicked, this, &MainWindow::handle_check_updates);
    connect(strict_mode_checkbox_, &QCheckBox::toggled, this,
            &MainWindow::handle_strict_mode_toggle);
    connect(overlay_checkbox_, &QCheckBox::toggled, this, &MainWindow::handle_overlay_toggle);
}

void MainWindow::position_window_for_overlay() {
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return;
    }

    const QRect geometry = screen->availableGeometry();
    const int reserved_bottom_space =
        kOverlayHeightPx + kOverlayBottomMarginPx + kMainWindowGapAboveOverlayPx;
    const int max_main_bottom = geometry.bottom() - reserved_bottom_space;
    const int max_startup_height = max_main_bottom - (geometry.top() + kMainWindowTopMarginPx) + 1;
    if (max_startup_height >= kMainWindowMinStartupHeightPx && height() > max_startup_height) {
        resize(width(), max_startup_height);
    }

    const int x = geometry.left() + (geometry.width() - width()) / 2;
    int y = geometry.top() + kMainWindowTopMarginPx;
    if (y + height() - 1 > max_main_bottom) {
        y = std::max(geometry.top(), max_main_bottom - height() + 1);
    }

    move(x, y);
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
    tag_store_.ensure_default_tag_for_songs(songs, kDefaultTag);
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
            overlay_lines_.clear();
            overlay_line_starts_.clear();
            current_index_ = 0;
            waiting_for_release_ = false;
            paused_ = false;
            pause_combo_latched_ = false;
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
    pause_combo_latched_ = false;
    left_key_latched_ = false;
    right_key_latched_ = false;
    up_key_latched_ = false;
    down_key_latched_ = false;
    tab_key_latched_ = false;

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
                   keys.find('-') != std::string::npos || keys.find('|') != std::string::npos;
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
    std::istringstream input(raw_text);

    std::size_t running_index = 0;
    std::size_t non_empty_line_count = 0;
    std::size_t longest_detected_line = 0;
    std::string line;
    while (std::getline(input, line)) {
        const std::vector<NoteGroup> parsed =
            parse_sheet(line + " ", song.open_brace, song.close_brace, song.sustain_indicator);
        if (parsed.empty()) {
            continue;
        }

        ++non_empty_line_count;
        longest_detected_line = std::max(longest_detected_line, parsed.size());
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
    const bool has_usable_line_break_data =
        non_empty_line_count > 1 && longest_detected_line <= kOverlayMaxAutoDetectedLineSize;
    if (overlay_lines_.empty() || mismatch || !has_usable_line_break_data) {
        build_fixed_chunks(kOverlayChunkSizeNoBreaks);
    }
}

std::optional<std::size_t>
MainWindow::line_index_for_note_index(const std::size_t note_index) const {
    if (overlay_lines_.empty() || overlay_line_starts_.empty() || current_sheet_.empty()) {
        return std::nullopt;
    }

    if (note_index >= current_sheet_.size()) {
        return overlay_lines_.size() - 1;
    }

    for (std::size_t i = 0; i < overlay_line_starts_.size(); ++i) {
        const std::size_t start = overlay_line_starts_[i];
        const std::size_t end = start + overlay_lines_[i].size();
        if (note_index >= start && note_index < end) {
            return i;
        }
    }

    return overlay_lines_.size() - 1;
}

QString MainWindow::build_song_details_text() const {
    if (!current_song_.has_value() || !settings_.show_song_details) {
        return {};
    }

    QStringList details;
    if (settings_.show_tag_details) {
        const std::vector<std::string> tags = tag_store_.tags_for_song(current_song_->id);
        const QString rendered_tags = tags.empty() ? "No tags" : join_tags(tags);
        details.push_back(QString("Tags: %1").arg(rendered_tags));
    }
    if (settings_.show_bpm_details) {
        const QString rendered_bpm = current_song_->bpm.has_value()
                                         ? QString::number(*current_song_->bpm)
                                         : "BPM data not found!";
        details.push_back(QString("BPM: %1").arg(rendered_bpm));
    }

    return details.join(" | ");
}

void MainWindow::update_playback_labels() {
    if (!current_song_.has_value()) {
        current_song_label_->setText("CURRENT SONG: None");
        duration_label_->setText("SONG DURATION: 0 / 0");
        details_label_->clear();
        details_label_->hide();
        update_floating_overlay();
        update_practice_sheet_window();
        return;
    }

    const QString pause_suffix = paused_ ? " [PAUSED]" : "";
    current_song_label_->setText(QString("CURRENT SONG: %1%2")
                                     .arg(QString::fromStdString(current_song_->name))
                                     .arg(pause_suffix));

    const std::size_t total = current_sheet_.size();
    const bool completed = total > 0 && current_index_ >= total;
    const std::size_t display_current = total == 0 ? 0 : std::min(current_index_ + 1, total);
    const QString state_hint =
        completed ? " (Tab to Restart)"
                  : (paused_ ? " (Shift+Enter to Resume)" : " (Shift+Enter to Pause)");
    duration_label_->setText(QString("SONG DURATION: %1 / %2%3")
                                 .arg(to_qt_int(display_current))
                                 .arg(to_qt_int(total))
                                 .arg(state_hint));

    const QString details_text = build_song_details_text();
    if (details_text.isEmpty()) {
        details_label_->hide();
    } else {
        details_label_->setText(details_text);
        details_label_->show();
    }

    update_floating_overlay();
    update_practice_sheet_window();
}

void MainWindow::update_floating_overlay() {
    if (floating_overlay_ == nullptr) {
        return;
    }

    const std::string song_name = current_song_.has_value() ? current_song_->name : std::string{};
    const std::string details_text = build_song_details_text().toStdString();
    const std::size_t progress_current =
        current_sheet_.empty() ? 0 : std::min(current_index_ + 1, current_sheet_.size());
    const std::size_t progress_total = current_sheet_.size();

    if (current_sheet_.empty() || overlay_lines_.empty()) {
        floating_overlay_->set_song_progress({}, std::nullopt, {}, false, paused_, song_name,
                                             details_text, progress_current, progress_total);
        return;
    }

    if (current_index_ >= current_sheet_.size()) {
        floating_overlay_->set_song_progress({}, std::nullopt, {}, true, paused_, song_name,
                                             details_text, progress_total, progress_total);
        return;
    }

    const std::optional<std::size_t> line_index_opt = line_index_for_note_index(current_index_);
    if (!line_index_opt.has_value()) {
        floating_overlay_->set_song_progress({}, std::nullopt, {}, false, paused_, song_name,
                                             details_text, progress_current, progress_total);
        return;
    }
    const std::size_t line_index = *line_index_opt;

    const std::size_t line_start = overlay_line_starts_[line_index];
    const std::size_t key_in_line = current_index_ - line_start;
    const std::vector<std::string>& current_line = overlay_lines_[line_index];
    static const std::vector<std::string> kEmptyLine;
    const std::vector<std::string>& next_line =
        (line_index + 1 < overlay_lines_.size()) ? overlay_lines_[line_index + 1] : kEmptyLine;

    floating_overlay_->set_song_progress(current_line, key_in_line, next_line, false, paused_,
                                         song_name, details_text, progress_current, progress_total);
}

void MainWindow::update_practice_sheet_window() {
    if (floating_overlay_ == nullptr) {
        return;
    }

    floating_overlay_->set_sheet_tab_visible(settings_.show_sheet_tab_button);
    if (!settings_.show_sheet_tab_button) {
        floating_overlay_->set_sheet_view_visible(false);
        floating_overlay_->set_sheet_view_data({}, {}, std::nullopt);
        return;
    }

    floating_overlay_->set_sheet_view_visible(settings_.show_practice_sheet);
    if (!settings_.show_practice_sheet) {
        floating_overlay_->set_sheet_view_data({}, {}, std::nullopt);
        return;
    }

    if (!current_song_.has_value() || overlay_lines_.empty()) {
        floating_overlay_->set_sheet_view_data({}, {}, std::nullopt);
        return;
    }

    floating_overlay_->set_sheet_view_data(current_song_->name, overlay_lines_,
                                           line_index_for_note_index(current_index_));
}

std::vector<Song> MainWindow::selected_songs_from_table(QTableWidget* table,
                                                        const std::vector<Song>& songs) const {
    std::vector<Song> selected;
    if (table == nullptr || table->selectionModel() == nullptr) {
        return selected;
    }

    std::vector<int> rows;
    for (const QModelIndex& index : table->selectionModel()->selectedRows()) {
        rows.push_back(index.row());
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (const int row : rows) {
        if (row >= 0 && row < static_cast<int>(songs.size())) {
            selected.push_back(songs[static_cast<std::size_t>(row)]);
        }
    }
    return selected;
}

std::optional<Song> MainWindow::selected_song_from_table() const {
    const int row = song_table_->currentRow();
    if (row < 0 || row >= static_cast<int>(visible_songs_.size())) {
        return std::nullopt;
    }
    return visible_songs_[static_cast<std::size_t>(row)];
}

bool MainWindow::create_backup_zip(QString* error_message) const {
    const QString backup_zip = QDir::current().absoluteFilePath("backup.zip");
    const QString sheet_dir =
        QString::fromStdString(std::filesystem::absolute(repository_.sheet_folder()).string());

    QFile::remove(backup_zip);

    QString escaped_sheet = sheet_dir;
    escaped_sheet.replace('\'', "''");
    QString escaped_backup = backup_zip;
    escaped_backup.replace('\'', "''");

    const QString command = QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -Force")
                                .arg(escaped_sheet, escaped_backup);

    QProcess process;
    process.start("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command});
    if (!process.waitForStarted(5000)) {
        if (error_message != nullptr) {
            *error_message = "Failed to start backup process.";
        }
        return false;
    }
    if (!process.waitForFinished(-1)) {
        if (error_message != nullptr) {
            *error_message = "Backup process did not finish.";
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error_message != nullptr) {
            QString stderr_text = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (stderr_text.isEmpty()) {
                stderr_text = "PowerShell Compress-Archive returned a non-zero exit code.";
            }
            *error_message = stderr_text;
        }
        return false;
    }
    return true;
}

void MainWindow::handle_import_songs() {
    QDialog dialog(this);
    dialog.setWindowTitle("Import Songs");
    dialog.resize(640, 460);

    auto* root = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* name_edit = new QLineEdit(&dialog);
    auto* tags_edit = new QLineEdit(&dialog);
    auto* quick_tag_combo = new QComboBox(&dialog);
    auto* add_tag_button = new QPushButton("Add", &dialog);
    auto* bpm_spin = new QSpinBox(&dialog);
    auto* grouping_combo = new QComboBox(&dialog);
    auto* notes_edit = new QPlainTextEdit(&dialog);
    notes_edit->setPlaceholderText("Paste Virtual Piano notes here...");

    quick_tag_combo->addItem("Select existing tag...");
    for (const std::string& tag : tag_store_.list_all_tags()) {
        quick_tag_combo->addItem(QString::fromStdString(tag));
    }

    grouping_combo->addItem("Square Brackets [ ]", "[]");
    grouping_combo->addItem("Parentheses ( )", "()");
    bpm_spin->setRange(0, 400);
    bpm_spin->setSpecialValueText("Not set");
    bpm_spin->setValue(0);

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
    form->addRow("BPM (optional):", bpm_spin);
    form->addRow("Grouping Mode:", grouping_combo);
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

    auto* button_box =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
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
        QMessageBox::warning(this, "Import Songs", "Song name is required.");
        return;
    }

    try {
        const auto [open_brace, close_brace] =
            grouping_from_token(grouping_combo->currentData().toString());
        const std::optional<int> bpm =
            bpm_spin->value() > 0 ? std::optional<int>(bpm_spin->value()) : std::nullopt;
        const std::string saved_song_id = repository_.import_song(
            song_name, notes.toStdString(), open_brace, close_brace, '-', bpm);
        const std::vector<std::string> tags = parse_tags(tags_edit->text());
        tag_store_.set_tags_for_song(saved_song_id, tags);
        refresh_song_list();
    } catch (const std::exception& exception) {
        QMessageBox::critical(this, "Import Songs",
                              QString("Failed to import song:\n%1").arg(exception.what()));
    }
}

void MainWindow::handle_manage_songs() {
    QDialog dialog(this);
    dialog.setWindowTitle("Manage Songs");
    dialog.resize(940, 620);

    auto* root = new QVBoxLayout(&dialog);
    auto* table = new QTableWidget(&dialog);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Song", "Tags", "BPM", "Grouping"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    root->addWidget(table, 1);

    auto* button_row = new QHBoxLayout();
    auto* edit_button = new QPushButton("Edit Selected", &dialog);
    auto* delete_button = new QPushButton("Delete Selected", &dialog);
    auto* export_selected_button = new QPushButton("Export Selected", &dialog);
    auto* export_all_button = new QPushButton("Export All", &dialog);
    auto* import_button = new QPushButton("Import String", &dialog);
    auto* migrate_button = new QPushButton("Sanitize Data", &dialog);
    auto* close_button = new QPushButton("Close", &dialog);
    delete_button->setStyleSheet("QPushButton { color: #B00020; font-weight: 600; }");

    button_row->addWidget(edit_button);
    button_row->addWidget(delete_button);
    button_row->addWidget(export_selected_button);
    button_row->addWidget(export_all_button);
    button_row->addWidget(import_button);
    button_row->addWidget(migrate_button);
    button_row->addStretch(1);
    button_row->addWidget(close_button);
    root->addLayout(button_row);

    std::vector<Song> manager_songs;
    const auto refresh_manager_table = [&]() {
        manager_songs = repository_.list_songs();
        tag_store_.migrate_song_name_keys_to_ids(manager_songs);

        table->clearContents();
        table->setRowCount(0);
        for (const Song& song : manager_songs) {
            const int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(song.name)));
            table->setItem(row, 1,
                           new QTableWidgetItem(join_tags(tag_store_.tags_for_song(song.id))));
            table->setItem(row, 2, new QTableWidgetItem(bpm_display_value(song)));
            table->setItem(row, 3, new QTableWidgetItem(mode_token_for_song(song)));
        }
    };

    const auto edit_song_dialog = [&](Song& song) -> bool {
        QDialog edit_dialog(&dialog);
        edit_dialog.setWindowTitle("Edit Song");
        edit_dialog.resize(700, 520);

        auto* edit_root = new QVBoxLayout(&edit_dialog);
        auto* form = new QFormLayout();
        auto* name_edit = new QLineEdit(QString::fromStdString(song.name), &edit_dialog);
        auto* tags_edit = new QLineEdit(join_tags(tag_store_.tags_for_song(song.id)), &edit_dialog);
        auto* quick_tag_combo = new QComboBox(&edit_dialog);
        auto* add_tag_button = new QPushButton("Add", &edit_dialog);
        auto* bpm_spin = new QSpinBox(&edit_dialog);
        auto* grouping_combo = new QComboBox(&edit_dialog);
        auto* notes_edit = new QPlainTextEdit(
            QString::fromStdString(repository_.load_raw_sheet_text(song)), &edit_dialog);

        quick_tag_combo->addItem("Select existing tag...");
        for (const std::string& tag : tag_store_.list_all_tags()) {
            quick_tag_combo->addItem(QString::fromStdString(tag));
        }

        bpm_spin->setRange(0, 400);
        bpm_spin->setSpecialValueText("Not set");
        bpm_spin->setValue(song.bpm.value_or(0));

        grouping_combo->addItem("Square Brackets [ ]", "[]");
        grouping_combo->addItem("Parentheses ( )", "()");
        const int grouping_index = grouping_combo->findData(mode_token_for_song(song));
        grouping_combo->setCurrentIndex(grouping_index < 0 ? 0 : grouping_index);

        auto* quick_tag_row = new QWidget(&edit_dialog);
        auto* quick_tag_layout = new QHBoxLayout(quick_tag_row);
        quick_tag_layout->setContentsMargins(0, 0, 0, 0);
        quick_tag_layout->setSpacing(6);
        quick_tag_layout->addWidget(quick_tag_combo, 1);
        quick_tag_layout->addWidget(add_tag_button);

        form->addRow("Song Name:", name_edit);
        form->addRow("Tags:", tags_edit);
        form->addRow("Quick Tag:", quick_tag_row);
        form->addRow("BPM (optional):", bpm_spin);
        form->addRow("Grouping Mode:", grouping_combo);
        edit_root->addLayout(form);
        edit_root->addWidget(notes_edit, 1);

        const auto handle_quick_tag = [tags_edit, quick_tag_combo]() {
            if (quick_tag_combo->currentIndex() <= 0) {
                return;
            }
            append_tag_to_text(tags_edit, quick_tag_combo->currentText());
        };
        connect(add_tag_button, &QPushButton::clicked, &edit_dialog, handle_quick_tag);
        connect(quick_tag_combo, &QComboBox::activated, &edit_dialog,
                [handle_quick_tag](int index) {
                    if (index > 0) {
                        handle_quick_tag();
                    }
                });

        auto* buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &edit_dialog);
        edit_root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &edit_dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &edit_dialog, &QDialog::reject);

        if (edit_dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString notes = notes_edit->toPlainText().trimmed();
        if (notes.isEmpty()) {
            QMessageBox::warning(this, "Manage Songs", "Song notes cannot be empty.");
            return false;
        }

        std::string requested_name = name_edit->text().trimmed().toStdString();
        if (requested_name.empty()) {
            QMessageBox::warning(&edit_dialog, "Manage Songs", "Song name is required.");
            return false;
        }

        const auto [open_brace, close_brace] =
            grouping_from_token(grouping_combo->currentData().toString());
        song.open_brace = open_brace;
        song.close_brace = close_brace;
        song.sustain_indicator = '-';
        song.bpm = bpm_spin->value() > 0 ? std::optional<int>(bpm_spin->value()) : std::nullopt;

        try {
            song.name = repository_.rename_song(song, requested_name);
            repository_.update_song_contents(song, notes.toStdString());
            tag_store_.set_tags_for_song(song.id, parse_tags(tags_edit->text()));
            if (current_song_.has_value() && current_song_->id == song.id) {
                select_song(song);
            }
            return true;
        } catch (const std::exception& exception) {
            QMessageBox::critical(this, "Manage Songs",
                                  QString("Failed to save changes:\n%1").arg(exception.what()));
            return false;
        }
    };

    const auto show_export_dialog = [&](const QString& share_payload) {
        QDialog export_dialog(&dialog);
        export_dialog.setWindowTitle("Export Song Data");
        export_dialog.resize(760, 420);
        auto* export_root = new QVBoxLayout(&export_dialog);
        auto* text = new QPlainTextEdit(share_payload, &export_dialog);
        text->setReadOnly(false);
        text->setWordWrapMode(QTextOption::NoWrap);
        export_root->addWidget(text, 1);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &export_dialog);
        auto* copy_button = new QPushButton("Copy", &export_dialog);
        buttons->addButton(copy_button, QDialogButtonBox::ActionRole);
        connect(copy_button, &QPushButton::clicked, &export_dialog, [text]() {
            if (QClipboard* clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
                clipboard->setText(text->toPlainText());
            }
        });
        connect(buttons, &QDialogButtonBox::rejected, &export_dialog, &QDialog::reject);
        export_root->addWidget(buttons);

        export_dialog.exec();
    };

    refresh_manager_table();

    connect(edit_button, &QPushButton::clicked, &dialog, [&]() {
        std::vector<Song> selected = selected_songs_from_table(table, manager_songs);
        if (selected.size() != 1) {
            QMessageBox::information(&dialog, "Manage Songs", "Select exactly one song to edit.");
            return;
        }

        Song edited = selected.front();
        if (edit_song_dialog(edited)) {
            refresh_manager_table();
            refresh_song_list();
        }
    });

    connect(delete_button, &QPushButton::clicked, &dialog, [&]() {
        const std::vector<Song> selected = selected_songs_from_table(table, manager_songs);
        if (selected.empty()) {
            QMessageBox::information(&dialog, "Manage Songs",
                                     "Select one or more songs to delete.");
            return;
        }

        const auto choice = QMessageBox::question(
            &dialog, "Delete Songs",
            QString("Delete %1 selected song(s)?").arg(to_qt_int(selected.size())),
            QMessageBox::Yes | QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }

        for (const Song& song : selected) {
            repository_.delete_song(song);
            tag_store_.remove_song(song.id);
            if (current_song_.has_value() && current_song_->id == song.id) {
                current_song_.reset();
                current_sheet_.clear();
                overlay_lines_.clear();
                overlay_line_starts_.clear();
                current_index_ = 0;
                waiting_for_release_ = false;
                paused_ = false;
            }
        }

        refresh_manager_table();
        refresh_song_list();
    });

    connect(export_selected_button, &QPushButton::clicked, &dialog, [&]() {
        const std::vector<Song> selected = selected_songs_from_table(table, manager_songs);
        if (selected.empty()) {
            QMessageBox::information(&dialog, "Export Songs",
                                     "Select one or more songs to export.");
            return;
        }
        show_export_dialog(build_export_share_string(selected, repository_, tag_store_));
    });

    connect(export_all_button, &QPushButton::clicked, &dialog, [&]() {
        if (manager_songs.empty()) {
            QMessageBox::information(&dialog, "Export Songs", "There are no songs to export.");
            return;
        }
        show_export_dialog(build_export_share_string(manager_songs, repository_, tag_store_));
    });

    connect(import_button, &QPushButton::clicked, &dialog, [&]() {
        QDialog import_dialog(&dialog);
        import_dialog.setWindowTitle("Import Share String");
        import_dialog.resize(760, 420);
        auto* import_root = new QVBoxLayout(&import_dialog);
        auto* input = new QPlainTextEdit(&import_dialog);
        input->setPlaceholderText("Paste exported SMX1 share string here...");
        input->setWordWrapMode(QTextOption::NoWrap);
        import_root->addWidget(input, 1);

        auto* buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &import_dialog);
        import_root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &import_dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &import_dialog, &QDialog::reject);

        if (import_dialog.exec() != QDialog::Accepted) {
            return;
        }

        QJsonArray songs_to_import;
        QString parse_error;
        if (!parse_export_share_string(input->toPlainText(), &songs_to_import, &parse_error)) {
            QMessageBox::critical(&dialog, "Import Share String", parse_error);
            return;
        }

        int imported_count = 0;
        for (const QJsonValue& value : songs_to_import) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject item = value.toObject();
            const QString name = item.value("name").toString().trimmed();
            const QString notes = item.value("notes").toString();
            const QString grouping = item.value("grouping").toString("[]");
            const auto [open_brace, close_brace] = grouping_from_token(grouping);

            if (name.isEmpty() || notes.trimmed().isEmpty()) {
                continue;
            }

            std::optional<int> bpm = std::nullopt;
            if (item.value("bpm").isDouble()) {
                const int bpm_value = item.value("bpm").toInt();
                if (bpm_value > 0) {
                    bpm = bpm_value;
                }
            }

            std::vector<std::string> tags;
            if (item.value("tags").isArray()) {
                for (const QJsonValue& tag_value : item.value("tags").toArray()) {
                    const QString tag = tag_value.toString().trimmed();
                    if (!tag.isEmpty()) {
                        tags.push_back(tag.toStdString());
                    }
                }
            }

            const std::string id = repository_.import_song(name.toStdString(), notes.toStdString(),
                                                           open_brace, close_brace, '-', bpm);
            tag_store_.set_tags_for_song(id, tags);
            ++imported_count;
        }

        refresh_manager_table();
        refresh_song_list();
        QMessageBox::information(&dialog, "Import Share String",
                                 QString("Imported %1 song(s).").arg(imported_count));
    });

    connect(migrate_button, &QPushButton::clicked, &dialog, [&]() {
        const MigrationSummary file_preview = repository_.preview_convention_migration();
        const std::vector<Song> preview_songs = repository_.list_songs();
        const TagRepairSummary tag_preview =
            tag_store_.preview_sanitize_repairs(preview_songs, kDefaultTag);
        if (!file_preview.has_changes() && !tag_preview.has_changes()) {
            QMessageBox::information(&dialog, "Sanitize Data",
                                     "No sanitize changes are currently needed.");
            return;
        }

        const QString summary = sanitize_summary_text(file_preview, tag_preview);
        const auto confirm = QMessageBox::question(
            &dialog, "Sanitize Data",
            summary +
                "\n\nA full backup of your sheets folder will be created as backup.zip before "
                "sanitization.\nProceed?",
            QMessageBox::Yes | QMessageBox::No);
        if (confirm != QMessageBox::Yes) {
            return;
        }

        QString backup_error;
        if (!create_backup_zip(&backup_error)) {
            QMessageBox::critical(&dialog, "Sanitize Data",
                                  QString("Failed to create backup.zip:\n%1").arg(backup_error));
            return;
        }

        const MigrationSummary file_applied = repository_.apply_convention_migration();
        const std::vector<Song> applied_songs = repository_.list_songs();
        const TagRepairSummary tag_applied =
            tag_store_.apply_sanitize_repairs(applied_songs, kDefaultTag);
        refresh_manager_table();
        refresh_song_list();

        QMessageBox::information(&dialog, "Sanitize Data",
                                 QString("Sanitization complete.\n\n%1")
                                     .arg(sanitize_summary_text(file_applied, tag_applied)));
    });

    connect(close_button, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::handle_settings() {
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    dialog.resize(560, 420);

    auto* root = new QVBoxLayout(&dialog);
    root->setSpacing(10);

    auto* playback_group = new QGroupBox("Playback", &dialog);
    auto* details_group = new QGroupBox("Song Details", &dialog);
    auto* sheet_group = new QGroupBox("Overlay Sheet View", &dialog);

    auto* playback_layout = new QVBoxLayout(playback_group);
    auto* details_layout = new QVBoxLayout(details_group);
    auto* sheet_layout = new QVBoxLayout(sheet_group);

    auto* strict_checkbox = new QCheckBox("Strict Mode", playback_group);
    auto* poll_spin = new QSpinBox(playback_group);
    auto* chunking_combo = new QComboBox(playback_group);
    auto* poll_label = new QLabel("Playback Poll Interval:", playback_group);
    auto* chunking_label = new QLabel("Overlay Chunking:", playback_group);
    auto* show_details_checkbox = new QCheckBox("Show Song Details", details_group);
    auto* show_tags_checkbox = new QCheckBox("Show Tags in Details", details_group);
    auto* show_bpm_checkbox = new QCheckBox("Show BPM in Details", details_group);
    auto* practice_window_checkbox = new QCheckBox("Show Sheet View Tab Button", sheet_group);
    auto* sheet_hint = new QLabel(
        "Hides or shows the Sheet View tab button on the floating overlay.", sheet_group);
    sheet_hint->setWordWrap(true);
    sheet_hint->setStyleSheet("QLabel { color: #8A8A8A; }");

    auto* playback_form = new QFormLayout();
    playback_form->addRow(poll_label, poll_spin);
    playback_form->addRow(chunking_label, chunking_combo);
    playback_layout->addWidget(strict_checkbox);
    playback_layout->addLayout(playback_form);

    details_layout->addWidget(show_details_checkbox);
    details_layout->addWidget(show_tags_checkbox);
    details_layout->addWidget(show_bpm_checkbox);

    sheet_layout->addWidget(practice_window_checkbox);
    sheet_layout->addWidget(sheet_hint);

    root->addWidget(playback_group);
    root->addWidget(details_group);
    root->addWidget(sheet_group);

    poll_spin->setRange(1, 100);
    poll_spin->setSuffix(" ms");
    poll_spin->setValue(settings_.input_poll_interval_ms);
    strict_checkbox->setToolTip(kStrictModeTooltip);
    strict_checkbox->setToolTipDuration(20000);
    poll_spin->setToolTip(kPollIntervalTooltip);
    poll_spin->setToolTipDuration(20000);
    chunking_combo->addItem("Auto Detect");
    chunking_combo->addItem("Smart");
    chunking_combo->setCurrentIndex(chunking_mode_to_combo_index(settings_.overlay_chunking_mode));
    chunking_combo->setToolTip(kOverlayChunkingTooltip);
    chunking_combo->setToolTipDuration(20000);
    poll_label->setToolTip(kPollIntervalTooltip);
    poll_label->setToolTipDuration(20000);
    chunking_label->setToolTip(kOverlayChunkingTooltip);
    chunking_label->setToolTipDuration(20000);
    strict_checkbox->setChecked(settings_.strict_mode);
    show_details_checkbox->setChecked(settings_.show_song_details);
    show_tags_checkbox->setChecked(settings_.show_tag_details);
    show_bpm_checkbox->setChecked(settings_.show_bpm_details);
    practice_window_checkbox->setChecked(settings_.show_sheet_tab_button);

    show_tags_checkbox->setEnabled(show_details_checkbox->isChecked());
    show_bpm_checkbox->setEnabled(show_details_checkbox->isChecked());
    connect(show_details_checkbox, &QCheckBox::toggled, &dialog,
            [show_tags_checkbox, show_bpm_checkbox](const bool checked) {
                show_tags_checkbox->setEnabled(checked);
                show_bpm_checkbox->setEnabled(checked);
            });

    auto* tooltip_filter = new InstantToolTipFilter(&dialog);
    strict_checkbox->installEventFilter(tooltip_filter);
    poll_spin->installEventFilter(tooltip_filter);
    chunking_combo->installEventFilter(tooltip_filter);
    poll_label->installEventFilter(tooltip_filter);
    chunking_label->installEventFilter(tooltip_filter);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    settings_.strict_mode = strict_checkbox->isChecked();
    settings_.input_poll_interval_ms = poll_spin->value();
    settings_.overlay_chunking_mode =
        chunking_mode_from_combo_index(chunking_combo->currentIndex());
    settings_.show_song_details = show_details_checkbox->isChecked();
    settings_.show_tag_details = show_tags_checkbox->isChecked();
    settings_.show_bpm_details = show_bpm_checkbox->isChecked();
    settings_.show_sheet_tab_button = practice_window_checkbox->isChecked();
    if (!settings_.show_sheet_tab_button) {
        settings_.show_practice_sheet = false;
    }
    if (floating_overlay_ != nullptr) {
        floating_overlay_->set_sheet_tab_visible(settings_.show_sheet_tab_button);
        floating_overlay_->set_sheet_view_visible(settings_.show_practice_sheet);
    }
    strict_mode_checkbox_->setChecked(settings_.strict_mode);
    input_poll_timer_.setInterval(settings_.input_poll_interval_ms);
    settings_store_.save(settings_);

    if (current_song_.has_value()) {
        rebuild_overlay_lines(*current_song_);
    }
    update_playback_labels();
}

void MainWindow::handle_check_updates() { check_for_updates(true); }

void MainWindow::maybe_check_for_updates_on_startup() {
    if (!should_check_for_updates_now()) {
        return;
    }
    check_for_updates(false);
}

void MainWindow::check_for_updates(const bool user_initiated) {
    if (update_network_ == nullptr) {
        return;
    }

    if (update_metadata_reply_ != nullptr || update_download_reply_ != nullptr) {
        if (user_initiated) {
            QMessageBox::information(this, "Update check", "An update check is already running.");
        }
        return;
    }

    update_user_initiated_check_ = user_initiated;
    record_update_check_now();

    QNetworkRequest request(QUrl(QString::fromLatin1(AppInfo::kLatestReleaseApiUrl)));
    request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QString("%1/%2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    update_metadata_reply_ = update_network_->get(request);
    connect(update_metadata_reply_, &QNetworkReply::finished, this,
            &MainWindow::handle_update_metadata_reply);
}

void MainWindow::handle_update_metadata_reply() {
    const bool user_initiated = update_user_initiated_check_;
    QPointer<QNetworkReply> reply = update_metadata_reply_;
    update_metadata_reply_ = nullptr;
    if (reply == nullptr) {
        return;
    }

    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError network_error = reply->error();
    const QString network_error_text = reply->errorString();
    reply->deleteLater();

    if (network_error != QNetworkReply::NoError) {
        if (user_initiated) {
            QMessageBox::warning(this, "Update check",
                                 QString("Failed to check updates:\n%1").arg(network_error_text));
        }
        return;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (user_initiated) {
            QMessageBox::warning(this, "Update check", "Update metadata was invalid.");
        }
        return;
    }

    const QJsonObject root = document.object();
    const QString latest_tag = root.value("tag_name").toString().trimmed();
    const QString release_url =
        root.value("html_url").toString(QString::fromLatin1(AppInfo::kReleasesPageUrl));
    if (latest_tag.isEmpty()) {
        if (user_initiated) {
            QMessageBox::warning(this, "Update check",
                                 "Latest release did not include a version tag.");
        }
        return;
    }

    QString local_version = QCoreApplication::applicationVersion().trimmed();
    if (local_version.isEmpty()) {
        local_version = "0.0.0";
    }
    if (!is_remote_version_newer(latest_tag.toStdString(), local_version.toStdString())) {
        if (user_initiated) {
            QMessageBox::information(this, "Update check", "You're already on the latest version.");
        }
        return;
    }

    const SelectedReleaseAsset asset = pick_best_release_asset(parse_release_assets(root.value("assets").toArray()));
    if (!asset.has_download) {
        const QMessageBox::StandardButton open_releases = QMessageBox::question(
            this, "Update available",
            QString("A new version (%1) is available, but no downloadable asset was found.\n\n"
                    "Open the releases page now?")
                .arg(latest_tag),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (open_releases == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl(release_url));
        }
        return;
    }

    QString message =
        QString("A new version (%1) is available.\nCurrent version: %2\n\nDownload and install now?")
            .arg(latest_tag, local_version);
    if (!asset.is_installer) {
        message += "\n\nNo installer asset was found. SheetMaster can download this release "
                   "package, but installation may require manual steps.";
    }

    if (QMessageBox::question(this, "Update available", message, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes) != QMessageBox::Yes) {
        return;
    }

    cleanup_update_download(false);

    QString temp_root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (temp_root.isEmpty()) {
        temp_root = QDir::tempPath();
    }
    if (temp_root.isEmpty() || !QDir().mkpath(temp_root)) {
        QMessageBox::warning(this, "Update download", "No writable temp directory is available.");
        return;
    }

    const QString safe_name = QFileInfo(QString::fromStdString(asset.name)).fileName();
    update_downloaded_asset_name_ =
        safe_name.isEmpty() ? QString("sheetmaster-update-%1.bin").arg(latest_tag) : safe_name;
    update_downloaded_file_path_ = QDir(temp_root).filePath(update_downloaded_asset_name_);
    update_expected_sha256_hex_ = QString::fromStdString(asset.sha256_hex).trimmed().toLower();

    update_download_file_ = new QFile(update_downloaded_file_path_);
    if (!update_download_file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete update_download_file_;
        update_download_file_ = nullptr;
        QMessageBox::warning(this, "Update download", "Failed to open local file for update download.");
        return;
    }

    QNetworkRequest download_request(QUrl(QString::fromStdString(asset.download_url)));
    download_request.setHeader(
        QNetworkRequest::UserAgentHeader,
        QString("%1/%2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()));
    download_request.setRawHeader("Accept", "application/octet-stream");

    update_download_reply_ = update_network_->get(download_request);
    connect(update_download_reply_, &QNetworkReply::readyRead, this,
            &MainWindow::handle_update_download_ready_read);
    connect(update_download_reply_, &QNetworkReply::finished, this,
            &MainWindow::handle_update_download_finished);
}

void MainWindow::handle_update_download_ready_read() {
    if (update_download_reply_ == nullptr || update_download_file_ == nullptr) {
        return;
    }

    const QByteArray chunk = update_download_reply_->readAll();
    if (!chunk.isEmpty()) {
        update_download_file_->write(chunk);
    }
}

void MainWindow::handle_update_download_finished() {
    QPointer<QNetworkReply> reply = update_download_reply_;
    update_download_reply_ = nullptr;
    if (reply == nullptr) {
        return;
    }

    handle_update_download_ready_read();

    if (update_download_file_ != nullptr) {
        update_download_file_->flush();
        update_download_file_->close();
        delete update_download_file_;
        update_download_file_ = nullptr;
    }

    const QNetworkReply::NetworkError network_error = reply->error();
    const QString network_error_text = reply->errorString();
    reply->deleteLater();

    if (network_error != QNetworkReply::NoError) {
        cleanup_update_download(false);
        QMessageBox::warning(this, "Update download",
                             QString("Failed to download update:\n%1").arg(network_error_text));
        return;
    }

    if (!update_expected_sha256_hex_.isEmpty()) {
        QFile downloaded_file(update_downloaded_file_path_);
        if (!downloaded_file.open(QIODevice::ReadOnly)) {
            cleanup_update_download(false);
            QMessageBox::warning(this, "Update verification",
                                 "Downloaded update could not be reopened for verification.");
            return;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!downloaded_file.atEnd()) {
            hash.addData(downloaded_file.read(64 * 1024));
        }
        const QString actual_digest = QString::fromLatin1(hash.result().toHex()).toLower();
        if (actual_digest != update_expected_sha256_hex_) {
            cleanup_update_download(false);
            QMessageBox::warning(this, "Update verification",
                                 "Downloaded update failed checksum verification.");
            return;
        }
    }

    const bool installer_asset = is_installer_asset_name(update_downloaded_asset_name_.toStdString());
    if (!installer_asset) {
        QMessageBox::information(
            this, "Update downloaded",
            QString("Downloaded the latest release package to:\n%1\n\nNo installer was detected "
                    "in this asset. You can install it manually.")
                .arg(QDir::toNativeSeparators(update_downloaded_file_path_)));
        cleanup_update_download(true);
        return;
    }

    if (QMessageBox::question(this, "Install update",
                              QString("Update downloaded to:\n%1\n\nInstall now? SheetMaster will close.")
                                  .arg(QDir::toNativeSeparators(update_downloaded_file_path_)),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes) != QMessageBox::Yes) {
        cleanup_update_download(true);
        return;
    }

    const QString lower_name = update_downloaded_asset_name_.toLower();
    const bool started = lower_name.endsWith(".msi")
                             ? QProcess::startDetached(
                                   "msiexec",
                                   QStringList() << "/i"
                                                 << QDir::toNativeSeparators(update_downloaded_file_path_))
                             : QProcess::startDetached(QDir::toNativeSeparators(update_downloaded_file_path_),
                                                       QStringList());
    if (!started) {
        QMessageBox::warning(this, "Install update", "Failed to launch installer.");
        cleanup_update_download(true);
        return;
    }

    cleanup_update_download(true);
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void MainWindow::cleanup_update_download(const bool keep_downloaded_file) {
    if (update_download_reply_ != nullptr) {
        disconnect(update_download_reply_, nullptr, this, nullptr);
        update_download_reply_->abort();
        update_download_reply_->deleteLater();
        update_download_reply_ = nullptr;
    }

    if (update_download_file_ != nullptr) {
        if (update_download_file_->isOpen()) {
            update_download_file_->close();
        }
        delete update_download_file_;
        update_download_file_ = nullptr;
    }

    if (!keep_downloaded_file && !update_downloaded_file_path_.isEmpty()) {
        QFile::remove(update_downloaded_file_path_);
    }

    update_downloaded_file_path_.clear();
    update_downloaded_asset_name_.clear();
    update_expected_sha256_hex_.clear();
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
        floating_overlay_->set_sheet_tab_visible(settings_.show_sheet_tab_button);
        floating_overlay_->set_sheet_view_visible(settings_.show_practice_sheet);
        floating_overlay_->show();
        floating_overlay_->raise();
        update_floating_overlay();
        update_practice_sheet_window();
    } else {
        floating_overlay_->hide();
    }
}

void MainWindow::restart_current_song() {
    if (!current_song_.has_value() || current_sheet_.empty()) {
        return;
    }

    current_index_ = 0;
    paused_ = false;
    waiting_for_release_ = true;
    update_playback_labels();
}

void MainWindow::apply_manual_navigation_hotkeys() {
    if (!current_song_.has_value() || current_sheet_.empty()) {
        return;
    }

    const bool left_down = is_left_key_down();
    const bool right_down = is_right_key_down();
    const bool up_down = is_up_key_down();
    const bool down_down = is_down_key_down();
    bool changed = false;

    if (left_down && !left_key_latched_) {
        if (current_index_ > 0) {
            --current_index_;
            changed = true;
        }
    }
    left_key_latched_ = left_down;

    if (right_down && !right_key_latched_) {
        if (current_index_ < current_sheet_.size()) {
            ++current_index_;
            changed = true;
        }
    }
    right_key_latched_ = right_down;

    if (up_down && !up_key_latched_) {
        if (const std::optional<std::size_t> line_index = line_index_for_note_index(current_index_);
            line_index.has_value()) {
            if (*line_index == 0) {
                current_index_ = 0;
            } else {
                current_index_ = overlay_line_starts_[*line_index - 1];
            }
            changed = true;
        }
    }
    up_key_latched_ = up_down;

    if (down_down && !down_key_latched_) {
        if (const std::optional<std::size_t> line_index = line_index_for_note_index(current_index_);
            line_index.has_value()) {
            if (*line_index + 1 < overlay_line_starts_.size()) {
                current_index_ = overlay_line_starts_[*line_index + 1];
            } else {
                current_index_ = current_sheet_.size();
            }
            changed = true;
        }
    }
    down_key_latched_ = down_down;

    if (changed) {
        waiting_for_release_ = true;
        update_playback_labels();
    }
}

void MainWindow::poll_input() {
    const bool pause_combo_down = is_pause_combo_down();
    if (pause_combo_down && !pause_combo_latched_) {
        paused_ = !paused_;
        pause_combo_latched_ = true;
        waiting_for_release_ = true;
        update_playback_labels();
    } else if (!pause_combo_down) {
        pause_combo_latched_ = false;
    }

    if (!current_song_.has_value()) {
        return;
    }

    apply_manual_navigation_hotkeys();

    if (current_sheet_.empty()) {
        return;
    }

    const bool tab_down = is_tab_key_down();
    if (current_index_ >= current_sheet_.size()) {
        if (tab_down && !tab_key_latched_) {
            restart_current_song();
            tab_key_latched_ = true;
        } else if (!tab_down) {
            tab_key_latched_ = false;
        }
        return;
    }
    tab_key_latched_ = tab_down;

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
        if (current_index_ + 1 >= current_sheet_.size()) {
            current_index_ = current_sheet_.size();
        } else {
            ++current_index_;
        }
        waiting_for_release_ = true;
        update_playback_labels();
    }
}

} // namespace piano_assist

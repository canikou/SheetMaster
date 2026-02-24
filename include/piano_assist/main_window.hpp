#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "piano_assist/keyboard.hpp"
#include "piano_assist/settings_store.hpp"
#include "piano_assist/song_repository.hpp"
#include "piano_assist/tag_store.hpp"
#include "piano_assist/types.hpp"

class QCheckBox;
class QComboBox;
class QFile;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QString;
class QTableWidget;

namespace piano_assist {
class FloatingOverlayWindow;
} // namespace piano_assist

namespace piano_assist {

class MainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

  private slots:
    void refresh_song_list();
    void handle_song_double_click(int row, int column);
    void handle_import_songs();
    void handle_manage_songs();
    void handle_settings();
    void handle_check_updates();
    void handle_strict_mode_toggle(bool checked);
    void handle_overlay_toggle(bool checked);
    void poll_input();

  private:
    SongRepository repository_;
    TagStore tag_store_;
    SettingsStore settings_store_;
    AppSettings settings_;
    KeyboardInput keyboard_;

    QTimer input_poll_timer_;

    QLineEdit* search_edit_{nullptr};
    QComboBox* tag_filter_{nullptr};
    QTableWidget* song_table_{nullptr};
    QPushButton* import_button_{nullptr};
    QPushButton* manage_button_{nullptr};
    QPushButton* settings_button_{nullptr};
    QPushButton* check_updates_button_{nullptr};
    QLabel* current_song_label_{nullptr};
    QLabel* duration_label_{nullptr};
    QLabel* details_label_{nullptr};
    QCheckBox* strict_mode_checkbox_{nullptr};
    QCheckBox* overlay_checkbox_{nullptr};
    std::unique_ptr<FloatingOverlayWindow> floating_overlay_;

    std::vector<Song> visible_songs_;
    std::optional<Song> current_song_;
    std::vector<NoteGroup> current_sheet_;
    std::vector<std::vector<std::string>> overlay_lines_;
    std::vector<std::size_t> overlay_line_starts_;
    std::size_t current_index_{0};
    bool waiting_for_release_{false};
    bool paused_{false};
    bool pause_combo_latched_{false};
    bool left_key_latched_{false};
    bool right_key_latched_{false};
    bool up_key_latched_{false};
    bool down_key_latched_{false};
    bool tab_key_latched_{false};
    QNetworkAccessManager* update_network_{nullptr};
    QPointer<QNetworkReply> update_metadata_reply_;
    QPointer<QNetworkReply> update_download_reply_;
    QFile* update_download_file_{nullptr};
    QString update_downloaded_file_path_;
    QString update_downloaded_asset_name_;
    QString update_expected_sha256_hex_;
    bool update_user_initiated_check_{false};

    void build_ui();
    void position_window_for_overlay();
    void repopulate_tag_filter();
    void select_song(const Song& song);
    void rebuild_overlay_lines(const Song& song);
    void update_playback_labels();
    void update_floating_overlay();
    void update_practice_sheet_window();
    [[nodiscard]] QString build_song_details_text() const;
    void restart_current_song();
    void apply_manual_navigation_hotkeys();
    [[nodiscard]] bool create_backup_zip(QString* error_message) const;
    [[nodiscard]] std::optional<std::size_t>
    line_index_for_note_index(std::size_t note_index) const;
    [[nodiscard]] std::vector<Song> selected_songs_from_table(QTableWidget* table,
                                                              const std::vector<Song>& songs) const;
    [[nodiscard]] std::optional<Song> selected_song_from_table() const;
    void maybe_check_for_updates_on_startup();
    void check_for_updates(bool user_initiated);
    void handle_update_metadata_reply();
    void handle_update_download_ready_read();
    void handle_update_download_finished();
    void cleanup_update_download(bool keep_downloaded_file);
};

} // namespace piano_assist

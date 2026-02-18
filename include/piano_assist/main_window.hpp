#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QMainWindow>
#include <QTimer>

#include "piano_assist/keyboard.hpp"
#include "piano_assist/settings_store.hpp"
#include "piano_assist/song_repository.hpp"
#include "piano_assist/tag_store.hpp"
#include "piano_assist/types.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace piano_assist {
class FloatingOverlayWindow;
}

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
    QLabel* current_song_label_{nullptr};
    QLabel* duration_label_{nullptr};
    QCheckBox* strict_mode_checkbox_{nullptr};
    QCheckBox* overlay_checkbox_{nullptr};
    QListWidget* key_list_{nullptr};
    std::unique_ptr<FloatingOverlayWindow> floating_overlay_;

    std::vector<Song> visible_songs_;
    std::optional<Song> current_song_;
    std::vector<NoteGroup> current_sheet_;
    std::vector<std::vector<std::string>> overlay_lines_;
    std::vector<std::size_t> overlay_line_starts_;
    std::size_t current_index_{0};
    bool waiting_for_release_{false};
    bool paused_{false};
    bool pause_key_latched_{false};

    void build_ui();
    void repopulate_tag_filter();
    void select_song(const Song& song);
    void rebuild_overlay_lines(const Song& song);
    void update_playback_labels();
    void update_floating_overlay();
    [[nodiscard]] std::optional<Song> selected_song_from_table() const;
};

} // namespace piano_assist

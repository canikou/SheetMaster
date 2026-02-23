#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QPoint>
#include <QWidget>

class QFrame;
class QHideEvent;
class QLabel;
class QMoveEvent;
class QMouseEvent;
class QPushButton;
class QResizeEvent;
class QShowEvent;

namespace piano_assist {

class FloatingOverlayWindow final : public QWidget {
    Q_OBJECT

  public:
    explicit FloatingOverlayWindow(QWidget* parent = nullptr);
    ~FloatingOverlayWindow() override = default;

    void set_song_progress(const std::vector<std::string>& current_line,
                           std::optional<std::size_t> highlighted_key_index,
                           const std::vector<std::string>& next_line, bool completed, bool paused,
                           std::string_view song_name, std::string_view details_text,
                           std::size_t progress_current, std::size_t progress_total);
    void set_sheet_tab_visible(bool visible);
    [[nodiscard]] bool is_sheet_tab_visible() const { return sheet_tab_visible_; }
    void set_sheet_view_data(std::string_view song_name,
                             const std::vector<std::vector<std::string>>& sheet_lines,
                             std::optional<std::size_t> focused_line_index);
    void set_sheet_view_visible(bool visible);
    [[nodiscard]] bool is_sheet_view_visible() const { return sheet_view_visible_; }

  signals:
    void sheet_view_toggled(bool visible);

  protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    QFrame* overlay_panel_{nullptr};
    QLabel* info_label_{nullptr};
    QLabel* left_details_label_{nullptr};
    QLabel* right_details_label_{nullptr};
    QLabel* current_label_{nullptr};
    QLabel* next_label_{nullptr};
    std::unique_ptr<QWidget> sheet_tab_window_;
    QPushButton* sheet_tab_button_{nullptr};
    std::unique_ptr<QWidget> sheet_overlay_window_;
    QLabel* sheet_title_label_{nullptr};
    std::vector<QLabel*> sheet_line_labels_{};
    std::vector<std::vector<std::string>> sheet_lines_{};
    std::optional<std::size_t> focused_sheet_line_{};
    bool sheet_tab_visible_{true};
    bool sheet_view_visible_{false};
    std::string current_sheet_song_name_{};
    QPoint drag_offset_{};
    bool dragging_{false};

    void ensure_sheet_tab_window();
    void ensure_sheet_overlay_window();
    void reposition_sheet_overlay();
    void reposition_sheet_tab();
    void update_sheet_toggle_button();
    void refresh_sheet_lines();
};

} // namespace piano_assist

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include <QPoint>
#include <QWidget>

class QLabel;
class QMouseEvent;

namespace piano_assist {

class FloatingOverlayWindow final : public QWidget {
public:
    explicit FloatingOverlayWindow(QWidget* parent = nullptr);
    ~FloatingOverlayWindow() override = default;

    void set_song_progress(
        const std::vector<std::string>& current_line,
        std::optional<std::size_t> highlighted_key_index,
        const std::vector<std::string>& next_line,
        bool completed,
        bool paused,
        std::string_view song_name,
        std::size_t progress_current,
        std::size_t progress_total
    );

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QLabel* info_label_{nullptr};
    QLabel* current_label_{nullptr};
    QLabel* next_label_{nullptr};
    QPoint drag_offset_{};
    bool dragging_{false};
};

} // namespace piano_assist

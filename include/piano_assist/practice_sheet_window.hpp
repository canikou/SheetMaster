#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include <QWidget>

class QCheckBox;
class QLabel;
class QListWidget;
class QHideEvent;
class QShowEvent;

namespace piano_assist {

class PracticeSheetWindow final : public QWidget {
    Q_OBJECT

  public:
    explicit PracticeSheetWindow(QWidget* parent = nullptr);
    ~PracticeSheetWindow() override = default;

    void clear_song();
    void set_song_lines(std::string_view song_name,
                        const std::vector<std::vector<std::string>>& lines);
    void set_current_line(std::size_t line_index);

  signals:
    void visibility_changed(bool visible);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    QLabel* title_label_{nullptr};
    QCheckBox* auto_scroll_checkbox_{nullptr};
    QListWidget* lines_list_{nullptr};
    std::size_t tracked_line_index_{0};

    void apply_scroll_position(bool force);
};

} // namespace piano_assist

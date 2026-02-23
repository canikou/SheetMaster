#include "piano_assist/practice_sheet_window.hpp"

#include <algorithm>
#include <string>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QPoint>
#include <QShowEvent>
#include <QStringList>
#include <QVBoxLayout>

namespace piano_assist {
namespace {

QString line_to_display(const std::vector<std::string>& line) {
    QStringList tokens;
    tokens.reserve(static_cast<qsizetype>(line.size()));
    for (const std::string& key : line) {
        tokens.push_back(QString::fromStdString(key));
    }
    return tokens.join("   ");
}

} // namespace

PracticeSheetWindow::PracticeSheetWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle("Sheet View");
    resize(900, 640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    title_label_ = new QLabel("Sheet View", this);
    QFont font = title_label_->font();
    font.setPointSize(12);
    font.setBold(true);
    title_label_->setFont(font);

    auto_scroll_checkbox_ = new QCheckBox("Auto Scroll", this);
    auto_scroll_checkbox_->setChecked(true);

    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(8);
    header_row->addWidget(title_label_);
    header_row->addStretch(1);
    header_row->addWidget(auto_scroll_checkbox_);

    lines_list_ = new QListWidget(this);
    lines_list_->setSelectionMode(QAbstractItemView::NoSelection);
    lines_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lines_list_->setAlternatingRowColors(true);
    lines_list_->setWordWrap(false);
    lines_list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lines_list_->setFocusPolicy(Qt::NoFocus);

    root->addLayout(header_row);
    root->addWidget(lines_list_, 1);

    connect(auto_scroll_checkbox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (checked) {
            apply_scroll_position(true);
        }
    });
}

void PracticeSheetWindow::clear_song() {
    title_label_->setText("Sheet View");
    tracked_line_index_ = 0;
    lines_list_->clear();
}

void PracticeSheetWindow::set_song_lines(const std::string_view song_name,
                                         const std::vector<std::vector<std::string>>& lines) {
    title_label_->setText(
        QString("Sheet View: %1")
            .arg(QString::fromUtf8(song_name.data(), static_cast<int>(song_name.size()))));

    lines_list_->clear();
    for (const std::vector<std::string>& line : lines) {
        lines_list_->addItem(line_to_display(line));
    }

    tracked_line_index_ = 0;
    if (!lines.empty()) {
        apply_scroll_position(true);
    }
}

void PracticeSheetWindow::set_current_line(const std::size_t line_index) {
    if (lines_list_->count() == 0) {
        return;
    }
    if (line_index >= static_cast<std::size_t>(lines_list_->count())) {
        return;
    }

    tracked_line_index_ = line_index;
    apply_scroll_position(false);
}

void PracticeSheetWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    emit visibility_changed(true);
}

void PracticeSheetWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    emit visibility_changed(false);
}

void PracticeSheetWindow::apply_scroll_position(const bool force) {
    if (lines_list_->count() == 0) {
        return;
    }
    if (auto_scroll_checkbox_ == nullptr || !auto_scroll_checkbox_->isChecked()) {
        return;
    }

    const int max_row = lines_list_->count() - 1;
    const int tracked_row = std::clamp(static_cast<int>(tracked_line_index_), 0, max_row);
    const int desired_top_row = std::max(0, tracked_row - 5);
    const QModelIndex top_index = lines_list_->indexAt(QPoint(0, 0));
    const int visible_top_row = top_index.isValid() ? top_index.row() : 0;

    if (!force && visible_top_row == desired_top_row) {
        return;
    }

    if (QListWidgetItem* item = lines_list_->item(desired_top_row); item != nullptr) {
        lines_list_->scrollToItem(item, QAbstractItemView::PositionAtTop);
    }
}

} // namespace piano_assist

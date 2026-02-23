#include "piano_assist/floating_overlay_window.hpp"

#include <algorithm>
#include <limits>
#include <optional>

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

namespace piano_assist {
namespace {

constexpr int kOverlayWidthPx = 940;
constexpr int kOverlayHeightPx = 144;
constexpr int kOverlayBottomMarginPx = 72;
constexpr int kSheetOverlayWidthMinPx = 320;
constexpr int kSheetOverlayWidthMaxPx = 470;
constexpr double kSheetOverlayWidthRatio = 0.40;
constexpr int kSheetVisibleLines = 10;
constexpr int kSheetFocusRow = 4;
constexpr int kDetailBaseWidthPx = 220;
constexpr int kDetailMaxWidthPx = 320;
constexpr int kDetailTextPaddingPx = 92;
constexpr int kSheetTabMinWidthPx = 94;
constexpr int kSheetTabMarginPx = 1;

QString token_html(const QString& text, const QString& color, const int weight) {
    return QString("<span style='color:%1; font-weight:%2;'>%3</span>")
        .arg(color)
        .arg(weight)
        .arg(text);
}

int to_qt_int(const std::size_t value) {
    if (value > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
}

QString to_qstring(const std::string_view value) {
    return QString::fromUtf8(value.data(), to_qt_int(value.size()));
}

QString line_html(const QStringList& tokens) {
    if (tokens.isEmpty()) {
        return "<span style='font-family:\"Consolas\",\"Courier New\",monospace; font-size:20px; "
               "color:#6A6A6A;'>-</span>";
    }
    return QString("<span style='font-family:\"Consolas\",\"Courier New\",monospace; "
                   "font-size:20px;'>%1</span>")
        .arg(tokens.join("&nbsp;&nbsp;&nbsp;"));
}

QString sheet_line_html(const QString& text, const int row) {
    const QString background =
        row % 2 == 0 ? "rgba(255, 255, 255, 0.055)" : "rgba(255, 255, 255, 0.028)";
    return QString("<div style='background:%1; border-radius:6px; padding:4px 8px;'>"
                   "<span style='font-family:\"Consolas\",\"Courier New\",monospace; "
                   "font-size:16px; color:#E3E3E3; font-weight:500;'>%2</span></div>")
        .arg(background)
        .arg(text.toHtmlEscaped().replace(' ', "&nbsp;"));
}

QString detail_value_from_segment(const QString& segment, const QString& key) {
    if (!segment.startsWith(key, Qt::CaseInsensitive)) {
        return {};
    }
    return segment.mid(key.size()).trimmed();
}

QString detail_block_html(const QString& heading, const QString& value) {
    return QString("<table width='100%%' cellspacing='0' cellpadding='0'>"
                   "<tr>"
                   "<td align='left' width='48'>"
                   "<span style='color:#EAEAEA; font-size:11px; font-weight:700;'>%1</span>"
                   "</td>"
                   "<td align='center'>"
                   "<span style='color:#BFC5CE; font-size:11px; font-weight:500;'>%2</span>"
                   "</td>"
                   "</tr>"
                   "</table>")
        .arg(heading.toHtmlEscaped())
        .arg(value.toHtmlEscaped());
}

} // namespace

FloatingOverlayWindow::FloatingOverlayWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setWindowTitle("SheetMaster Overlay");

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    overlay_panel_ = new QFrame(this);
    overlay_panel_->setStyleSheet("QFrame {"
                                  " background-color: rgba(16, 16, 16, 215);"
                                  " border: 1px solid rgba(255, 255, 255, 72);"
                                  " border-top-left-radius: 12px;"
                                  " border-top-right-radius: 0px;"
                                  " border-bottom-left-radius: 12px;"
                                  " border-bottom-right-radius: 12px;"
                                  "}");

    auto* panel_layout = new QVBoxLayout(overlay_panel_);
    panel_layout->setContentsMargins(14, 10, 14, 10);
    panel_layout->setSpacing(4);

    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(8);

    left_details_label_ = new QLabel(overlay_panel_);
    left_details_label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    left_details_label_->setTextFormat(Qt::RichText);
    left_details_label_->setWordWrap(false);
    left_details_label_->setStyleSheet("QLabel {"
                                       " color: #BFC5CE;"
                                       " font-size: 11px;"
                                       " padding: 0 2px 0 4px;"
                                       "}");
    left_details_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    info_label_ = new QLabel(overlay_panel_);
    info_label_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    info_label_->setStyleSheet("QLabel {"
                               " color: #FDFDFD;"
                               " background-color: rgba(0, 0, 0, 110);"
                               " border-radius: 6px;"
                               " padding: 2px 10px;"
                               " font-size: 12px;"
                               " font-weight: 700;"
                               "}");
    info_label_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    right_details_label_ = new QLabel(overlay_panel_);
    right_details_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    right_details_label_->setTextFormat(Qt::RichText);
    right_details_label_->setWordWrap(false);
    right_details_label_->setStyleSheet("QLabel {"
                                        " color: #BFC5CE;"
                                        " font-size: 11px;"
                                        " padding: 0 2px;"
                                        "}");
    right_details_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    current_label_ = new QLabel(overlay_panel_);
    next_label_ = new QLabel(overlay_panel_);
    current_label_->setTextFormat(Qt::RichText);
    next_label_->setTextFormat(Qt::RichText);
    current_label_->setWordWrap(false);
    next_label_->setWordWrap(false);
    current_label_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    next_label_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    const auto apply_outline = [](QLabel* label) {
        auto* outline = new QGraphicsDropShadowEffect(label);
        outline->setBlurRadius(3.0);
        outline->setOffset(0.0, 0.0);
        outline->setColor(QColor(0, 0, 0, 220));
        label->setGraphicsEffect(outline);
    };
    apply_outline(left_details_label_);
    apply_outline(info_label_);
    apply_outline(right_details_label_);
    apply_outline(current_label_);
    apply_outline(next_label_);

    QFont heading_font = current_label_->font();
    heading_font.setBold(false);
    current_label_->setFont(heading_font);
    next_label_->setFont(heading_font);

    header_row->addWidget(left_details_label_, 1);
    header_row->addWidget(info_label_, 0, Qt::AlignHCenter);
    header_row->addWidget(right_details_label_, 1);
    panel_layout->addLayout(header_row);
    panel_layout->addWidget(current_label_);
    panel_layout->addWidget(next_label_);
    root_layout->addWidget(overlay_panel_);

    resize(kOverlayWidthPx, kOverlayHeightPx);

    if (const QScreen* screen = QGuiApplication::primaryScreen(); screen != nullptr) {
        const QRect geometry = screen->availableGeometry();
        const int x = geometry.left() + (geometry.width() - width()) / 2;
        const int y = geometry.top() + geometry.height() - height() - kOverlayBottomMarginPx;
        move(x, y);
    }

    ensure_sheet_tab_window();
    update_sheet_toggle_button();

    set_song_progress({}, std::nullopt, {}, false, false, {}, {}, 0, 0);
    set_sheet_view_data({}, {}, std::nullopt);
}

void FloatingOverlayWindow::set_song_progress(
    const std::vector<std::string>& current_line,
    const std::optional<std::size_t> highlighted_key_index,
    const std::vector<std::string>& next_line, const bool completed, const bool paused,
    const std::string_view song_name, const std::string_view details_text,
    const std::size_t progress_current, const std::size_t progress_total) {
    const QString paused_suffix = paused ? "   [PAUSED]" : "";
    if (!song_name.empty()) {
        info_label_->setText(QString("Song: %1   Progress: %2/%3%4")
                                 .arg(to_qstring(song_name))
                                 .arg(to_qt_int(progress_current))
                                 .arg(to_qt_int(progress_total))
                                 .arg(paused_suffix));
    } else {
        info_label_->setText(QString("Song: -   Progress: 0/0%1").arg(paused_suffix));
    }

    if (left_details_label_ != nullptr && right_details_label_ != nullptr) {
        const QString details = to_qstring(details_text).trimmed();
        QString tags_text;
        QString bpm_text;
        bool has_tags_block = false;
        bool has_bpm_block = false;
        if (!details.isEmpty()) {
            QStringList segments = details.split('|', Qt::SkipEmptyParts);
            for (QString& segment : segments) {
                segment = segment.trimmed();
                const QString parsed_tags = detail_value_from_segment(segment, "Tags:");
                if (!parsed_tags.isEmpty() || segment.startsWith("Tags:", Qt::CaseInsensitive)) {
                    tags_text = parsed_tags;
                    has_tags_block = true;
                    continue;
                }

                const QString parsed_bpm = detail_value_from_segment(segment, "BPM:");
                if (!parsed_bpm.isEmpty() || segment.startsWith("BPM:", Qt::CaseInsensitive)) {
                    bpm_text = parsed_bpm;
                    has_bpm_block = true;
                    continue;
                }

                if (!has_tags_block) {
                    tags_text = segment;
                    has_tags_block = true;
                } else if (!has_bpm_block) {
                    bpm_text = segment;
                    has_bpm_block = true;
                }
            }
        }

        if (has_tags_block && tags_text.isEmpty()) {
            tags_text = "No tags";
        }
        if (has_bpm_block && bpm_text.isEmpty()) {
            bpm_text = "BPM data not found!";
        }

        const QFontMetrics metrics(left_details_label_->font());
        const auto block_width_for_value = [&metrics](const QString& heading,
                                                      const QString& value) -> int {
            if (value.trimmed().isEmpty()) {
                return kDetailBaseWidthPx;
            }
            return metrics.horizontalAdvance(heading + " " + value) + kDetailTextPaddingPx;
        };
        const int left_required = block_width_for_value("Tags:", tags_text);
        const int right_required = block_width_for_value("BPM:", bpm_text);
        const int detail_width = std::clamp(std::max(left_required, right_required),
                                            kDetailBaseWidthPx, kDetailMaxWidthPx);
        left_details_label_->setFixedWidth(detail_width);
        right_details_label_->setFixedWidth(detail_width);

        const int value_width = std::max(72, detail_width - kDetailTextPaddingPx);
        if (!has_tags_block) {
            left_details_label_->clear();
        } else {
            const QString rendered_tags =
                metrics.elidedText(tags_text, Qt::ElideRight, value_width);
            left_details_label_->setText(detail_block_html("Tags:", rendered_tags));
        }

        if (!has_bpm_block) {
            right_details_label_->clear();
        } else {
            const QString rendered_bpm = metrics.elidedText(bpm_text, Qt::ElideRight, value_width);
            right_details_label_->setText(detail_block_html("BPM:", rendered_bpm));
        }

        if (!has_tags_block && !has_bpm_block) {
            left_details_label_->setFixedWidth(kDetailBaseWidthPx);
            right_details_label_->setFixedWidth(kDetailBaseWidthPx);
            left_details_label_->clear();
            right_details_label_->clear();
        }
    }

    if (completed) {
        current_label_->setText(
            "<span style='font-size:24px; font-weight:700; color:#FFD54A;'>completed!</span>");
        next_label_->setText(
            "<span style='font-size:16px; color:#BFBFBF;'>tab to restart or select a new "
            "song.</span>");
        return;
    }

    QStringList top_tokens;
    top_tokens.reserve(static_cast<qsizetype>(current_line.size()));
    for (std::size_t index = 0; index < current_line.size(); ++index) {
        const bool highlighted =
            highlighted_key_index.has_value() && index == *highlighted_key_index;
        const QString color = highlighted ? "#FFD54A" : "#EAEAEA";
        const int weight = highlighted ? 700 : 500;
        top_tokens.push_back(
            token_html(QString::fromStdString(current_line[index]).toHtmlEscaped(), color, weight));
    }

    QStringList bottom_tokens;
    bottom_tokens.reserve(static_cast<qsizetype>(next_line.size()));
    for (const std::string& key : next_line) {
        bottom_tokens.push_back(
            token_html(QString::fromStdString(key).toHtmlEscaped(), "#8B8B8B", 500));
    }

    current_label_->setText(line_html(top_tokens));
    next_label_->setText(line_html(bottom_tokens));
}

void FloatingOverlayWindow::set_sheet_tab_visible(const bool visible) {
    if (sheet_tab_visible_ == visible) {
        if (sheet_tab_visible_ && isVisible()) {
            ensure_sheet_tab_window();
            reposition_sheet_tab();
            if (sheet_tab_window_ != nullptr) {
                sheet_tab_window_->show();
                sheet_tab_window_->raise();
            }
        }
        return;
    }

    sheet_tab_visible_ = visible;
    if (!sheet_tab_visible_) {
        if (sheet_view_visible_) {
            set_sheet_view_visible(false);
            emit sheet_view_toggled(false);
        }
        if (sheet_tab_window_ != nullptr) {
            sheet_tab_window_->hide();
        }
        return;
    }

    ensure_sheet_tab_window();
    update_sheet_toggle_button();
    if (sheet_tab_window_ != nullptr && isVisible()) {
        reposition_sheet_tab();
        sheet_tab_window_->show();
        sheet_tab_window_->raise();
    }
}

void FloatingOverlayWindow::set_sheet_view_data(
    const std::string_view song_name, const std::vector<std::vector<std::string>>& sheet_lines,
    const std::optional<std::size_t> focused_line_index) {
    current_sheet_song_name_ = std::string(song_name);
    sheet_lines_ = sheet_lines;
    focused_sheet_line_ = focused_line_index;
    refresh_sheet_lines();
}

void FloatingOverlayWindow::set_sheet_view_visible(const bool visible) {
    if (visible && !sheet_tab_visible_) {
        return;
    }

    if (sheet_view_visible_ == visible) {
        if (sheet_view_visible_ && sheet_overlay_window_ != nullptr && isVisible()) {
            refresh_sheet_lines();
            reposition_sheet_overlay();
            sheet_overlay_window_->show();
            sheet_overlay_window_->raise();
        }
        reposition_sheet_tab();
        return;
    }

    sheet_view_visible_ = visible;
    update_sheet_toggle_button();
    ensure_sheet_overlay_window();

    if (sheet_overlay_window_ == nullptr) {
        return;
    }

    if (sheet_view_visible_ && isVisible()) {
        refresh_sheet_lines();
        reposition_sheet_overlay();
        sheet_overlay_window_->show();
        sheet_overlay_window_->raise();
    } else {
        sheet_overlay_window_->hide();
    }
    reposition_sheet_tab();
}

void FloatingOverlayWindow::ensure_sheet_tab_window() {
    if (sheet_tab_window_ != nullptr) {
        return;
    }

    auto window = std::make_unique<QWidget>(nullptr);
    window->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setAttribute(Qt::WA_ShowWithoutActivating);
    window->setFocusPolicy(Qt::NoFocus);
    window->setWindowTitle("SheetMaster Sheet Tab");

    auto* root = new QVBoxLayout(window.get());
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* tab = new QPushButton(window.get());
    tab->setCursor(Qt::PointingHandCursor);
    tab->setFocusPolicy(Qt::NoFocus);
    tab->setStyleSheet("QPushButton {"
                       " color: #DADADA;"
                       " background-color: rgba(16, 16, 16, 215);"
                       " border: 1px solid rgba(255, 255, 255, 72);"
                       " border-top-left-radius: 8px;"
                       " border-top-right-radius: 8px;"
                       " border-bottom-left-radius: 4px;"
                       " border-bottom-right-radius: 0px;"
                       " border-bottom-color: rgba(16, 16, 16, 215);"
                       " padding: 2px 10px 3px 10px;"
                       " font-size: 10px;"
                       " font-weight: 700;"
                       "}"
                       "QPushButton:hover {"
                       " color: #FFD54A;"
                       " background-color: rgba(16, 16, 16, 230);"
                       " border: 1px solid rgba(255, 213, 74, 180);"
                       " border-bottom-color: rgba(16, 16, 16, 230);"
                       "}"
                       "QPushButton:pressed {"
                       " color: #FFD54A;"
                       " background-color: rgba(16, 16, 16, 245);"
                       " border: 1px solid rgba(255, 213, 74, 220);"
                       " border-bottom-color: rgba(16, 16, 16, 245);"
                       "}");
    root->addWidget(tab);

    connect(tab, &QPushButton::clicked, this, [this]() {
        set_sheet_view_visible(!sheet_view_visible_);
        emit sheet_view_toggled(sheet_view_visible_);
    });

    sheet_tab_button_ = tab;
    sheet_tab_window_ = std::move(window);
    update_sheet_toggle_button();

    if (!sheet_tab_visible_ || !isVisible()) {
        sheet_tab_window_->hide();
    }
}

void FloatingOverlayWindow::ensure_sheet_overlay_window() {
    if (sheet_overlay_window_ != nullptr) {
        return;
    }

    auto window = std::make_unique<QWidget>(nullptr);
    window->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setAttribute(Qt::WA_ShowWithoutActivating);
    window->setFocusPolicy(Qt::NoFocus);
    window->setWindowTitle("SheetMaster Sheet View");

    auto* root = new QVBoxLayout(window.get());
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* panel = new QFrame(window.get());
    panel->setStyleSheet("QFrame {"
                         " background-color: rgba(16, 16, 16, 215);"
                         " border: 1px solid rgba(255, 255, 255, 72);"
                         " border-top-left-radius: 12px;"
                         " border-top-right-radius: 0px;"
                         " border-bottom-left-radius: 12px;"
                         " border-bottom-right-radius: 12px;"
                         "}");

    auto* panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(12, 10, 12, 10);
    panel_layout->setSpacing(4);

    sheet_title_label_ = new QLabel("Sheet View: -", panel);
    sheet_title_label_->setStyleSheet("QLabel {"
                                      " color: #FDFDFD;"
                                      " font-size: 13px;"
                                      " font-weight: 700;"
                                      " padding: 0 2px;"
                                      "}");
    panel_layout->addWidget(sheet_title_label_);

    sheet_line_labels_.clear();
    sheet_line_labels_.reserve(static_cast<std::size_t>(kSheetVisibleLines));
    for (int row = 0; row < kSheetVisibleLines; ++row) {
        auto* line = new QLabel(panel);
        line->setTextFormat(Qt::RichText);
        line->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        line->setMinimumHeight(30);
        line->setText(sheet_line_html("-", row));
        panel_layout->addWidget(line);
        sheet_line_labels_.push_back(line);
    }

    root->addWidget(panel);
    const int preferred_width = std::clamp(static_cast<int>(width() * kSheetOverlayWidthRatio),
                                           kSheetOverlayWidthMinPx, kSheetOverlayWidthMaxPx);
    window->resize(preferred_width, panel->sizeHint().height());

    sheet_overlay_window_ = std::move(window);
    refresh_sheet_lines();
    if (!sheet_view_visible_ || !isVisible()) {
        sheet_overlay_window_->hide();
    }
}

void FloatingOverlayWindow::reposition_sheet_overlay() {
    if (!sheet_view_visible_ || sheet_overlay_window_ == nullptr) {
        return;
    }

    QScreen* target_screen = screen();
    if (target_screen == nullptr) {
        target_screen = QGuiApplication::screenAt(frameGeometry().center());
    }
    if (target_screen == nullptr) {
        target_screen = QGuiApplication::primaryScreen();
    }
    if (target_screen == nullptr) {
        return;
    }

    const QRect available = target_screen->availableGeometry();
    QWidget* sheet = sheet_overlay_window_.get();
    const QRect overlay_rect = frameGeometry();

    const int preferred_width = std::clamp(static_cast<int>(width() * kSheetOverlayWidthRatio),
                                           kSheetOverlayWidthMinPx, kSheetOverlayWidthMaxPx);
    if (sheet->width() != preferred_width) {
        sheet->resize(preferred_width, sheet->height());
    }

    int x = overlay_rect.right() - sheet->width() + 1;
    int y = overlay_rect.top() - sheet->height();

    x = std::clamp(x, available.left(), available.right() - sheet->width() + 1);
    y = std::clamp(y, available.top(), available.bottom() - sheet->height() + 1);

    sheet->move(x, y);
}

void FloatingOverlayWindow::reposition_sheet_tab() {
    if (!sheet_tab_visible_ || !isVisible()) {
        if (sheet_tab_window_ != nullptr) {
            sheet_tab_window_->hide();
        }
        return;
    }

    ensure_sheet_tab_window();
    if (sheet_tab_window_ == nullptr || sheet_tab_button_ == nullptr) {
        return;
    }

    QWidget* anchor = this;
    if (sheet_view_visible_ && sheet_overlay_window_ != nullptr &&
        sheet_overlay_window_->isVisible()) {
        anchor = sheet_overlay_window_.get();
    }

    QScreen* target_screen = anchor->screen();
    if (target_screen == nullptr) {
        target_screen = QGuiApplication::screenAt(anchor->frameGeometry().center());
    }
    if (target_screen == nullptr) {
        target_screen = QGuiApplication::primaryScreen();
    }
    if (target_screen == nullptr) {
        return;
    }

    const QRect available = target_screen->availableGeometry();
    const QRect anchor_rect = anchor->frameGeometry();
    const int tab_width = std::max(kSheetTabMinWidthPx, sheet_tab_button_->sizeHint().width() + 8);
    const int tab_height = sheet_tab_button_->sizeHint().height() + 2;

    if (sheet_tab_window_->size() != QSize(tab_width, tab_height)) {
        sheet_tab_window_->resize(tab_width, tab_height);
    }

    int x = anchor_rect.right() - tab_width + 1;
    int y = anchor_rect.top() - tab_height + kSheetTabMarginPx;
    x = std::clamp(x, available.left(), available.right() - tab_width + 1);
    y = std::clamp(y, available.top(), available.bottom() - tab_height + 1);
    sheet_tab_window_->move(x, y);
    sheet_tab_window_->show();
    sheet_tab_window_->raise();
}

void FloatingOverlayWindow::update_sheet_toggle_button() {
    if (sheet_tab_button_ == nullptr) {
        return;
    }
    sheet_tab_button_->setText(sheet_view_visible_ ? "Sheet View v" : "Sheet View >");
}

void FloatingOverlayWindow::refresh_sheet_lines() {
    if (sheet_title_label_ == nullptr || sheet_line_labels_.empty()) {
        return;
    }

    if (current_sheet_song_name_.empty()) {
        sheet_title_label_->setText("Sheet View: -");
    } else {
        sheet_title_label_->setText(
            QString("Sheet View: %1").arg(to_qstring(current_sheet_song_name_)));
    }

    const std::size_t line_count = sheet_lines_.size();
    std::size_t focused = 0;
    if (focused_sheet_line_.has_value() && line_count > 0) {
        focused = std::min(*focused_sheet_line_, line_count - 1);
    }

    std::size_t start_line = 0;
    if (focused_sheet_line_.has_value() && focused >= static_cast<std::size_t>(kSheetFocusRow)) {
        start_line = focused - static_cast<std::size_t>(kSheetFocusRow);
    }

    for (int row = 0; row < kSheetVisibleLines; ++row) {
        QLabel* label = sheet_line_labels_[static_cast<std::size_t>(row)];
        if (label == nullptr) {
            continue;
        }

        const std::size_t line_index = start_line + static_cast<std::size_t>(row);
        if (line_index >= line_count) {
            label->setText(sheet_line_html("-", row));
            continue;
        }

        QStringList tokens;
        for (const std::string& token : sheet_lines_[line_index]) {
            tokens.push_back(QString::fromStdString(token));
        }
        const QString text = tokens.isEmpty() ? "-" : tokens.join("   ");
        label->setText(sheet_line_html(text, row));
    }
}

void FloatingOverlayWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingOverlayWindow::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_ && (event->buttons() & Qt::LeftButton) != 0) {
        move(event->globalPosition().toPoint() - drag_offset_);
        reposition_sheet_overlay();
        reposition_sheet_tab();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingOverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingOverlayWindow::moveEvent(QMoveEvent* event) {
    QWidget::moveEvent(event);
    reposition_sheet_overlay();
    reposition_sheet_tab();
}

void FloatingOverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (sheet_tab_visible_) {
        ensure_sheet_tab_window();
    }
    if (sheet_view_visible_) {
        ensure_sheet_overlay_window();
        if (sheet_overlay_window_ != nullptr) {
            refresh_sheet_lines();
            reposition_sheet_overlay();
            sheet_overlay_window_->show();
            sheet_overlay_window_->raise();
        }
    }
    reposition_sheet_tab();
}

void FloatingOverlayWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (sheet_overlay_window_ != nullptr) {
        sheet_overlay_window_->hide();
    }
    if (sheet_tab_window_ != nullptr) {
        sheet_tab_window_->hide();
    }
}

void FloatingOverlayWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    reposition_sheet_overlay();
    reposition_sheet_tab();
}

} // namespace piano_assist

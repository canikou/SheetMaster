#include "piano_assist/floating_overlay_window.hpp"

#include <limits>
#include <optional>

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QStringList>
#include <QVBoxLayout>

namespace piano_assist {
namespace {

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
        return "<span style='font-family:\"Consolas\",\"Courier New\",monospace; font-size:20px; color:#6A6A6A;'>-</span>";
    }
    return QString("<span style='font-family:\"Consolas\",\"Courier New\",monospace; font-size:20px;'>%1</span>")
        .arg(tokens.join("&nbsp;&nbsp;&nbsp;"));
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

    auto* panel = new QFrame(this);
    panel->setStyleSheet(
        "QFrame {"
        " background-color: rgba(16, 16, 16, 215);"
        " border: 1px solid rgba(255, 255, 255, 72);"
        " border-radius: 12px;"
        "}"
    );

    auto* panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(14, 10, 14, 10);
    panel_layout->setSpacing(4);

    info_label_ = new QLabel(panel);
    info_label_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    info_label_->setStyleSheet(
        "QLabel {"
        " color: #FDFDFD;"
        " background-color: rgba(0, 0, 0, 110);"
        " border-radius: 6px;"
        " padding: 2px 10px;"
        " font-size: 12px;"
        " font-weight: 700;"
        "}"
    );

    current_label_ = new QLabel(panel);
    next_label_ = new QLabel(panel);
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
    apply_outline(info_label_);
    apply_outline(current_label_);
    apply_outline(next_label_);

    QFont heading_font = current_label_->font();
    heading_font.setBold(false);
    current_label_->setFont(heading_font);
    next_label_->setFont(heading_font);

    panel_layout->addWidget(info_label_, 0, Qt::AlignHCenter);
    panel_layout->addWidget(current_label_);
    panel_layout->addWidget(next_label_);
    root_layout->addWidget(panel);

    resize(940, 144);

    if (const QScreen* screen = QGuiApplication::primaryScreen(); screen != nullptr) {
        const QRect geometry = screen->availableGeometry();
        const int x = geometry.left() + (geometry.width() - width()) / 2;
        const int y = geometry.top() + geometry.height() - height() - 72;
        move(x, y);
    }

    set_song_progress({}, std::nullopt, {}, false, false, {}, 0, 0);
}

void FloatingOverlayWindow::set_song_progress(
    const std::vector<std::string>& current_line,
    const std::optional<std::size_t> highlighted_key_index,
    const std::vector<std::string>& next_line,
    const bool completed,
    const bool paused,
    const std::string_view song_name,
    const std::size_t progress_current,
    const std::size_t progress_total
) {
    const QString paused_suffix = paused ? "   [PAUSED]" : "";
    if (!song_name.empty()) {
        info_label_->setText(
            QString("Song: %1   Progress: %2/%3%4")
                .arg(to_qstring(song_name))
                .arg(to_qt_int(progress_current))
                .arg(to_qt_int(progress_total))
                .arg(paused_suffix)
        );
    } else {
        info_label_->setText(QString("Song: -   Progress: 0/0%1").arg(paused_suffix));
    }

    if (completed) {
        current_label_->setText("<span style='font-size:24px; font-weight:700; color:#FFD54A;'>completed!</span>");
        next_label_->setText("<span style='font-size:18px; color:#808080;'>-</span>");
        return;
    }

    QStringList top_tokens;
    top_tokens.reserve(static_cast<qsizetype>(current_line.size()));
    for (std::size_t index = 0; index < current_line.size(); ++index) {
        const bool highlighted = highlighted_key_index.has_value() && index == *highlighted_key_index;
        const QString color = highlighted ? "#FFD54A" : "#EAEAEA";
        const int weight = highlighted ? 700 : 500;
        top_tokens.push_back(token_html(
            QString::fromStdString(current_line[index]).toHtmlEscaped(),
            color,
            weight
        ));
    }

    QStringList bottom_tokens;
    bottom_tokens.reserve(static_cast<qsizetype>(next_line.size()));
    for (const std::string& key : next_line) {
        bottom_tokens.push_back(token_html(QString::fromStdString(key).toHtmlEscaped(), "#8B8B8B", 500));
    }

    current_label_->setText(line_html(top_tokens));
    next_label_->setText(line_html(bottom_tokens));
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

} // namespace piano_assist

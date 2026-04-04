#include "terminalview.hpp"
#include "pty.hpp"
#include "terminalstate.hpp"

#include <QPainter>
#include <QKeyEvent>
#include <QWheelEvent>

TerminalView::TerminalView(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setFlag(ItemAcceptsInputMethod);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFillColor(QColor(30, 30, 30));

    m_refreshTimer.setInterval(16); // ~60fps
    connect(&m_refreshTimer, &QTimer::timeout, this, &TerminalView::onRefresh);
}

TerminalView::~TerminalView() {
    if (m_rowCells) ghostty_render_state_row_cells_free(m_rowCells);
    if (m_rowIterator) ghostty_render_state_row_iterator_free(m_rowIterator);
}

void TerminalView::componentComplete() {
    QQuickPaintedItem::componentComplete();
    startTerminal();
}

void TerminalView::setShell(const QString& shell) {
    if (m_shell == shell) return;
    m_shell = shell;
    emit shellChanged();
}

void TerminalView::setFontFamily(const QString& family) {
    if (m_fontFamily == family) return;
    m_fontFamily = family;
    emit fontFamilyChanged();
    if (m_started) recalculateGrid();
}

void TerminalView::setFontSize(qreal size) {
    if (qFuzzyCompare(m_fontSize, size)) return;
    m_fontSize = size;
    emit fontSizeChanged();
    if (m_started) recalculateGrid();
}

void TerminalView::recalculateGrid() {
    m_font = QFont(m_fontFamily);
    m_font.setPointSizeF(m_fontSize);
    m_font.setStyleHint(QFont::Monospace);
    m_fontMetrics = std::make_unique<QFontMetricsF>(m_font);
    m_cellWidth = m_fontMetrics->horizontalAdvance(QChar('M'));
    m_cellHeight = m_fontMetrics->height();

    if (m_cellWidth <= 0 || m_cellHeight <= 0) return;

    uint16_t newCols = static_cast<uint16_t>(width() / m_cellWidth);
    uint16_t newRows = static_cast<uint16_t>(height() / m_cellHeight);
    if (newCols < 1) newCols = 1;
    if (newRows < 1) newRows = 1;

    if (newCols != m_cols || newRows != m_rows) {
        m_cols = newCols;
        m_rows = newRows;
        auto cw = static_cast<uint16_t>(m_cellWidth);
        auto ch = static_cast<uint16_t>(m_cellHeight);
        if (m_state)
            m_state->resize(m_cols, m_rows, cw, ch);
        if (m_pty && m_pty->isRunning())
            m_pty->resize(m_cols, m_rows, cw, ch);
    }
}

void TerminalView::startTerminal() {
    if (m_started) return;
    m_started = true;

    recalculateGrid();
    if (m_cols == 0 || m_rows == 0) return;

    m_state = std::make_unique<TerminalState>(this);
    if (!m_state->init(m_cols, m_rows)) return;

    connect(m_state.get(), &TerminalState::titleChanged, this, [this](const QString& t) {
        m_title = t;
        emit titleChanged();
    });

    m_pty = std::make_unique<Pty>(this);
    m_state->setPty(m_pty.get());
    connect(m_pty.get(), &Pty::dataReady, this, &TerminalView::onPtyData);

    // Pre-allocate reusable iterators
    ghostty_render_state_row_iterator_new(nullptr, &m_rowIterator);
    ghostty_render_state_row_cells_new(nullptr, &m_rowCells);

    std::string shell = m_shell.isEmpty() ? "" : m_shell.toStdString();
    if (!m_pty->spawn(shell, m_cols, m_rows,
                      static_cast<uint16_t>(m_cellWidth),
                      static_cast<uint16_t>(m_cellHeight))) {
        return;
    }

    m_refreshTimer.start();
}

void TerminalView::geometryChange(const QRectF& newGeo, const QRectF& oldGeo) {
    QQuickPaintedItem::geometryChange(newGeo, oldGeo);
    if (m_started) recalculateGrid();
}

void TerminalView::onPtyData(const QByteArray& data) {
    if (m_state)
        m_state->feedData(data.constData(), data.size());
}

void TerminalView::onRefresh() {
    if (!m_state) return;
    auto dirty = m_state->updateRenderState();
    if (dirty != GHOSTTY_RENDER_STATE_DIRTY_FALSE)
        update(); // triggers paint()
}

QColor TerminalView::ghosttyToQColor(GhosttyColorRgb rgb) {
    return QColor(rgb.r, rgb.g, rgb.b);
}

void TerminalView::paint(QPainter* painter) {
    if (!m_state || !m_state->renderState()) return;

    auto rs = m_state->renderState();

    // Get colors
    GhosttyRenderStateColors colors{};
    colors.size = sizeof(GhosttyRenderStateColors);
    ghostty_render_state_colors_get(rs, &colors);

    // Fill background
    painter->fillRect(QRectF(0, 0, width(), height()), ghosttyToQColor(colors.background));

    // Get row iterator
    ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_ROW_ITERATOR, &m_rowIterator);

    int y = 0;
    while (ghostty_render_state_row_iterator_next(m_rowIterator)) {
        ghostty_render_state_row_get(m_rowIterator,
                                     GHOSTTY_RENDER_STATE_ROW_DATA_CELLS,
                                     &m_rowCells);

        int x = 0;
        while (ghostty_render_state_row_cells_next(m_rowCells)) {
            paintCell(painter, x, y, m_rowCells, colors);
            x++;
        }
        y++;
    }

    // Draw cursor
    bool cursorVisible = false;
    ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISIBLE, &cursorVisible);
    bool cursorInViewport = false;
    ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_HAS_VALUE, &cursorInViewport);

    if (cursorVisible && cursorInViewport) {
        uint16_t cx = 0, cy = 0;
        ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_X, &cx);
        ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_CURSOR_VIEWPORT_Y, &cy);

        QRectF cursorRect(cx * m_cellWidth, cy * m_cellHeight, m_cellWidth, m_cellHeight);

        GhosttyRenderStateCursorVisualStyle style = GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK;
        ghostty_render_state_get(rs, GHOSTTY_RENDER_STATE_DATA_CURSOR_VISUAL_STYLE, &style);

        QColor cursorColor = ghosttyToQColor(colors.foreground);
        if (colors.cursor_has_value)
            cursorColor = ghosttyToQColor(colors.cursor);

        switch (style) {
        case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK:
            painter->fillRect(cursorRect, cursorColor);
            break;
        case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BAR:
            painter->fillRect(QRectF(cursorRect.x(), cursorRect.y(), 2, cursorRect.height()), cursorColor);
            break;
        case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_UNDERLINE:
            painter->fillRect(QRectF(cursorRect.x(), cursorRect.bottom() - 2, cursorRect.width(), 2), cursorColor);
            break;
        case GHOSTTY_RENDER_STATE_CURSOR_VISUAL_STYLE_BLOCK_HOLLOW:
            painter->setPen(cursorColor);
            painter->drawRect(cursorRect.adjusted(0.5, 0.5, -0.5, -0.5));
            break;
        }
    }

    // Reset dirty state
    GhosttyRenderStateDirty clean = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_set(rs, GHOSTTY_RENDER_STATE_OPTION_DIRTY, &clean);
}

void TerminalView::paintCell(QPainter* painter, int x, int y,
                             GhosttyRenderStateRowCells cells,
                             const GhosttyRenderStateColors& colors) {
    QRectF cellRect(x * m_cellWidth, y * m_cellHeight, m_cellWidth, m_cellHeight);

    // Background
    GhosttyColorRgb bgColor{};
    if (ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_BG_COLOR, &bgColor)
        == GHOSTTY_SUCCESS) {
        painter->fillRect(cellRect, ghosttyToQColor(bgColor));
    }

    // Get grapheme length
    uint32_t graphemeLen = 0;
    ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_LEN, &graphemeLen);
    if (graphemeLen == 0) return;

    // Read codepoints
    QVarLengthArray<uint32_t, 4> codepoints(graphemeLen);
    ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_GRAPHEMES_BUF, codepoints.data());

    QString text = QString::fromUcs4(codepoints.data(), graphemeLen);

    // Get style
    GhosttyStyle style{};
    style.size = sizeof(GhosttyStyle);
    ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_STYLE, &style);

    // Foreground color
    GhosttyColorRgb fgColor{};
    QColor fg = ghosttyToQColor(colors.foreground);
    if (ghostty_render_state_row_cells_get(cells, GHOSTTY_RENDER_STATE_ROW_CELLS_DATA_FG_COLOR, &fgColor)
        == GHOSTTY_SUCCESS) {
        fg = ghosttyToQColor(fgColor);
    }

    // Apply style to font
    QFont cellFont = m_font;
    if (style.bold) cellFont.setBold(true);
    if (style.italic) cellFont.setItalic(true);
    if (style.faint) fg.setAlphaF(0.5);

    painter->setFont(cellFont);
    painter->setPen(fg);
    painter->drawText(cellRect, Qt::AlignLeft | Qt::AlignTop, text);

    // Underline
    if (style.underline > 0) {
        qreal uy = cellRect.bottom() - 1;
        painter->drawLine(QPointF(cellRect.left(), uy), QPointF(cellRect.right(), uy));
    }

    // Strikethrough
    if (style.strikethrough) {
        qreal sy = cellRect.center().y();
        painter->drawLine(QPointF(cellRect.left(), sy), QPointF(cellRect.right(), sy));
    }
}

// --- Input handling ---

void TerminalView::keyPressEvent(QKeyEvent* event) {
    if (!m_state || !m_pty) return;

    // Sync encoder with terminal modes
    ghostty_key_encoder_setopt_from_terminal(m_state->keyEncoder(), m_state->terminal());

    GhosttyKeyEvent keyEvent;
    ghostty_key_event_new(nullptr, &keyEvent);
    ghostty_key_event_set_action(keyEvent, GHOSTTY_KEY_ACTION_PRESS);

    GhosttyKey gkey = qtKeyToGhosttyKey(event->key());
    ghostty_key_event_set_key(keyEvent, gkey);
    ghostty_key_event_set_mods(keyEvent, qtModsToGhosttyMods(event->modifiers()));

    // Set UTF-8 text for printable characters
    QString text = event->text();
    if (!text.isEmpty()) {
        QByteArray utf8 = text.toUtf8();
        // Don't pass control characters as text — let the encoder handle them
        if (utf8.size() > 0 && static_cast<unsigned char>(utf8[0]) >= 0x20) {
            ghostty_key_event_set_utf8(keyEvent, utf8.constData(), utf8.size());
        }
    }

    char buf[128];
    size_t written = 0;
    GhosttyResult result = ghostty_key_encoder_encode(
        m_state->keyEncoder(), keyEvent, buf, sizeof(buf), &written);

    if (result == GHOSTTY_SUCCESS && written > 0) {
        m_pty->write(buf, written);
    } else if (result == GHOSTTY_OUT_OF_SPACE) {
        // Shouldn't happen with 128 bytes but handle gracefully
        std::vector<char> bigBuf(written);
        ghostty_key_encoder_encode(
            m_state->keyEncoder(), keyEvent, bigBuf.data(), bigBuf.size(), &written);
        m_pty->write(bigBuf.data(), written);
    }

    ghostty_key_event_free(keyEvent);
    event->accept();
}

void TerminalView::keyReleaseEvent(QKeyEvent* event) {
    event->accept();
}

void TerminalView::wheelEvent(QWheelEvent* event) {
    if (!m_state) return;
    int delta = event->angleDelta().y() > 0 ? -3 : 3;
    m_state->scrollViewport(delta);
    event->accept();
}

GhosttyMods TerminalView::qtModsToGhosttyMods(Qt::KeyboardModifiers mods) {
    GhosttyMods gmods = 0;
    if (mods & Qt::ShiftModifier) gmods |= GHOSTTY_MODS_SHIFT;
    if (mods & Qt::ControlModifier) gmods |= GHOSTTY_MODS_CTRL;
    if (mods & Qt::AltModifier) gmods |= GHOSTTY_MODS_ALT;
    if (mods & Qt::MetaModifier) gmods |= GHOSTTY_MODS_SUPER;
    return gmods;
}

GhosttyKey TerminalView::qtKeyToGhosttyKey(int qtKey) {
    switch (qtKey) {
    // Letters
    case Qt::Key_A: return GHOSTTY_KEY_A;
    case Qt::Key_B: return GHOSTTY_KEY_B;
    case Qt::Key_C: return GHOSTTY_KEY_C;
    case Qt::Key_D: return GHOSTTY_KEY_D;
    case Qt::Key_E: return GHOSTTY_KEY_E;
    case Qt::Key_F: return GHOSTTY_KEY_F;
    case Qt::Key_G: return GHOSTTY_KEY_G;
    case Qt::Key_H: return GHOSTTY_KEY_H;
    case Qt::Key_I: return GHOSTTY_KEY_I;
    case Qt::Key_J: return GHOSTTY_KEY_J;
    case Qt::Key_K: return GHOSTTY_KEY_K;
    case Qt::Key_L: return GHOSTTY_KEY_L;
    case Qt::Key_M: return GHOSTTY_KEY_M;
    case Qt::Key_N: return GHOSTTY_KEY_N;
    case Qt::Key_O: return GHOSTTY_KEY_O;
    case Qt::Key_P: return GHOSTTY_KEY_P;
    case Qt::Key_Q: return GHOSTTY_KEY_Q;
    case Qt::Key_R: return GHOSTTY_KEY_R;
    case Qt::Key_S: return GHOSTTY_KEY_S;
    case Qt::Key_T: return GHOSTTY_KEY_T;
    case Qt::Key_U: return GHOSTTY_KEY_U;
    case Qt::Key_V: return GHOSTTY_KEY_V;
    case Qt::Key_W: return GHOSTTY_KEY_W;
    case Qt::Key_X: return GHOSTTY_KEY_X;
    case Qt::Key_Y: return GHOSTTY_KEY_Y;
    case Qt::Key_Z: return GHOSTTY_KEY_Z;

    // Digits
    case Qt::Key_0: return GHOSTTY_KEY_DIGIT_0;
    case Qt::Key_1: return GHOSTTY_KEY_DIGIT_1;
    case Qt::Key_2: return GHOSTTY_KEY_DIGIT_2;
    case Qt::Key_3: return GHOSTTY_KEY_DIGIT_3;
    case Qt::Key_4: return GHOSTTY_KEY_DIGIT_4;
    case Qt::Key_5: return GHOSTTY_KEY_DIGIT_5;
    case Qt::Key_6: return GHOSTTY_KEY_DIGIT_6;
    case Qt::Key_7: return GHOSTTY_KEY_DIGIT_7;
    case Qt::Key_8: return GHOSTTY_KEY_DIGIT_8;
    case Qt::Key_9: return GHOSTTY_KEY_DIGIT_9;

    // Punctuation
    case Qt::Key_Minus: return GHOSTTY_KEY_MINUS;
    case Qt::Key_Equal: return GHOSTTY_KEY_EQUAL;
    case Qt::Key_BracketLeft: return GHOSTTY_KEY_BRACKET_LEFT;
    case Qt::Key_BracketRight: return GHOSTTY_KEY_BRACKET_RIGHT;
    case Qt::Key_Backslash: return GHOSTTY_KEY_BACKSLASH;
    case Qt::Key_Semicolon: return GHOSTTY_KEY_SEMICOLON;
    case Qt::Key_Apostrophe: return GHOSTTY_KEY_QUOTE;
    case Qt::Key_QuoteLeft: return GHOSTTY_KEY_BACKQUOTE;
    case Qt::Key_Comma: return GHOSTTY_KEY_COMMA;
    case Qt::Key_Period: return GHOSTTY_KEY_PERIOD;
    case Qt::Key_Slash: return GHOSTTY_KEY_SLASH;
    case Qt::Key_Space: return GHOSTTY_KEY_SPACE;

    // Functional
    case Qt::Key_Return: return GHOSTTY_KEY_ENTER;
    case Qt::Key_Enter: return GHOSTTY_KEY_NUMPAD_ENTER;
    case Qt::Key_Backspace: return GHOSTTY_KEY_BACKSPACE;
    case Qt::Key_Tab: return GHOSTTY_KEY_TAB;
    case Qt::Key_Escape: return GHOSTTY_KEY_ESCAPE;
    case Qt::Key_CapsLock: return GHOSTTY_KEY_CAPS_LOCK;

    // Navigation
    case Qt::Key_Up: return GHOSTTY_KEY_ARROW_UP;
    case Qt::Key_Down: return GHOSTTY_KEY_ARROW_DOWN;
    case Qt::Key_Left: return GHOSTTY_KEY_ARROW_LEFT;
    case Qt::Key_Right: return GHOSTTY_KEY_ARROW_RIGHT;
    case Qt::Key_Home: return GHOSTTY_KEY_HOME;
    case Qt::Key_End: return GHOSTTY_KEY_END;
    case Qt::Key_PageUp: return GHOSTTY_KEY_PAGE_UP;
    case Qt::Key_PageDown: return GHOSTTY_KEY_PAGE_DOWN;
    case Qt::Key_Insert: return GHOSTTY_KEY_INSERT;
    case Qt::Key_Delete: return GHOSTTY_KEY_DELETE;

    // Function keys
    case Qt::Key_F1: return GHOSTTY_KEY_F1;
    case Qt::Key_F2: return GHOSTTY_KEY_F2;
    case Qt::Key_F3: return GHOSTTY_KEY_F3;
    case Qt::Key_F4: return GHOSTTY_KEY_F4;
    case Qt::Key_F5: return GHOSTTY_KEY_F5;
    case Qt::Key_F6: return GHOSTTY_KEY_F6;
    case Qt::Key_F7: return GHOSTTY_KEY_F7;
    case Qt::Key_F8: return GHOSTTY_KEY_F8;
    case Qt::Key_F9: return GHOSTTY_KEY_F9;
    case Qt::Key_F10: return GHOSTTY_KEY_F10;
    case Qt::Key_F11: return GHOSTTY_KEY_F11;
    case Qt::Key_F12: return GHOSTTY_KEY_F12;

    default: return GHOSTTY_KEY_UNIDENTIFIED;
    }
}

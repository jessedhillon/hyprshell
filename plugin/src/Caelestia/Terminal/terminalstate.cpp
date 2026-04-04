#include "terminalstate.hpp"
#include "pty.hpp"

TerminalState::TerminalState(QObject* parent) : QObject(parent) {}

TerminalState::~TerminalState() {
    if (m_mouseEncoder) ghostty_mouse_encoder_free(m_mouseEncoder);
    if (m_keyEncoder) ghostty_key_encoder_free(m_keyEncoder);
    if (m_renderState) ghostty_render_state_free(m_renderState);
    if (m_terminal) ghostty_terminal_free(m_terminal);
}

bool TerminalState::init(uint16_t cols, uint16_t rows, size_t maxScrollback) {
    GhosttyTerminalOptions opts{};
    opts.cols = cols;
    opts.rows = rows;
    opts.max_scrollback = maxScrollback;

    if (ghostty_terminal_new(nullptr, &m_terminal, opts) != GHOSTTY_SUCCESS)
        return false;

    // Register userdata and effect callbacks
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_USERDATA, this);

    auto writePty = &writePtyCallback;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_WRITE_PTY,
                         reinterpret_cast<const void*>(writePty));

    auto titleChanged = &titleChangedCallback;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,
                         reinterpret_cast<const void*>(titleChanged));

    auto bell = &bellCallback;
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_BELL,
                         reinterpret_cast<const void*>(bell));

    // Set default colors (light-on-dark)
    GhosttyColorRgb fg = {.r = 204, .g = 204, .b = 204};
    GhosttyColorRgb bg = {.r = 30, .g = 30, .b = 30};
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_FOREGROUND, &fg);
    ghostty_terminal_set(m_terminal, GHOSTTY_TERMINAL_OPT_COLOR_BACKGROUND, &bg);

    if (ghostty_render_state_new(nullptr, &m_renderState) != GHOSTTY_SUCCESS)
        return false;

    if (ghostty_key_encoder_new(nullptr, &m_keyEncoder) != GHOSTTY_SUCCESS)
        return false;

    if (ghostty_mouse_encoder_new(nullptr, &m_mouseEncoder) != GHOSTTY_SUCCESS)
        return false;

    return true;
}

void TerminalState::feedData(const char* data, size_t len) {
    if (!m_terminal) return;
    ghostty_terminal_vt_write(m_terminal,
                              reinterpret_cast<const uint8_t*>(data), len);
}

void TerminalState::resize(uint16_t cols, uint16_t rows,
                           uint16_t cellWidth, uint16_t cellHeight) {
    if (!m_terminal) return;
    ghostty_terminal_resize(m_terminal, cols, rows, cellWidth, cellHeight);
}

void TerminalState::scrollViewport(int delta) {
    if (!m_terminal) return;
    GhosttyTerminalScrollViewport sv{};
    sv.tag = GHOSTTY_SCROLL_VIEWPORT_DELTA;
    sv.value.delta = delta;
    ghostty_terminal_scroll_viewport(m_terminal, sv);
}

GhosttyRenderStateDirty TerminalState::updateRenderState() {
    if (!m_terminal || !m_renderState)
        return GHOSTTY_RENDER_STATE_DIRTY_FALSE;

    ghostty_render_state_update(m_renderState, m_terminal);

    GhosttyRenderStateDirty dirty = GHOSTTY_RENDER_STATE_DIRTY_FALSE;
    ghostty_render_state_get(m_renderState, GHOSTTY_RENDER_STATE_DATA_DIRTY, &dirty);
    return dirty;
}

void TerminalState::writePtyCallback(GhosttyTerminal, void* userdata,
                                     const uint8_t* data, size_t len) {
    auto* self = static_cast<TerminalState*>(userdata);
    if (self->m_pty)
        self->m_pty->write(reinterpret_cast<const char*>(data), len);
}

void TerminalState::titleChangedCallback(GhosttyTerminal terminal, void* userdata) {
    auto* self = static_cast<TerminalState*>(userdata);
    // Query the title from the terminal
    GhosttyString title{};
    if (ghostty_terminal_get(terminal, GHOSTTY_TERMINAL_DATA_TITLE, &title) == GHOSTTY_SUCCESS) {
        emit self->titleChanged(QString::fromUtf8(
            reinterpret_cast<const char*>(title.ptr), title.len));
    }
}

void TerminalState::bellCallback(GhosttyTerminal, void* userdata) {
    auto* self = static_cast<TerminalState*>(userdata);
    emit self->bellRing();
}

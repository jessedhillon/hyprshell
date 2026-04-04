#pragma once

#include <QObject>
#include <ghostty/vt.h>
#include <cstdint>

class Pty;

class TerminalState : public QObject {
    Q_OBJECT
public:
    explicit TerminalState(QObject* parent = nullptr);
    ~TerminalState() override;

    bool init(uint16_t cols, uint16_t rows, size_t maxScrollback = 10000);
    void feedData(const char* data, size_t len);
    void resize(uint16_t cols, uint16_t rows, uint16_t cellWidth, uint16_t cellHeight);
    void scrollViewport(int delta);

    GhosttyTerminal terminal() const { return m_terminal; }
    GhosttyRenderState renderState() const { return m_renderState; }
    GhosttyKeyEncoder keyEncoder() const { return m_keyEncoder; }
    GhosttyMouseEncoder mouseEncoder() const { return m_mouseEncoder; }

    GhosttyRenderStateDirty updateRenderState();

    void setPty(Pty* pty) { m_pty = pty; }

signals:
    void titleChanged(const QString& title);
    void bellRing();

private:
    static void writePtyCallback(GhosttyTerminal terminal, void* userdata,
                                 const uint8_t* data, size_t len);
    static void titleChangedCallback(GhosttyTerminal terminal, void* userdata);
    static void bellCallback(GhosttyTerminal terminal, void* userdata);

    GhosttyTerminal m_terminal = nullptr;
    GhosttyRenderState m_renderState = nullptr;
    GhosttyKeyEncoder m_keyEncoder = nullptr;
    GhosttyMouseEncoder m_mouseEncoder = nullptr;
    Pty* m_pty = nullptr;
};

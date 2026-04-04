#include "pty.hpp"

#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>

Pty::Pty(QObject* parent) : QObject(parent) {}

Pty::~Pty() { close(); }

bool Pty::spawn(const std::string& shell, uint16_t cols, uint16_t rows,
                uint16_t cellWidth, uint16_t cellHeight) {
    struct winsize ws {};
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = cols * cellWidth;
    ws.ws_ypixel = rows * cellHeight;

    m_pid = forkpty(&m_masterFd, nullptr, nullptr, &ws);
    if (m_pid < 0) return false;

    if (m_pid == 0) {
        // Child process
        const char* sh = shell.empty() ? getenv("SHELL") : shell.c_str();
        if (!sh) sh = "/bin/sh";
        setenv("TERM", "xterm-256color", 1);
        execlp(sh, sh, nullptr);
        _exit(1);
    }

    // Parent — set non-blocking
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &Pty::onReadReady);

    return true;
}

void Pty::resize(uint16_t cols, uint16_t rows,
                 uint16_t cellWidth, uint16_t cellHeight) {
    if (m_masterFd < 0) return;
    struct winsize ws {};
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = cols * cellWidth;
    ws.ws_ypixel = rows * cellHeight;
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

void Pty::write(const char* data, size_t len) {
    if (m_masterFd < 0) return;
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(m_masterFd, data + written, len - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break;
        }
        written += n;
    }
}

void Pty::close() {
    if (m_notifier) {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
    if (m_pid > 0) {
        kill(m_pid, SIGHUP);
        waitpid(m_pid, nullptr, WNOHANG);
        m_pid = -1;
    }
}

void Pty::onReadReady() {
    char buf[4096];
    while (true) {
        ssize_t n = ::read(m_masterFd, buf, sizeof(buf));
        if (n > 0) {
            emit dataReady(QByteArray(buf, n));
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
            close();
            emit finished(0);
            break;
        } else {
            break; // EAGAIN
        }
    }
}

#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <string>

class Pty : public QObject {
    Q_OBJECT
public:
    explicit Pty(QObject* parent = nullptr);
    ~Pty() override;

    bool spawn(const std::string& shell, uint16_t cols, uint16_t rows,
               uint16_t cellWidth, uint16_t cellHeight);
    void resize(uint16_t cols, uint16_t rows,
                uint16_t cellWidth, uint16_t cellHeight);
    void write(const char* data, size_t len);
    void close();

    int masterFd() const { return m_masterFd; }
    bool isRunning() const { return m_pid > 0; }

signals:
    void dataReady(const QByteArray& data);
    void finished(int exitCode);

private slots:
    void onReadReady();

private:
    int m_masterFd = -1;
    pid_t m_pid = -1;
    QSocketNotifier* m_notifier = nullptr;
};

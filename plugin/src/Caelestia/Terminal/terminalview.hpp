#pragma once

#include <QQuickPaintedItem>
#include <QFont>
#include <QFontMetricsF>
#include <QTimer>
#include <QVarLengthArray>
#include <ghostty/vt.h>
#include <memory>

class Pty;
class TerminalState;

class TerminalView : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString shell READ shell WRITE setShell NOTIFY shellChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(qreal fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)

public:
    explicit TerminalView(QQuickItem* parent = nullptr);
    ~TerminalView() override;

    void paint(QPainter* painter) override;

    QString shell() const { return m_shell; }
    void setShell(const QString& shell);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& family);
    qreal fontSize() const { return m_fontSize; }
    void setFontSize(qreal size);
    QString title() const { return m_title; }

signals:
    void shellChanged();
    void fontFamilyChanged();
    void fontSizeChanged();
    void titleChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void componentComplete() override;

private slots:
    void onPtyData(const QByteArray& data);
    void onRefresh();

private:
    void startTerminal();
    void recalculateGrid();
    void paintCell(QPainter* painter, int x, int y,
                   GhosttyRenderStateRowCells cells,
                   const GhosttyRenderStateColors& colors);
    QColor ghosttyToQColor(GhosttyColorRgb rgb);
    GhosttyKey qtKeyToGhosttyKey(int qtKey);
    GhosttyMods qtModsToGhosttyMods(Qt::KeyboardModifiers mods);

    QString m_shell;
    QString m_fontFamily = QStringLiteral("monospace");
    qreal m_fontSize = 11.0;
    QString m_title;

    QFont m_font;
    std::unique_ptr<QFontMetricsF> m_fontMetrics;
    qreal m_cellWidth = 0;
    qreal m_cellHeight = 0;
    uint16_t m_cols = 0;
    uint16_t m_rows = 0;

    std::unique_ptr<Pty> m_pty;
    std::unique_ptr<TerminalState> m_state;
    GhosttyRenderStateRowIterator m_rowIterator = nullptr;
    GhosttyRenderStateRowCells m_rowCells = nullptr;

    QTimer m_refreshTimer;
    bool m_started = false;
};

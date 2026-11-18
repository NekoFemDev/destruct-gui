#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows Omarchy's accent colour and Hyprland's window rounding live, so the
// app dresses like the rest of the desktop. Adapted from Omacut (MIT, David
// Heinemeier Hansson).
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    // The app's own palette comes from its icon: warn yellow on near-black.
    // It deliberately does not follow the desktop accent, so the mark, the
    // launcher icon, and the window read as one product.
    Q_PROPERTY(QString brandAccent READ brandAccent CONSTANT)
    Q_PROPERTY(QString brandAccentForeground READ brandAccentForeground CONSTANT)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr,
                   const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString brandAccent() const { return QStringLiteral("#FFD60A"); }
    QString brandAccentForeground() const { return QStringLiteral("black"); }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = QStringLiteral("#FFD60A");
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};

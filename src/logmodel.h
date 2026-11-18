#pragma once

#include <QAbstractListModel>
#include <QStringList>

// The live output of the running destruct process, one row per line. Keeping
// lines in a model instead of one huge string lets the ListView stay smooth
// on verbose ELF runs that print a line per function.
class LogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        KindRole,
    };

    // How the line is coloured: ordinary output, an error, a warning, the
    // command that was run, or a final summary.
    enum Kind {
        Normal = 0,
        Error = 1,
        Warning = 2,
        Command = 3,
        Summary = 4,
    };
    Q_ENUM(Kind)

    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_lines.size(); }
    bool isEmpty() const { return m_lines.isEmpty(); }

    void append(const QString &text);
    void clear();
    QString toText() const;

signals:
    void countChanged();

private:
    struct Line {
        QString text;
        int kind = Normal;
    };

    static int classify(const QString &text);

    QVector<Line> m_lines;
};

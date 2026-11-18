#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>

// Every file under a finished job's output directory, as one flat, filtered
// list. A flat list beats a tree here: decompiled output is often thousands of
// files in deep package directories, and filtering on the relative path gets
// to one faster than expanding folders.
class FileListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY countChanged)

public:
    enum Roles {
        RelativePathRole = Qt::UserRole + 1,
        FileNameRole,
        FolderRole,
        SizeRole,
        TextualRole,
    };

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_visible.size(); }
    int totalCount() const { return m_entries.size(); }
    qint64 totalSize() const;
    bool truncated() const { return m_truncated; }

    void scan(const QString &root, int limit = 50000);
    void clear();
    Q_INVOKABLE void setFilter(const QString &filter);
    QString filter() const { return m_filter; }

    // The absolute path for a visible row, or empty if the row is stale.
    QString absolutePath(int row) const;

signals:
    void countChanged();

private:
    struct Entry {
        QString relativePath;
        QString fileName;
        QString folder;
        qint64 size = 0;
        bool textual = false;
    };

    void refilter();

    QVector<Entry> m_entries;
    QVector<int> m_visible;
    QString m_root;
    QString m_filter;
    bool m_truncated = false;
};

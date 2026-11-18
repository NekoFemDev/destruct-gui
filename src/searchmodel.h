#pragma once

#include <QAbstractListModel>
#include <QVector>

// Matches from the in-output text search. Each row remembers the file and
// line so clicking it can open the preview at the right place.
class SearchModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        FileRole = Qt::UserRole + 1,
        LineRole,
        TextRole,
    };

    struct Hit {
        QString file;
        int line = 0;
        QString text;
    };

    explicit SearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_hits.size(); }

    void setHits(const QVector<Hit> &hits);
    void clear();

signals:
    void countChanged();

private:
    QVector<Hit> m_hits;
};

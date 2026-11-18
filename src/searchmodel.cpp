#include "searchmodel.h"

SearchModel::SearchModel(QObject *parent) : QAbstractListModel(parent) {}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_hits.size();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_hits.size())
        return {};
    const Hit &hit = m_hits.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return hit.text;
    case FileRole:
        return hit.file;
    case LineRole:
        return hit.line;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {{FileRole, "file"}, {LineRole, "line"}, {TextRole, "text"}};
}

void SearchModel::setHits(const QVector<Hit> &hits)
{
    beginResetModel();
    m_hits = hits;
    endResetModel();
    emit countChanged();
}

void SearchModel::clear()
{
    if (m_hits.isEmpty())
        return;
    beginResetModel();
    m_hits.clear();
    endResetModel();
    emit countChanged();
}

#include "filelistmodel.h"

#include "formats.h"

#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

FileListModel::FileListModel(QObject *parent) : QAbstractListModel(parent) {}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return {};
    const Entry &entry = m_entries.at(m_visible.at(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case RelativePathRole:
        return entry.relativePath;
    case FileNameRole:
        return entry.fileName;
    case FolderRole:
        return entry.folder;
    case SizeRole:
        return entry.size;
    case TextualRole:
        return entry.textual;
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {RelativePathRole, "relativePath"},
        {FileNameRole, "fileName"},
        {FolderRole, "folder"},
        {SizeRole, "size"},
        {TextualRole, "textual"},
    };
}

void FileListModel::scan(const QString &root, int limit)
{
    beginResetModel();
    m_entries.clear();
    m_visible.clear();
    m_truncated = false;
    m_root = root;

    if (!root.isEmpty()) {
        QDirIterator iterator(root, QDir::Files | QDir::NoSymLinks,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            if (m_entries.size() >= limit) {
                m_truncated = true;
                break;
            }
            const QFileInfo info(path);
            Entry entry;
            entry.relativePath = QDir(root).relativeFilePath(path);
            entry.fileName = info.fileName();
            entry.folder = QFileInfo(entry.relativePath).path();
            if (entry.folder == QStringLiteral("."))
                entry.folder.clear();
            entry.size = info.size();
            entry.textual = formats::isTextPath(path);
            m_entries.append(entry);
        }
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        return a.relativePath.compare(b.relativePath, Qt::CaseInsensitive) < 0;
    });

    m_visible.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_filter.isEmpty()
            || m_entries.at(i).relativePath.contains(m_filter, Qt::CaseInsensitive))
            m_visible.append(i);
    }
    endResetModel();
    emit countChanged();
}

void FileListModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_visible.clear();
    m_root.clear();
    m_truncated = false;
    m_filter.clear();
    endResetModel();
    emit countChanged();
}

void FileListModel::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    refilter();
}

void FileListModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_filter.isEmpty()
            || m_entries.at(i).relativePath.contains(m_filter, Qt::CaseInsensitive))
            m_visible.append(i);
    }
    endResetModel();
    emit countChanged();
}

QString FileListModel::absolutePath(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return {};
    return QDir(m_root).filePath(m_entries.at(m_visible.at(row)).relativePath);
}

qint64 FileListModel::totalSize() const
{
    qint64 total = 0;
    for (const Entry &entry : m_entries)
        total += entry.size;
    return total;
}

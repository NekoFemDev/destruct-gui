#include "logmodel.h"

LogModel::LogModel(QObject *parent) : QAbstractListModel(parent) {}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};
    const Line &line = m_lines.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return line.text;
    case KindRole:
        return line.kind;
    default:
        return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{TextRole, "text"}, {KindRole, "kind"}};
}

int LogModel::classify(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return Normal;
    if (trimmed.startsWith(QStringLiteral("$ destruct")))
        return Command;
    if (trimmed.startsWith(QStringLiteral("Error:"))
        || trimmed.startsWith(QStringLiteral("panic:"))
        || trimmed.contains(QStringLiteral("[failed to decompile")))
        return Error;
    if (trimmed.startsWith(QStringLiteral("warning:"))
        || trimmed.startsWith(QStringLiteral("Warning:"))
        || trimmed.contains(QStringLiteral("warning: ")))
        return Warning;
    if (trimmed.startsWith(QStringLiteral("Decompil"))
        || trimmed.startsWith(QStringLiteral("Disassembly"))
        || trimmed.startsWith(QStringLiteral("Wrote "))
        || trimmed.startsWith(QStringLiteral("Output:"))
        || trimmed.startsWith(QStringLiteral("IL2CPP dump complete"))
        || trimmed.startsWith(QStringLiteral("No changes detected"))
        || trimmed.startsWith(QStringLiteral("Successfully compiled")))
        return Summary;
    return Normal;
}

void LogModel::append(const QString &text)
{
    // The CLI prints progress with a carriage return; a line that arrives
    // with a stray \r is shown without it.
    QString clean = text;
    clean.remove(QLatin1Char('\r'));

    // Keep the view responsive on pathological output: drop the oldest
    // thousand lines at a time rather than one per insert.
    if (m_lines.size() >= 6000) {
        beginRemoveRows({}, 0, 999);
        m_lines.remove(0, 1000);
        endRemoveRows();
    }

    beginInsertRows({}, m_lines.size(), m_lines.size());
    m_lines.append({clean, classify(clean)});
    endInsertRows();
    emit countChanged();
}

void LogModel::clear()
{
    if (m_lines.isEmpty())
        return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
}

QString LogModel::toText() const
{
    QStringList lines;
    lines.reserve(m_lines.size());
    for (const Line &line : m_lines)
        lines << line.text;
    return lines.join(QLatin1Char('\n'));
}

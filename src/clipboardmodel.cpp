/*
    Copyright (c) 2009, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clipboardmodel.h"
#include <QDebug>
#include <QMap>

QString escape(const QString &str)
{
    static QMap<QChar,QString> repl;
    if ( repl.isEmpty() ) {
        repl[QChar(' ')] = QString("&nbsp;");
        repl[QChar('\t')] = QString("&nbsp;&nbsp;");
        repl[QChar('\n')] = QString("<br />");
        repl[QChar('>')] = QString("&gt;");
        repl[QChar('<')] = QString("&lt;");
        repl[QChar('&')] = QString("&amp;");
    }
    QString res;

    for ( QString::const_iterator it = str.begin(); it < str.end(); ++it ) {
        QString str(repl[*it]);
        if( str.isEmpty() )
            res += *it;
        else
            res += str;
    }

    return res;
}

ClipboardModel::ClipboardModel(const QStringList &items)
    : QAbstractListModel()
{
    foreach( QString str, items) {
        m_clipboardList.append(str);
    }
}

int ClipboardModel::rowCount(const QModelIndex&) const
{
    return m_clipboardList.count();
}

QVariant ClipboardModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_clipboardList.size())
        return QVariant();

    const QImage *image = m_clipboardList[index.row()].image();
    if ( image && (role == Qt::DisplayRole || role == Qt::EditRole) )
            return *image;
    else if (role == Qt::DisplayRole)
        return m_clipboardList[index.row()].highlighted();
    else if (role == Qt::EditRole)
        return m_clipboardList.at(index.row());
    else
        return QVariant();
}

Qt::ItemFlags ClipboardModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}

bool ClipboardModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        int row = index.row();
        if ( value.type() == QVariant::Image )
            m_clipboardList[row].setImage( value.value<QImage>() );
        else
            m_clipboardList.replace( row, value.toString() );
        setSearch(row);
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

bool ClipboardModel::insertRows(int position, int rows, const QModelIndex&)
{
    beginInsertRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.insert( position, QString() );
    }

    endInsertRows();
    return true;
}

bool ClipboardModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(empty_index, position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_clipboardList.removeAt(position);
    }

    endRemoveRows();
    return true;
}

int ClipboardModel::getRowNumber(int row, bool cycle) const
{
    int n = rowCount();
    if (n == 0)
        return -1;
    if (row >= n)
        return cycle ? 0 : n-1;
    else if (row < 0)
        return cycle ? n-1 : 0;
    else
        return row;
}

bool ClipboardModel::move(int pos, int newpos) {
    int from = getRowNumber(pos,true);
    int to   = getRowNumber(newpos,true);

    if( from == -1 || to == -1 )
        return false;
    if ( !beginMoveRows(empty_index, from, from, empty_index,
                        from < to ? to+1 : to) )
        return false;
    m_clipboardList.move(from, to);
    endMoveRows();
    return true;
}

/**
@fn  moveItems
@arg list items to move
@arg key move items in given direction (Qt::Key_Down, Qt::Key_Up, Qt::Key_End, Qt::Key_Home)
@return true if some item was moved to the top (item to clipboard), otherwise false
*/
bool ClipboardModel::moveItems(QModelIndexList list, int key) {
    qSort(list.begin(),list.end());
    int from, to, last;
    bool res = false;

    for(int i = 0; i<list.length(); ++i) {
        if (key == Qt::Key_Down || key == Qt::Key_End)
            from = list.at(list.length()-1-i).row();
        else
            from = list.at(i).row();

        switch (key) {
        case Qt::Key_Down:
            to = from+1;
            break;
        case Qt::Key_Up:
            to = from-1;
            break;
        case Qt::Key_End:
            to = rowCount()-i-1;
            break;
        default:
            to = 0+i;
            break;
        }
        last = from;
        if ( !move(from, to) )
            return false;
        if (!res)
            res = to==0;
    }

    return res;
}

bool ClipboardModel::isFiltered(int i) const
{
    return m_clipboardList[i].isFiltered();
}

void ClipboardModel::setSearch(int i, const QRegExp *const re)
{
    const QString &str = m_clipboardList[i];
    QString highlight;

    if ( !re || re->isEmpty() ) {
        m_clipboardList[i].setFiltered(false);
        return;
    }

    int a = 0;
    int b = re->indexIn(str, a);
    int len;

    while ( b != -1 ) {
        len = re->matchedLength();
        if ( len == 0 )
            break;

        highlight.append( escape(str.mid(a, b-a)) );
        highlight.append( "<span class=\"em\">" );
        highlight.append( escape(str.mid(b, len)) );
        highlight.append( "</span>" );

        a = b + len;
        b = re->indexIn(str, a);
    }

    // filter items
    if ( highlight.isEmpty() )
        m_clipboardList[i].setFiltered(true);
    else {
        if ( a != str.length() )
            highlight += escape(str.mid(a));
        // highlight matched
        m_clipboardList[i].setFiltered(false);
        m_clipboardList[i].setHighlight(highlight);
    }
}

void ClipboardModel::setSearch(const QRegExp *const re)
{
    if (!re) {
        if ( m_re.isEmpty() )
            return; // search already empty

        m_re = QRegExp();
    }
    else if ( m_re == *re )
        return; // search already set
    else
        m_re = *re;

    for( int i = 0; i<rowCount(); i++)
        setSearch(i, &m_re);

    emit dataChanged( index(0,0), index(rowCount()-1,0) );
}

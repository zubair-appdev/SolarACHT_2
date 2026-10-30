
#ifndef TESTTABLEVIEWSREMARKS_H
#define TESTTABLEVIEWSREMARKS_H

#include <QTableView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QApplication>
#include <QStandardItemModel>
#include <QStringList>
#include <QAbstractItemModel>
#include <QRegularExpression>
#include <QItemSelectionModel>
#include <algorithm>

class TestTableViewsRemarks : public QTableView
{
    Q_OBJECT

public:
    explicit TestTableViewsRemarks(QWidget *parent = nullptr)
        : QTableView(parent),
          m_remarksEditingEnabled(false)
    {
        setEditTriggers(
            QAbstractItemView::DoubleClicked |
            QAbstractItemView::EditKeyPressed);

        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

    void setRemarksEditingEnabled(bool enabled)
    {
        m_remarksEditingEnabled = enabled;

        setSelectionBehavior(
            enabled ? QAbstractItemView::SelectItems
                    : QAbstractItemView::SelectRows);

        setSelectionMode(
            enabled ? QAbstractItemView::ExtendedSelection
                    : QAbstractItemView::SingleSelection);

        updateRemarksFlags();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (!model())
        {
            QTableView::keyPressEvent(event);
            return;
        }

        const QModelIndex index = currentIndex();

        if (!index.isValid())
        {
            QTableView::keyPressEvent(event);
            return;
        }

        const int remarksColumn = findRemarksColumn();

        const bool isRemarks =
            remarksColumn >= 0 &&
            index.column() == remarksColumn;

        const bool ctrl =
            event->modifiers() & Qt::ControlModifier;

        // Copy the current Remarks cell.
        if (isRemarks && ctrl && event->key() == Qt::Key_C)
        {
            QApplication::clipboard()->setText(
                index.data(Qt::DisplayRole).toString());
            return;
        }

        // Paste into selected Remarks cells.
        if (isRemarks && ctrl && event->key() == Qt::Key_V)
        {
            if (!m_remarksEditingEnabled)
                return;

            QString text = QApplication::clipboard()->text();

            if (text.isEmpty())
                return;

            text.replace("\r\n", "\n");
            text.replace('\r', '\n');

            QStringList lines =
                text.split('\n', Qt::KeepEmptyParts);

            // Ignore the trailing newline commonly copied
            // from Excel or other applications.
            if (!lines.isEmpty() && lines.last().isEmpty())
                lines.removeLast();

            if (lines.isEmpty())
                return;

            const int column = remarksColumn;

            // Collect unique selected Remarks rows.
            QList<int> selectedRows;

            const QModelIndexList selectedIndexes =
                selectionModel()->selectedIndexes();

            for (const QModelIndex &selected : selectedIndexes)
            {
                if (selected.column() == column)
                    selectedRows.append(selected.row());
            }

            std::sort(selectedRows.begin(), selectedRows.end());

            selectedRows.erase(
                std::unique(selectedRows.begin(),
                            selectedRows.end()),
                selectedRows.end());

            // Ensure the current cell is included.
            if (!selectedRows.contains(index.row()))
                selectedRows.append(index.row());

            std::sort(selectedRows.begin(), selectedRows.end());

            // Multiple selected cells + one clipboard line:
            // copy the same text into every selected cell.
            if (selectedRows.size() > 1 && lines.size() == 1)
            {
                for (int row : selectedRows)
                {
                    model()->setData(
                        model()->index(row, column),
                        lines.first(),
                        Qt::EditRole);
                }

                return;
            }

            // Multiple clipboard lines + multiple selected cells:
            // paste one line per selected cell.
            if (selectedRows.size() > 1)
            {
                const int count =
                    qMin(lines.size(), selectedRows.size());

                for (int i = 0; i < count; ++i)
                {
                    model()->setData(
                        model()->index(selectedRows[i], column),
                        lines[i],
                        Qt::EditRole);
                }

                return;
            }

            // Only one cell selected:
            // paste multiple lines downward from that row.
            int row = index.row();

            for (const QString &line : lines)
            {
                if (row >= model()->rowCount())
                    break;

                model()->setData(
                    model()->index(row, column),
                    line,
                    Qt::EditRole);

                ++row;
            }

            return;
        }

        // Prevent keyboard editing of other columns.
        if (m_remarksEditingEnabled &&
            !isRemarks &&
            (event->key() == Qt::Key_F2 ||
             event->key() == Qt::Key_Return ||
             event->key() == Qt::Key_Enter))
        {
            return;
        }

        QTableView::keyPressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        const QModelIndex index = indexAt(event->pos());

        if (m_remarksEditingEnabled &&
            index.isValid() &&
            index.column() == findRemarksColumn())
        {
            setCurrentIndex(index);
            edit(index);
            return;
        }

        QTableView::mouseDoubleClickEvent(event);
    }

private:
    bool m_remarksEditingEnabled;

    int findRemarksColumn() const
    {
        if (!model())
            return -1;

        for (int col = 0; col < model()->columnCount(); ++col)
        {
            if (model()->headerData(col, Qt::Horizontal)
                    .toString()
                    .compare("Remarks", Qt::CaseInsensitive) == 0)
            {
                return col;
            }
        }

        return -1;
    }

    void updateRemarksFlags()
    {
        QStandardItemModel *standardModel =
            qobject_cast<QStandardItemModel *>(model());

        if (!standardModel)
            return;

        const int remarksColumn = findRemarksColumn();

        if (remarksColumn < 0)
            return;

        for (int row = 0;
             row < standardModel->rowCount();
             ++row)
        {
            QStandardItem *item =
                standardModel->item(row, remarksColumn);

            if (!item)
            {
                item = new QStandardItem;

                standardModel->setItem(
                    row, remarksColumn, item);
            }

            item->setEditable(m_remarksEditingEnabled);
        }
    }
};

#endif // TESTTABLEVIEWSREMARKS_H

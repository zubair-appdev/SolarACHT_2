
#ifndef CUSTOMTABLEVIEW_H
#define CUSTOMTABLEVIEW_H

#include <QTableView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QModelIndexList>
#include <QMessageBox>
#include <QStringList>
#include <QKeySequence>
#include <QDebug>
#include <algorithm>

class CustomTableView : public QTableView
{
    Q_OBJECT

public:
    explicit CustomTableView(QWidget *parent = nullptr)
        : QTableView(parent)
    {
        setSelectionMode(QAbstractItemView::SingleSelection);
        setSelectionBehavior(QAbstractItemView::SelectItems);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }

    void setEditingEnabled(bool enabled)
    {
        m_editingEnabled = enabled;

        setSelectionMode(
            enabled ? QAbstractItemView::ExtendedSelection
                    : QAbstractItemView::SingleSelection);

        setSelectionBehavior(QAbstractItemView::SelectItems);

        setEditTriggers(
            enabled ? QAbstractItemView::DoubleClicked
                    : QAbstractItemView::NoEditTriggers);

        if (!enabled)
        {
            clearSelection();
            m_copiedValues.clear();
        }

        qDebug() << "[CustomTableView]"
                 << objectName()
                 << "Editing enabled:" << enabled;
    }

    bool isEditingEnabled() const
    {
        return m_editingEnabled;
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (!m_editingEnabled)
        {
            event->accept();
            return;
        }

        QModelIndex index = indexAt(event->pos());

        if (!index.isValid() ||
            !(index.flags() & Qt::ItemIsEditable))
        {
            event->accept();
            return;
        }

        setCurrentIndex(index);
        edit(index);

        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        // Copy is allowed only in Edit mode.
        if (event->matches(QKeySequence::Copy))
        {
            if (m_editingEnabled)
                copySelection();

            event->accept();
            return;
        }

        // Paste is allowed only in Edit mode.
        if (event->matches(QKeySequence::Paste))
        {
            if (m_editingEnabled)
                pasteSelection();

            event->accept();
            return;
        }

        QTableView::keyPressEvent(event);
    }

private:
    bool m_editingEnabled = false;
    QStringList m_copiedValues;

    void copySelection()
    {
        if (!m_editingEnabled)
            return;

        QAbstractItemModel *tableModel = model();
        QItemSelectionModel *selModel = selectionModel();

        if (!tableModel || !selModel)
        {
            qDebug() << "[Copy] Model or selection model is null";
            return;
        }

        // Copy only explicitly selected cells.
        // Do not access currentIndex() when selection is empty.
        QModelIndexList indexes = selModel->selectedIndexes();

        if (indexes.isEmpty())
        {
            qDebug() << "[Copy] No cells selected";
            return;
        }

        // Keep a consistent top-to-bottom, left-to-right order.
        std::sort(indexes.begin(), indexes.end(),
                  [](const QModelIndex &a, const QModelIndex &b)
        {
            if (a.row() != b.row())
                return a.row() < b.row();

            return a.column() < b.column();
        });

        QStringList values;

        for (const QModelIndex &index : indexes)
        {
            if (!index.isValid())
            {
                qDebug() << "[Copy] Invalid index";
                return;
            }

            values.append(
                index.data(Qt::DisplayRole).toString());
        }

        if (values.isEmpty())
            return;

        m_copiedValues = values;

        QClipboard *clipboard = QApplication::clipboard();

        if (clipboard)
            clipboard->setText(m_copiedValues.join('\t'));

        qDebug() << "[Copy] Copied cells:"
                 << m_copiedValues.size();
    }

    void pasteSelection()
    {
        if (!m_editingEnabled)
            return;

        QAbstractItemModel *tableModel = model();
        QItemSelectionModel *selModel = selectionModel();

        if (!tableModel || !selModel)
            return;

        if (m_copiedValues.isEmpty())
        {
            QMessageBox::warning(
                this,
                "Paste Error",
                "Select and copy cells before pasting.");
            return;
        }

        QModelIndexList indexes = selModel->selectedIndexes();

        if (indexes.isEmpty())
        {
            qDebug() << "[Paste] No destination cells selected";
            return;
        }

        if (indexes.size() != m_copiedValues.size())
        {
            QMessageBox::warning(
                this,
                "Paste Error",
                QString("Copied cells: %1\n"
                        "Destination cells: %2\n\n"
                        "Select exactly %1 destination cell(s).")
                    .arg(m_copiedValues.size())
                    .arg(indexes.size()));
            return;
        }

        std::sort(indexes.begin(), indexes.end(),
                  [](const QModelIndex &a, const QModelIndex &b)
        {
            if (a.row() != b.row())
                return a.row() < b.row();

            return a.column() < b.column();
        });

        // Validate all destination cells before writing.
        for (const QModelIndex &index : indexes)
        {
            if (!index.isValid() ||
                !(index.flags() & Qt::ItemIsEditable))
            {
                QMessageBox::warning(
                    this,
                    "Paste Error",
                    "One or more destination cells are not editable.");
                return;
            }
        }

        // Write copied values to the selected destination cells.
        for (int i = 0; i < indexes.size(); ++i)
        {
            if (!tableModel->setData(
                    indexes.at(i),
                    m_copiedValues.at(i),
                    Qt::EditRole))
            {
                QMessageBox::warning(
                    this,
                    "Paste Error",
                    QString("Failed to paste at row %1, column %2.")
                        .arg(indexes.at(i).row() + 1)
                        .arg(indexes.at(i).column() + 1));
                return;
            }
        }

        qDebug() << "[Paste] Successfully pasted cells:"
                 << indexes.size();
    }
};

#endif // CUSTOMTABLEVIEW_H

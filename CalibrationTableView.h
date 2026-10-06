#ifndef CALIBRATIONTABLEVIEW_H
#define CALIBRATIONTABLEVIEW_H

#include <QTableView>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>

class CalibrationTableView : public QTableView
{
    Q_OBJECT

public:
    explicit CalibrationTableView(QWidget *parent = nullptr)
        : QTableView(parent)
    {
    }

    void setClipboardEnabled(bool enabled)
    {
        m_clipboardEnabled = enabled;
    }

protected:

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        QModelIndex index =
                indexAt(event->pos());

        if (!index.isValid())
            return;

        // Only column 2 can be edited
        if (index.column() != 2)
            return;

        QTableView::mouseDoubleClickEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_clipboardEnabled &&
            event->matches(QKeySequence::Copy))
        {
            QModelIndexList indexes =
                    selectionModel()->selectedIndexes();

            for (const QModelIndex &index : indexes)
            {
                if (index.column() != 2)
                    continue;

                QApplication::clipboard()->setText(
                    index.data().toString());

                break;
            }

            return;
        }

        if (m_clipboardEnabled &&
            event->matches(QKeySequence::Paste))
        {
            QString value =
                    QApplication::clipboard()->text();

            if (value.isEmpty())
                return;

            QModelIndexList indexes =
                    selectionModel()->selectedIndexes();

            for (const QModelIndex &index : indexes)
            {
                if (index.column() != 2)
                    continue;

                model()->setData(index, value);
            }

            return;
        }

        QTableView::keyPressEvent(event);
    }

private:
    bool m_clipboardEnabled = false;
};

#endif

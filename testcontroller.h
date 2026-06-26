#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include <QObject>
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>


class MainWindow;
class TestController:public QObject
{
    Q_OBJECT
public:
    explicit TestController(QObject *parent = nullptr);
     ~TestController();
    void welcome();
    QStringList availablePorts();
    void setPORTNAME(const QString &portName);
    bool isConnected() const;
    void setLogger(MainWindow *logger);
    bool mapLogicalToHardware(const QVariantList &patchData,const QVariantList &harnessData);

signals:
    void infor();
    void portOpening(const QString &);
    void executeWriteToNotes(const QString &dataNotes);

private:
    QSerialPort *serial;
    MainWindow *m_obj=nullptr;
    QByteArray buffer;

    QVector<QString> k_sourceCon;
    QVector<QString> k_sourcePin;
    QVector<QString> k_destinationCon;
    QVector<QString> k_destinationPin;

    QVector<QString> sourceCon, sourcePin, destCon, destPin, exp;
    QString m_simulate;

    // Patch data for Iso/Insu tests
    QVector<QString> userCon, userPin, patchCon, patchPin;
    QVector<QVector<QVector<QPair<QString, QString>>>> changedAllFixedGroups;
    QVector<QVector<QVector<QPair<QString, int>>>> allFixedGroups;

};

#endif // TESTCONTROLLER_H

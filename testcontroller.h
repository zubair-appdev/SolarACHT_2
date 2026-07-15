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

    inline const QVector<QString>& get_Cable() const
    {
        return cable;
    }
    inline const QVector<QString>& get_k_SourceCon() const
    {
        return k_sourceCon;
    }

    inline const QVector<QString>& get_k_SourcePin() const
    {
        return k_sourcePin;
    }

    inline const QVector<QString>& get_k_DestinationCon() const
    {
        return k_destinationCon;
    }

    inline const QVector<QString>& get_k_DestinationPin() const
    {
        return k_destinationPin;
    }

    inline const QVector<QString>& get_expResistance() const
    {
        return exp;
    }

    void startTwoWireTransmission(const QByteArray &startPacket,
                                     const QVector<QByteArray> &packets);

private slots:
    void onReadyRead();

signals:
    void infor();
    void portOpening(const QString &);
    void executeWriteToNotes(const QString &dataNotes);

private:
    QSerialPort *serial;
    MainWindow *m_obj = nullptr;
    QByteArray buffer;

    QVector<QString> k_sourceCon;
    QVector<QString> k_sourcePin;
    QVector<QString> k_destinationCon;
    QVector<QString> k_destinationPin;

    QVector<QString> cable, sourceCon, sourcePin, destCon, destPin, exp;
    QString m_simulate;

    // Patch data for Iso/Insu tests
    QVector<QString> userCon, userPin, patchCon, patchPin;
    QVector<QVector<QVector<QPair<QString, QString>>>> changedAllFixedGroups;
    QVector<QVector<QVector<QPair<QString, int>>>> allFixedGroups;

    // Two Wire Packet Sending
    QByteArray m_startPacket;
    QVector<QByteArray> m_packets;

    int m_currentPacket = -1;

    bool m_waitingForStartAck = false;
    bool m_waitingForPacketAck = false;

};

#endif // TESTCONTROLLER_H

#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include <QObject>
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>

#include <QEventLoop>
#include <QTimer>
#include <QApplication>

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

    inline QVector<float> getTwoWireResults() const
    {
        return m_twoWireResults;
    }

    void setTwoWireExpectedResults(int count);

    //Self Test
    void startSelfTest();

    //Calibration Test
    void startCalibration();
    void sendCalibrationSlot(quint8 slotNumber);
    void sendNextCalibrationSlot();
    void handleCalibrationReception();

    int calibrationSlotIndex() const
    {
        return m_currentCalibrationSlotIndex;
    }

    inline void pauseFor(int milliseconds) {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);  // After delay, quit the event loop
        loop.exec();  // Start the event loop and wait for it to quit
        QApplication::processEvents();  // Keep UI healthy
    }

    //Abort Command
    void abortCommand();

private slots:
    void onReadyRead();

signals:
    void infor();
    void portOpening(const QString &);
    void executeWriteToNotes(const QString &dataNotes);

    void twoWireResultReceived(float value, int index);

    void twoWireResultsCompleted();

    //Self Test Signals

    void selfTestSlotStarted(int slotNumber);

    // One slot response completed
    void selfTestSlotResult(
            int slotNumber,
            const QByteArray &slotResult);

    // All 30 slot responses completed
    void selfTestResultsCompleted(
            const QByteArray &allResults);

    //Calibration Test
    void calibrationSlotReceived(
            int slotNumber,
            const QByteArray &slotData);

    void calibrationSlotMissing(
            int slotNumber);

    void calibrationCompleted();


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

    QVector<float> m_twoWireResults;
    bool m_receivingTwoWireResults = false;

    int m_twoWireExpectedResults = 0;

    //Self Test
    QVector<quint8> m_selfTestSlots;
    int m_currentSelfTestSlotIndex = -1;
    bool m_selfTestRunning = false;

    QByteArray resultsOfSelfTestBytes;

    //Calibration Test
    QVector<quint8> m_calibrationSlots;
    int m_currentCalibrationSlotIndex = -1;
    bool m_calibrationRunning = false;
};

#endif // TESTCONTROLLER_H

#ifndef TESTCONTROLLER_H
#define TESTCONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QPrinter>
#include <QDesktopServices>
#include <QtEndian>
#include <QFont>
#include <QFontMetrics>
#include <QPageSize>
#include <QPrinter>
#include <QPainter>
#include <QTimer>

class LoginClass;

class TestController : public QObject
{
    Q_OBJECT

public:
    explicit TestController(QObject *parent = nullptr);
    ~TestController();

    Q_INVOKABLE QStringList availablePorts();
    Q_INVOKABLE bool connectPort(const QString &portName);
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE bool isConnected() const;

    void setLogger(LoginClass *logger);

    Q_INVOKABLE bool mapLogicalToHardware(const QVariantList &patchData,
                                          const QVariantList &harnessData);


    QByteArray constructUARTPacketForTwoWire(   const QVector<QString> &sourceCon,
                                                const QVector<QString> &sourcePin,
                                                const QVector<QString> &destCon,
                                                const QVector<QString> &destPin);

    Q_INVOKABLE bool runTwoWireContinuity(QString simulate);

    Q_INVOKABLE void startTwoWireRun();

    void processTwoWireResult(const QByteArray &zRecvByte);

    void generateTwoWirePDF(
        const QVector<QString>& sourceCon,
        const QVector<QString>& sourcePin,
        const QVector<QString>& destinationCon,
        const QVector<QString>& destinationPin,
        const QVector<QString>& expectedValues,
        const QVector<QString>& measuredValues,
        const QVector<QString>& results,
        const QString &projectName,
        const QString &setNo,
        const QString &testToRun,
        const QString &performedBy,
        const QString &inspectedBy);

    QVector<QString> formatResistanceValues(const QVector<float> &resistanceValues) {
        QVector<QString> formattedValues;
        QString ohmSymbol = QChar(0x03A9);

        for (float value : resistanceValues) {
            QString formattedValue;

            if (value > -1 && value < 1000) {
                formattedValue = QString::number(value, 'f', 2) + " " + ohmSymbol;  // Display in ohms
            } else if (value >= 1000 && value < 1000000) {
                formattedValue = QString::number(value / 1000, 'f', 2) + " k" + ohmSymbol;  // Convert to kΩ
            } else if (value >= 1000000 && value < 1000000000) {
                formattedValue = QString::number(value / 1000000, 'f', 2) + " M" + ohmSymbol;  // Convert to MΩ
            } else if (value >= 1000000000 && value < 1000000000000) {
                formattedValue = QString::number(value / 1000000000, 'f', 2) + " G" + ohmSymbol;  // Convert to GΩ
            } else {
                qDebug() << "Invalid resistance value: " << value;
                formattedValue = QString::number(1000000000000 / 1000000000, 'f', 2) + " G" + ohmSymbol;  // Convert to GΩ
            }

            formattedValues.append(formattedValue);
        }

        return formattedValues;
    }

    QVector<float> convertHexToFloatVector(const QByteArray &byteArray) {
        QVector<float> floatValues;

        // Ensure data length is a multiple of 4
        if (byteArray.size() % 4 != 0) {
            qDebug() << "Hex data length is not a multiple of 4. Some data may be ignored.";
            return {};
        }

        for (int i = 0; i + 3 < byteArray.size(); i += 4) {
            uint32_t intValue;
            memcpy(&intValue, byteArray.data() + i, sizeof(uint32_t));

            // Reverse byte order (change endianness)
            intValue = qFromBigEndian(intValue);

            // Convert to float
            float floatValue;
            memcpy(&floatValue, &intValue, sizeof(float));

            floatValue = floatValue - 0.35f; //For Calibration purpose 22Apr2025

            if(floatValue < 0)
            {
                floatValue = 0.02f;
            }

            floatValues.append(floatValue);
        }

        return floatValues;
    }

    Q_INVOKABLE bool runIsolation(const QString &simulate);

    void sendNextIsoPacket(const QString &simulate);

    void handleIsoInsuRx();

    QByteArray buildIsoInsuCommand();

    void extractAndStoreResult(const QByteArray &buffer);

    void abortIsoInsuTest(const QString &reason);

    Q_INVOKABLE bool runInsulation(const QString &simulate);

    void prepareFixedGroupsForPDF();

    void generateIsoInsuPdf(
        const QByteArray &resultData,
        const QString &testName   // "ISOLATION" or "INSULATION"
        );



private slots:
    void onReadyRead();

signals:
    void portStatusChanged(bool connected, const QString &msg);

    void twoWireFinished(bool success, const QString &message);

private:
    QSerialPort serial;
    LoginClass *login = nullptr;

    QVector<QString> k_sourceCon;
    QVector<QString> k_sourcePin;
    QVector<QString> k_destinationCon;
    QVector<QString> k_destinationPin;

    QByteArray rxBuffer;
    bool twoWireInProgress = false;


    // Harness data for two wire pdf
    QVector<QString> sourceCon, sourcePin, destCon, destPin, exp;
    QString m_simulate;

    // Patch data for Iso/Insu tests
    QVector<QString> userCon, userPin, patchCon, patchPin;
    QVector<QVector<QVector<QPair<QString, QString>>>> changedAllFixedGroups;
    QVector<QVector<QVector<QPair<QString, int>>>> allFixedGroups;

    // Isolation and Insulation tests
    void buildNetData();                 // COMMON
    QByteArray buildUARTPacket(const QVector<int> &netList);

    QVector<QVector<int>> allNetLists;
    QVector<QVector<QByteArray>> allUARTpacket;

    enum class ActiveTest {
        None,
        TwoWire,
        Isolation,
        Insulation
    };

    ActiveTest activeTest = ActiveTest::None;

    enum class IsoState {
        Idle,
        WaitingEndAfterNet,
        WaitingEndAfterCmd,
        WaitingFinalResult
    };

    IsoState isoState = IsoState::Idle;
    int currentPacketIndex = 0;

    QVector<int> totalNetsEachPacket;   // same meaning as old code
    QVector<int> voltageEachPacket;     // one voltage byte per packet

    QByteArray isolationResult;
    QByteArray insulationResult;

    bool zeroErrorShown = false;   // prevent repeated failure spam

};

#endif // TESTCONTROLLER_H

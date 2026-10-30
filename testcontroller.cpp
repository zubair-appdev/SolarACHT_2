#include "testcontroller.h"
#include "mainwindow.h"

TestController::TestController(QObject *parent) : QObject(parent)
{
    qDebug()<<"hello";

    serial = new QSerialPort(this);

    connect(serial, &QSerialPort::readyRead,
            this, &TestController::onReadyRead);
}

TestController::~TestController()
{

}
void TestController::welcome()
{
    emit infor();
    qDebug()<<"#####" ;
}

QStringList TestController::availablePorts()
{
    QStringList ports;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ports<<info.portName();
    }
    return ports;
}

void TestController::setPORTNAME(const QString &portName)
{
    buffer.clear();

    if(serial->isOpen())
    {
        serial->close();
    }

    serial->setPortName(portName);
    serial->setBaudRate(115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);


    if(!serial->open(QIODevice::ReadWrite))
    {
        qDebug()<<"Failed to open port"<<serial->portName();
        emit portOpening("Failed to open port "+serial->portName());
    }
    else
    {
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 115200";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 115200");
    }
}

bool TestController::isConnected() const
{
    return serial->isOpen();
}

void TestController::setLogger(MainWindow *logger)
{
    m_obj = logger;
}

bool TestController::mapLogicalToHardware(const QVariantList &patchData,
                                          const QVariantList &harnessData)
{
    if (!m_obj)
        return false;

    emit executeWriteToNotes("===========================================");
    emit executeWriteToNotes("STEP-1:Mapping Logical Connectors->Hardware Pins");
    emit executeWriteToNotes("=============================================");

    // Clear previous mappings
    cable.clear();
    k_sourceCon.clear();
    k_sourcePin.clear();
    k_destinationCon.clear();
    k_destinationPin.clear();
    exp.clear();

    // -------- Extract PATCH data --------
    QVector<QString> userCon,userPin,patchCon, patchPin;

    for (const QVariant &v : patchData) {
        QVariantMap m = v.toMap();
        userCon.append(m["userCon"].toString());
        userPin.append(m["userPin"].toString());
        patchCon.append(m["patchCon"].toString());
        patchPin.append(QString::number(m["patchPin"].toInt()));
    }

    this->userCon = userCon;
    this->userPin = userPin;
    this->patchCon = patchCon;
    this->patchPin = patchPin;

    emit executeWriteToNotes(QString("Patch rows loaded : %1").arg(userCon.size()));

    // -------- Extract HARNESS data --------
    QVector<QString> cable, sourceCon, sourcePin, destCon, destPin, exp;

    for (const QVariant &v : harnessData) {
        QVariantMap m = v.toMap();

        cable.append(m["cable"].toString());
        sourceCon.append(m["sourceCon"].toString());
        sourcePin.append(m["sourcePin"].toString());
        destCon.append(m["destCon"].toString());
        destPin.append(m["destPin"].toString());
        exp.append(m["exp"].toString());
    }

    this->cable = cable;
    this->sourceCon = sourceCon;
    this->sourcePin = sourcePin;
    this->destCon = destCon;
    this->destPin = destPin;
    this->exp = exp;

    emit executeWriteToNotes(QString("Harness rows loaded : %1").arg(sourceCon.size()));


    // -------- Mapping process --------
    bool mappingOk = true;
    QString errorLog;

    for (int i = 0; i < sourceCon.size(); ++i)
    {
        bool srcFound = false;
        bool dstFound = false;

        // ---------------- Source ----------------
        for (int j = 0; j < userCon.size(); ++j)
        {
            if (sourceCon[i] == userCon[j] &&
                sourcePin[i] == userPin[j])
            {
                k_sourceCon.append(patchCon[j]);
                k_sourcePin.append(patchPin[j]);
                srcFound = true;
                break;
            }
        }

        // ---------------- Destination ----------------
        for (int j = 0; j < userCon.size(); ++j)
        {
            if (destCon[i] == userCon[j] &&
                destPin[i] == userPin[j])
            {
                k_destinationCon.append(patchCon[j]);
                k_destinationPin.append(patchPin[j]);
                dstFound = true;
                break;
            }
        }

        if (!srcFound || !dstFound)
        {
            mappingOk = false;
            errorLog += QString(
                "Row %1 mapping missing | SRC(%2:%3) DST(%4:%5)\n")
                .arg(i + 1)
                .arg(sourceCon[i])
                .arg(sourcePin[i])
                .arg(destCon[i])
                .arg(destPin[i]);
        }
    }

    // -------- Validation result --------
    if (!mappingOk)
    {
        emit executeWriteToNotes("❌ Mapping FAILED");
        emit executeWriteToNotes(errorLog);
        return false;
    }

    // -------- Log successful mappings --------
    emit executeWriteToNotes(QString("✔ Mapping SUCCESS | Total Connections: %1")
                            .arg(k_sourceCon.size()));

    for (int i = 0; i < k_sourceCon.size(); ++i)
    {
        emit executeWriteToNotes(
            QString("MAP %1 : %2-%3  →  %4-%5")
                .arg(i + 1)
                .arg(k_sourceCon[i])
                .arg(k_sourcePin[i])
                .arg(k_destinationCon[i])
                .arg(k_destinationPin[i]));
    }

    emit executeWriteToNotes("STEP-1 COMPLETED SUCCESSFULLY");
    return true;
}

QString TestController::detectDevicePort()
{
    qDebug() << "\n========== DEVICE AUTO DETECTION START ==========";

    // Return existing connection if already connected.
    if (serial->isOpen())
    {
        qDebug() << "[INFO] Already connected:" << serial->portName();
        return serial->portName();
    }

    const QByteArray packet =
        QByteArray::fromHex("53 4B 31 01 28");

    const QList<QByteArray> expectedResponses = {
        QByteArray::fromHex("51 52 FF 01 01"),
        QByteArray::fromHex("51 52 FF FF 01")
    };

    const int responseTimeoutMs = 500;

    const QList<QSerialPortInfo> ports =
        QSerialPortInfo::availablePorts();

    qDebug() << "[INFO] Available ports:" << ports.size();

    foreach (const QSerialPortInfo &info, ports)
    {
        QSerialPort tempSerial;
        tempSerial.setPort(info);

        tempSerial.setBaudRate(QSerialPort::Baud115200);
        tempSerial.setDataBits(QSerialPort::Data8);
        tempSerial.setParity(QSerialPort::NoParity);
        tempSerial.setStopBits(QSerialPort::OneStop);
        tempSerial.setFlowControl(QSerialPort::NoFlowControl);

        qDebug() << "[CHECK] Port:" << info.portName();

        if (!tempSerial.open(QIODevice::ReadWrite))
        {
            qDebug() << "[WARN] Cannot open"
                     << info.portName() << ":"
                     << tempSerial.errorString();
            continue;
        }

        // Clear stale data before sending the request.
        tempSerial.clear(QSerialPort::AllDirections);

        // Transmit detection command.
        if (tempSerial.write(packet) != packet.size() ||
            !tempSerial.waitForBytesWritten(500))
        {
            qDebug() << "[WARN] Command transmission failed on"
                     << info.portName();

            tempSerial.close();
            continue;
        }

        // Receive response.
        QByteArray response;
        QElapsedTimer timer;
        timer.start();

        while (timer.elapsed() < responseTimeoutMs &&
               response.size() < expectedResponses.first().size())
        {
            if (tempSerial.bytesAvailable() == 0)
            {
                if (!tempSerial.waitForReadyRead(50))
                    continue;
            }

            response.append(tempSerial.readAll());
        }

        // Accept either valid response.
        if (expectedResponses.contains(response))
        {
            const QString detectedPort = info.portName();

            tempSerial.close();

            qDebug() << "[SUCCESS] Device detected on:"
                     << detectedPort;

            qDebug() << "========== AUTO DETECTION SUCCESS ==========\n";

            return detectedPort;
        }

        if (response.isEmpty())
        {
            qDebug() << "[WARN] No response on" << info.portName();
        }
        else
        {
            qDebug() << "[WARN] Invalid response on"
                     << info.portName()
                     << ":" << response.toHex(' ').toUpper();
        }

        tempSerial.close();
    }

    qDebug() << "[FAIL] Device not detected on any available port";
    qDebug() << "========== DEVICE AUTO DETECTION END ==========\n";

    return QString();
}

void TestController::startTwoWireTransmission(
        const QByteArray &startPacket,
        const QVector<QByteArray> &packets)
{
    if(!serial->isOpen())
    {
        qDebug() << "Serial Port not open";
        return;
    }

    // Reset previous transmission state
    buffer.clear();

    m_twoWireResults.clear();
    m_receivingTwoWireResults = false;

    m_packets = packets;
    m_startPacket = startPacket;

    m_currentPacket = -1;

    m_waitingForStartAck = true;
    m_waitingForPacketAck = false;

    serial->write(m_startPacket);

    emit executeWriteToNotes("TX START : "
                             + m_startPacket.toHex(' ').toUpper());

    qDebug() << "Start Packet Sent";
}

void TestController::setTwoWireExpectedResults(int count)
{
    m_twoWireExpectedResults = count;

    qDebug() << "Expected Two Wire Results:"
             << m_twoWireExpectedResults;
}

void TestController::startSelfTest()
{
    if (!serial->isOpen())
    {
        emit executeWriteToNotes(
                    "Self Test Error: Serial port not open.");
        return;
    }

    // -------------------------------------------------
    // Reset Self Test state
    // -------------------------------------------------

    // Clear previous communication data
    buffer.clear();

    m_selfTestSlots.clear();

    m_currentSelfTestSlotIndex = -1;

    m_selfTestRunning = true;

    resultsOfSelfTestBytes.clear();

    // -------------------------------------------------
    // Create slot sequence
    //
    // 1 ... 29
    // 31
    //
    // Slot 30 is intentionally missing.
    // -------------------------------------------------

    for (quint8 slot = 1; slot <= 29; ++slot)
    {
        m_selfTestSlots.append(slot);
    }

    m_selfTestSlots.append(31);

    // -------------------------------------------------
    // Start with first slot
    // -------------------------------------------------

    m_currentSelfTestSlotIndex = 0;

    quint8 slotNum =
            m_selfTestSlots[
                m_currentSelfTestSlotIndex];

    // -------------------------------------------------
    // Build command
    //
    // 53 4B 39 SLOT XOR
    // -------------------------------------------------

    QByteArray command;

    command.append(
                static_cast<quint8>(0x53));

    command.append(
                static_cast<quint8>(0x4B));

    command.append(
                static_cast<quint8>(0x39));

    command.append(slotNum);

    quint8 checksum =
            0x53 ^
            0x4B ^
            0x39 ^
            slotNum;

    command.append(checksum);

    // -------------------------------------------------
    // Send first Self Test command
    // -------------------------------------------------

    emit selfTestSlotStarted(
        static_cast<int>(slotNum));

    serial->write(command);

    QString txHex =
            QString::fromLatin1(
                command.toHex(' ').toUpper());

    qDebug() << "Self Test TX:"
             << txHex;

    emit executeWriteToNotes(
                QString("Self Test TX Slot %1 : %2")
                .arg(static_cast<int>(slotNum))
                .arg(txHex));

    qDebug()
            << "Self Test TX Slot:"
            << slotNum
            << command.toHex(' ').toUpper();
}

void TestController::startCalibration()
{
    if (!serial || !serial->isOpen())
    {
        QMessageBox::warning(
            nullptr,
            "Calibration Test",
            "Serial port is not connected.");

        return;
    }

    // -------------------------------------------------
    // Calibration slot sequence
    // 1 ... 29, 31
    // -------------------------------------------------

    m_calibrationSlots.clear();

    for (quint8 slot = 1; slot <= 29; ++slot)
    {
        m_calibrationSlots.append(slot);
    }

    m_calibrationSlots.append(31);

    // -------------------------------------------------
    // Reset state
    // -------------------------------------------------

    m_currentCalibrationSlotIndex = 0;
    m_calibrationRunning = true;

    buffer.clear();

    // -------------------------------------------------
    // Send first slot
    // -------------------------------------------------

    sendCalibrationSlot(
        m_calibrationSlots[
                m_currentCalibrationSlotIndex]);
}

void TestController::sendCalibrationSlot(
    quint8 slotNumber)
{
    QByteArray command;

    command.append(0x53);
    command.append(0x4B);
    command.append(0x37);
    command.append(slotNumber);

    quint8 checksum =
        0x53 ^
        0x4B ^
        0x37 ^
        slotNumber;

    command.append(checksum);

    serial->write(command);

    emit executeWriteToNotes(
           QString("Calibration TX: %1")
               .arg(QString::fromLatin1(
                   command.toHex(' ').toUpper())));

    qDebug() << "Calibration TX:"
             << command.toHex(' ').toUpper();
}

void TestController::sendNextCalibrationSlot()
{
    m_currentCalibrationSlotIndex++;

    // =================================================
    // ALL 30 SLOTS COMPLETED
    // =================================================

    if (m_currentCalibrationSlotIndex >=
        m_calibrationSlots.size())
    {
        m_calibrationRunning = false;

        qDebug() << "Calibration:"
                 << "All 30 slots completed.";

        emit executeWriteToNotes(
            "Calibration Data Received - "
            "All 30 Slots Completed");

        emit calibrationCompleted();

        return;
    }

    // =================================================
    // SEND NEXT SLOT
    // =================================================

    const quint8 nextSlot =
        m_calibrationSlots[
            m_currentCalibrationSlotIndex];

    qDebug() << "Calibration:"
             << "Sending next slot"
             << nextSlot;

    emit executeWriteToNotes(
        QString("Calibration TX Slot %1")
            .arg(static_cast<int>(nextSlot)));

    sendCalibrationSlot(nextSlot);
}

void TestController::handleCalibrationReception()
{
    const QByteArray endMarker =
        QByteArray::fromHex("4142434445");

    const QByteArray missingSlot =
        QByteArray::fromHex("24242424");

    if (m_currentCalibrationSlotIndex < 0 ||
        m_currentCalibrationSlotIndex >=
            m_calibrationSlots.size())
    {
        qDebug() << "Calibration ERROR:"
                 << "Invalid slot index.";

        return;
    }

    const quint8 currentSlot =
        m_calibrationSlots[
            m_currentCalibrationSlotIndex];

    // =================================================
    // SLOT MISS
    // =================================================

    if (buffer.size() >= missingSlot.size() &&
        buffer.left(missingSlot.size()) == missingSlot)
    {
        buffer.remove(
            0,
            missingSlot.size());

        qDebug() << "Calibration RX Slot"
                 << currentSlot
                 << ": SLOT MISS";

        emit executeWriteToNotes(
            QString("Calibration RX Slot %1 : SLOT MISS")
                .arg(static_cast<int>(currentSlot)));

        emit calibrationSlotMissing(
            static_cast<int>(currentSlot));

        pauseFor(800);
        sendNextCalibrationSlot();

        return;
    }

    // =================================================
    // NORMAL SLOT
    // =================================================

    const int endIndex =
        buffer.indexOf(endMarker);

    if (endIndex < 0)
    {
        // Incomplete RX.
        // Keep waiting for more serial data.
        return;
    }

    QByteArray slotData =
        buffer.left(endIndex);

    buffer.remove(
        0,
        endIndex + endMarker.size());

    // =================================================
    // Verify 64 FLOATS
    // =================================================

    if (slotData.size() != 256)
    {
        qDebug() << "Calibration ERROR:"
                 << "Slot" << currentSlot
                 << "expected 256 bytes, received"
                 << slotData.size();

        emit executeWriteToNotes(
            QString(
                "Calibration ERROR Slot %1 : "
                "Expected 256 bytes, received %2")
                .arg(static_cast<int>(currentSlot))
                .arg(slotData.size()));

        return;
    }

    // =================================================
    // COMPLETE SLOT RECEIVED
    // =================================================

    qDebug() << "Calibration RX Slot"
             << currentSlot
             << ": 256 bytes received";

    emit executeWriteToNotes(
        QString(
            "Calibration RX Slot %1 : "
            "256 bytes received")
            .arg(static_cast<int>(currentSlot)));

    emit calibrationSlotReceived(
        static_cast<int>(currentSlot),
        slotData);

    // =================================================
    // NEXT SLOT
    // =================================================

    pauseFor(800);
    sendNextCalibrationSlot();
}

void TestController::sendInsulationStartPacket(
    quint16 numberOfPackets,
    quint16 numberOfLooms,
    float insulationVoltage,
    float resistanceThreshold,
    quint8 testBetweenConnectors,
    quint8 testType,
    const QVector<QByteArray> &packets)
{
    QByteArray packet;

    packet.append(char(0x53));
    packet.append(char(0x4B));
    packet.append(char(0x34));

    // Number Of Packets
    packet.append(
        char((numberOfPackets >> 8) & 0xFF));

    packet.append(
        char(numberOfPackets & 0xFF));

    // Number Of Looms
    packet.append(
        char((numberOfLooms >> 8) & 0xFF));

    packet.append(
        char(numberOfLooms & 0xFF));

    // Insulation Voltage - reverse float byte order
    quint32 voltageBits;

    memcpy(
        &voltageBits,
        &insulationVoltage,
        sizeof(float));

    packet.append(
        char((voltageBits >> 24) & 0xFF));

    packet.append(
        char((voltageBits >> 16) & 0xFF));

    packet.append(
        char((voltageBits >> 8) & 0xFF));

    packet.append(
        char(voltageBits & 0xFF));

    // Resistance Threshold - reverse float byte order
    quint32 resistanceBits;

    memcpy(
        &resistanceBits,
        &resistanceThreshold,
        sizeof(float));

    packet.append(
        char((resistanceBits >> 24) & 0xFF));

    packet.append(
        char((resistanceBits >> 16) & 0xFF));

    packet.append(
        char((resistanceBits >> 8) & 0xFF));

    packet.append(
        char(resistanceBits & 0xFF));

    // Test Between Connectors
    packet.append(
        char(testBetweenConnectors));

    // Test Type : 3F (Insulation), 4F (Isolation)
    packet.append(char(testType));

    // XOR
    quint8 checksum = 0;

    for (char byte : packet)
    {
        checksum ^=
            static_cast<quint8>(byte);
    }

    packet.append(char(checksum));

    // Store Insulation packets
    m_insulationPackets = packets;

    m_currentInsulationPacket = 0;

    // Set Insulation state
    m_waitingForInsulationStartAck = true;
    m_waitingForInsulationPacketAck = false;
    m_receivingInsulationResults = false;

    m_insulationResults.clear();

    m_currentInsulationLoom = 0;

    m_insulationNetMarkersSeen = 0;

    // TX START packet
    serial->write(packet);

    qDebug() << "Insulation Start TX:"
             << packet.toHex(' ').toUpper();

    emit executeWriteToNotes(
        QString("Insulation Start TX: %1")
            .arg(QString::fromLatin1(
                packet.toHex(' ').toUpper())));
}

void TestController::abortCommand()
{
    serial->write(QByteArray::fromHex("41 42 4F 52 54"));;
    emit executeWriteToNotes("TX Abort : 41 42 4F 52 54");

    // Stop Self Test
    m_selfTestRunning = false;
    m_currentSelfTestSlotIndex = -1;
    m_selfTestSlots.clear();

    // Stop Calibration
    m_calibrationRunning = false;
    m_currentCalibrationSlotIndex = -1;
    m_calibrationSlots.clear();

    // Stop Two Wire
    m_receivingTwoWireResults = false;
    m_waitingForStartAck = false;
    m_waitingForPacketAck = false;

    // Reset packet state
    m_currentPacket = 0;
    m_packets.clear();

    // Stop Insulation / Isolation
    m_waitingForInsulationStartAck = false;
    m_waitingForInsulationPacketAck = false;
    m_receivingInsulationResults = false;

    // Stop/Reset Insulation / Isolation packet state
    m_currentInsulationPacket = 0;
    m_insulationPackets.clear();

    // Clear results
    m_insulationResults.clear();

    // Reset loom state
    m_currentInsulationLoom = 0;

    // Clear received data
    buffer.clear();
    resultsOfSelfTestBytes.clear();
    m_twoWireResults.clear();

    // Stop Relay
    m_waitingForRelayStartAck = false;
    m_waitingForRelayPacketAck = false;
    m_waitingForRelayResults = false;

    // Reset Relay packet state
    m_currentRelayPacket = 0;
    m_relayPackets.clear();

    // Reset Relay result state
    m_relayResultsReceived = 0;
    m_relayResultIndex = 0;
    m_relayResultBuffer.clear();

    // Stop Diode
    m_waitingForDiodeStartAck = false;
    m_waitingForDiodePacketAck = false;
    m_waitingForDiodeResults = false;

    // Reset Diode packet state
    m_currentDiodePacket = 0;
    m_diodePackets.clear();

    // Reset Diode result state
    m_diodeResultsReceived = 0;

    emit executeWriteToNotes(
                "Test Aborted");
}

bool TestController::mapDiodeToHardware(
    const QVariantList &patchData,
    const QVariantList &diodeData)
{
    if (!m_obj)
        return false;

    emit executeWriteToNotes("===========================================");
    emit executeWriteToNotes("STEP-1: Diode Logical Connectors -> Hardware Pins");
    emit executeWriteToNotes("===========================================");

    // Clear previous Diode mappings
    m_diodeCable.clear();
    m_diodeAnodeCon.clear();
    m_diodeAnodePin.clear();
    m_diodeCathodeCon.clear();
    m_diodeCathodePin.clear();
    m_diodeExpVoltage.clear();

    // -------- Extract PATCH data --------
    QVector<QString> userCon, userPin, patchCon, patchPin;

    for (const QVariant &v : patchData)
    {
        QVariantMap m = v.toMap();

        userCon.append(m["userCon"].toString().simplified());
        userPin.append(m["userPin"].toString().simplified());
        patchCon.append(m["patchCon"].toString().simplified());
        patchPin.append(QString::number(m["patchPin"].toInt()));
    }

    emit executeWriteToNotes(
        QString("Patch rows loaded : %1").arg(userCon.size()));

    // -------- Extract DIODE data --------
    QVector<QString> cable, anodeCon, anodePin;
    QVector<QString> cathodeCon, cathodePin, expVoltage;

    for (const QVariant &v : diodeData)
    {
        QVariantMap m = v.toMap();

        cable.append(m["cable"].toString());
        anodeCon.append(m["anodeCon"].toString().simplified());
        anodePin.append(m["anodePin"].toString().simplified());
        cathodeCon.append(m["cathodeCon"].toString().simplified());
        cathodePin.append(m["cathodePin"].toString().simplified());
        expVoltage.append(m["expVoltage"].toString());
    }

    emit executeWriteToNotes(
        QString("Diode rows loaded : %1").arg(anodeCon.size()));

    // -------- Mapping process --------
    bool mappingOk = true;
    QString errorLog;

    for (int i = 0; i < anodeCon.size(); ++i)
    {
        bool anodeFound = false;
        bool cathodeFound = false;

        // -------- Anode mapping --------
        for (int j = 0; j < userCon.size(); ++j)
        {
            if (anodeCon[i] == userCon[j] &&
                anodePin[i] == userPin[j])
            {
                m_diodeAnodeCon.append(patchCon[j]);
                m_diodeAnodePin.append(patchPin[j]);
                anodeFound = true;
                break;
            }
        }

        // -------- Cathode mapping --------
        for (int j = 0; j < userCon.size(); ++j)
        {
            if (cathodeCon[i] == userCon[j] &&
                cathodePin[i] == userPin[j])
            {
                m_diodeCathodeCon.append(patchCon[j]);
                m_diodeCathodePin.append(patchPin[j]);
                cathodeFound = true;
                break;
            }
        }

        if (!anodeFound || !cathodeFound)
        {
            mappingOk = false;

            errorLog += QString(
                "Row %1 mapping missing | ANODE(%2:%3) CATHODE(%4:%5)\n")
                .arg(i + 1)
                .arg(anodeCon[i])
                .arg(anodePin[i])
                .arg(cathodeCon[i])
                .arg(cathodePin[i]);
        }

        m_diodeCable.append(cable[i]);
        m_diodeExpVoltage.append(expVoltage[i]);
    }

    // -------- Validation result --------
    if (!mappingOk)
    {
        emit executeWriteToNotes("Diode Mapping FAILED");
        emit executeWriteToNotes(errorLog);

        // Clear partial results after failure
        m_diodeCable.clear();
        m_diodeAnodeCon.clear();
        m_diodeAnodePin.clear();
        m_diodeCathodeCon.clear();
        m_diodeCathodePin.clear();
        m_diodeExpVoltage.clear();

        return false;
    }

    // -------- Log successful mappings --------
    emit executeWriteToNotes(
        QString("Diode Mapping SUCCESS | Total Connections: %1")
            .arg(m_diodeAnodeCon.size()));

    for (int i = 0; i < m_diodeAnodeCon.size(); ++i)
    {
        emit executeWriteToNotes(
            QString("DIODE MAP %1 : ANODE %2-%3 -> CATHODE %4-%5 | Expected: %6")
                .arg(i + 1)
                .arg(m_diodeAnodeCon[i])
                .arg(m_diodeAnodePin[i])
                .arg(m_diodeCathodeCon[i])
                .arg(m_diodeCathodePin[i])
                .arg(m_diodeExpVoltage[i]));
    }

    emit executeWriteToNotes("DIODE MAPPING COMPLETED SUCCESSFULLY");

    return true;
}

void TestController::startDiodeTransmission(
    const QByteArray &startPacket,
    const QVector<QByteArray> &packets)
{
    if (!serial || !serial->isOpen() || packets.isEmpty())
    {
        emit executeWriteToNotes(
            "Diode transmission failed: Serial closed or no packets.");
        return;
    }

    m_diodePackets = packets;
    m_currentDiodePacket = 0;

    m_waitingForDiodeStartAck = true;
    m_waitingForDiodePacketAck = false;
    m_waitingForDiodeResults = false;

    m_diodeResultsReceived = 0;

    serial->write(startPacket);

    emit executeWriteToNotes("Diode START packet sent");
    emit executeWriteToNotes(
        QString("Diode START HEX: %1")
                .arg(QString(startPacket.toHex(' ').toUpper())));
}


bool TestController::mapRelayToHardware(
    const QVariantList &patchData,
    const QVariantList &relayData)
{
    if (!m_obj)
        return false;

    emit executeWriteToNotes("===========================================");
    emit executeWriteToNotes("STEP-1: Relay Logical Connectors -> Hardware Pins");
    emit executeWriteToNotes("===========================================");

    // Clear previous Relay mappings.
    m_relayCable.clear();
    m_relaySourceCon.clear();
    m_relaySourcePin.clear();
    m_relaySideCon.clear();
    m_relaySideRSPins.clear();
    m_relayDestCon.clear();
    m_relayDestPin.clear();
    m_relayExcitationVoltage.clear();
    m_relayExp.clear();

    if (patchData.isEmpty() || relayData.isEmpty())
    {
        emit executeWriteToNotes(
            "RELAY Mapping FAILED: Patch or Relay data is empty.");
        return false;
    }

    // -------------------------------------------------
    // Extract PATCH data
    // -------------------------------------------------
    QVector<QString> userCon;
    QVector<QString> userPin;
    QVector<QString> patchCon;
    QVector<QString> patchPin;

    for (const QVariant &v : patchData)
    {
        const QVariantMap m = v.toMap();

        userCon.append(m["userCon"].toString().simplified());
        userPin.append(m["userPin"].toString().simplified());
        patchCon.append(m["patchCon"].toString().simplified());
        patchPin.append(QString::number(m["patchPin"].toInt()));
    }

    emit executeWriteToNotes(
        QString("Patch rows loaded: %1").arg(userCon.size()));

    // -------------------------------------------------
    // Extract RELAY data
    // -------------------------------------------------
    QVector<QString> cable;
    QVector<QString> sourceCon;
    QVector<QString> sourcePin;
    QVector<QString> relaySideCon;
    QVector<QString> relaySideRSPins;
    QVector<QString> destCon;
    QVector<QString> destPin;
    QVector<QString> excitationVoltage;
    QVector<QString> expected;

    for (const QVariant &v : relayData)
    {
        const QVariantMap m = v.toMap();

        cable.append(m["cable"].toString());
        sourceCon.append(m["sourceCon"].toString().simplified());
        sourcePin.append(m["sourcePin"].toString().simplified());

        relaySideCon.append(
            m["relaySideCon"].toString().simplified());

        relaySideRSPins.append(
            m["relaySideRSPins"].toString().simplified());

        destCon.append(m["destCon"].toString().simplified());
        destPin.append(m["destPin"].toString().simplified());

        excitationVoltage.append(
            m["excitationVoltage"].toString().simplified());

        expected.append(m["exp"].toString().simplified());
    }

    emit executeWriteToNotes(
        QString("Relay rows loaded: %1").arg(cable.size()));

    // -------------------------------------------------
    // Mapping process
    // -------------------------------------------------
    QString errorLog;
    bool mappingOk = true;

    for (int i = 0; i < cable.size(); ++i)
    {
        bool sourceFound = false;
        bool destFound = false;

        QString mappedSourceCon;
        QString mappedSourcePin;
        QString mappedDestCon;
        QString mappedDestPin;

        emit executeWriteToNotes("-------------------------------------------");
        emit executeWriteToNotes(
            QString("RELAY ROW %1 | Cable: %2").arg(i + 1).arg(cable[i]));

        // -------------------------------------------------
        // Map Source connector + pin to Patch hardware
        // -------------------------------------------------
        emit executeWriteToNotes(
            QString("Finding SOURCE: %1 - %2")
                .arg(sourceCon[i])
                .arg(sourcePin[i]));

        for (int j = 0; j < userCon.size(); ++j)
        {
            if (sourceCon[i] == userCon[j] &&
                sourcePin[i] == userPin[j])
            {
                mappedSourceCon = patchCon[j];
                mappedSourcePin = patchPin[j];
                sourceFound = true;

                emit executeWriteToNotes(
                    QString("SOURCE MATCH: %1-%2 -> Hardware %3-%4")
                        .arg(sourceCon[i])
                        .arg(sourcePin[i])
                        .arg(mappedSourceCon)
                        .arg(mappedSourcePin));

                break;
            }
        }

        if (!sourceFound)
        {
            errorLog += QString(
                "Row %1: SOURCE mapping missing for %2-%3\n")
                .arg(i + 1)
                .arg(sourceCon[i])
                .arg(sourcePin[i]);

            emit executeWriteToNotes(
                QString("SOURCE NOT FOUND: %1-%2")
                    .arg(sourceCon[i])
                    .arg(sourcePin[i]));
        }

        // -------------------------------------------------
        // Map Destination connector + pin to Patch hardware
        // -------------------------------------------------
        emit executeWriteToNotes(
            QString("Finding DESTINATION: %1 - %2")
                .arg(destCon[i])
                .arg(destPin[i]));

        for (int j = 0; j < userCon.size(); ++j)
        {
            if (destCon[i] == userCon[j] &&
                destPin[i] == userPin[j])
            {
                mappedDestCon = patchCon[j];
                mappedDestPin = patchPin[j];
                destFound = true;

                emit executeWriteToNotes(
                    QString("DESTINATION MATCH: %1-%2 -> Hardware %3-%4")
                        .arg(destCon[i])
                        .arg(destPin[i])
                        .arg(mappedDestCon)
                        .arg(mappedDestPin));

                break;
            }
        }

        if (!destFound)
        {
            errorLog += QString(
                "Row %1: DESTINATION mapping missing for %2-%3\n")
                .arg(i + 1)
                .arg(destCon[i])
                .arg(destPin[i]);

            emit executeWriteToNotes(
                QString("DESTINATION NOT FOUND: %1-%2")
                    .arg(destCon[i])
                    .arg(destPin[i]));
        }

        // -------------------------------------------------
        // Store aligned results only if both ends are mapped
        // -------------------------------------------------
        if (!sourceFound || !destFound)
        {
            mappingOk = false;
            continue;
        }

        // -------------------------------------------------
        // Validate coil connections: mapped ends must be TP31
        // -------------------------------------------------
        const QString rsPins = relaySideRSPins[i].trimmed().toUpper();

        if (rsPins == "X1&X2" || rsPins == "Y1&Y2")
        {
            if (mappedSourceCon.compare("TP-31", Qt::CaseInsensitive) != 0 ||
                mappedDestCon.compare("TP-31", Qt::CaseInsensitive) != 0)
            {
                emit executeWriteToNotes(
                    QString("RELAY Mapping FAILED: Coil pins must connect "
                            "to TP31 only. Relay Pins: %1, "
                            "Mapped Source Connector: %2, "
                            "Mapped Destination Connector: %3")
                        .arg(rsPins, mappedSourceCon, mappedDestCon));

                QMessageBox::warning(
                    m_obj,
                    "Relay Mapping Error",
                    QString("Coil Pins Need To Connect TP31 connector only\n\n"
                            "Relay Pins: %1\n"
                            "Mapped Source Connector: %2\n"
                            "Mapped Destination Connector: %3")
                        .arg(rsPins, mappedSourceCon, mappedDestCon));

                // Clear mappings collected from previous rows.
                m_relayCable.clear();
                m_relaySourceCon.clear();
                m_relaySourcePin.clear();
                m_relaySideCon.clear();
                m_relaySideRSPins.clear();
                m_relayDestCon.clear();
                m_relayDestPin.clear();
                m_relayExcitationVoltage.clear();
                m_relayExp.clear();

                return false;
            }
        }

        m_relayCable.append(cable[i]);

        m_relaySourceCon.append(mappedSourceCon);
        m_relaySourcePin.append(mappedSourcePin);

        // Relay-side values are metadata, not Patch endpoints.
        m_relaySideCon.append(relaySideCon[i]);
        m_relaySideRSPins.append(relaySideRSPins[i]);

        m_relayDestCon.append(mappedDestCon);
        m_relayDestPin.append(mappedDestPin);

        m_relayExcitationVoltage.append(excitationVoltage[i]);
        m_relayExp.append(expected[i]);

        emit executeWriteToNotes(
            QString("RELAY SIDE: %1 | Pins: %2 | Excitation: %3 | Expected: %4")
                .arg(relaySideCon[i])
                .arg(relaySideRSPins[i])
                .arg(excitationVoltage[i].isEmpty()
                         ? QString("N/A")
                         : excitationVoltage[i])
                .arg(expected[i]));
    }

    // -------------------------------------------------
    // Handle mapping failure
    // -------------------------------------------------
    if (!mappingOk)
    {
        emit executeWriteToNotes("RELAY Mapping FAILED");
        emit executeWriteToNotes(errorLog);

        m_relayCable.clear();
        m_relaySourceCon.clear();
        m_relaySourcePin.clear();
        m_relaySideCon.clear();
        m_relaySideRSPins.clear();
        m_relayDestCon.clear();
        m_relayDestPin.clear();
        m_relayExcitationVoltage.clear();
        m_relayExp.clear();

        return false;
    }

    // -------------------------------------------------
    // Log successful mappings
    // -------------------------------------------------
    emit executeWriteToNotes(
        QString("RELAY Mapping SUCCESS | Total Connections: %1")
            .arg(m_relayCable.size()));

    for (int i = 0; i < m_relayCable.size(); ++i)
    {
        emit executeWriteToNotes(
            QString("RELAY MAP %1: SOURCE %2-%3 -> DESTINATION %4-%5 | "
                    "Relay: %6 | Pins: %7 | Excitation: %8 | Expected: %9")
                .arg(i + 1)
                .arg(m_relaySourceCon[i])
                .arg(m_relaySourcePin[i])
                .arg(m_relayDestCon[i])
                .arg(m_relayDestPin[i])
                .arg(m_relaySideCon[i])
                .arg(m_relaySideRSPins[i])
                .arg(m_relayExcitationVoltage[i].isEmpty()
                         ? QString("N/A")
                         : m_relayExcitationVoltage[i])
                .arg(m_relayExp[i]));
    }

    emit executeWriteToNotes("RELAY MAPPING COMPLETED SUCCESSFULLY");

    return true;
}

void TestController::startRelayTransmission(
    const QByteArray &startPacket,
    const QVector<QByteArray> &packets)
{
    if (!serial || !serial->isOpen() || packets.isEmpty())
    {
        emit executeWriteToNotes(
            "Relay transmission failed: Serial closed or no packets.");
        return;
    }

    m_relayPackets = packets;
    m_currentRelayPacket = 0;

    m_relayResultBuffer.clear();
    m_relayResultIndex = 0;

    m_waitingForRelayStartAck = true;
    m_waitingForRelayPacketAck = false;
    m_waitingForRelayResults = false;

    m_relayResultsReceived = 0;

    serial->write(startPacket);

    emit executeWriteToNotes("Relay START packet sent");
    emit executeWriteToNotes(
        QString("Relay START HEX: %1")
                .arg(QString(startPacket.toHex(' ').toUpper())));
}

void TestController::skipRelayResultRows(int count)
{
    m_relayResultIndex =
        qMin(m_relayResultIndex + count,
             m_relayExpectedResults);
}


void TestController::onReadyRead()
{
    buffer.append(serial->readAll());

    qDebug() << "RX BUFFER:"
             << buffer.toHex(' ').toUpper();

    // Calibration Test Start
    if (m_calibrationRunning)
    {
        handleCalibrationReception();
        return;
    }
    // Calibration Test End

    // =====================================================
    // SELF TEST RECEPTION START
    // =====================================================

    if (m_selfTestRunning)
    {
        const QByteArray endMarker =
                QByteArray::fromHex("FF4142434445");

        const QByteArray missingSlot =
                QByteArray::fromHex("4B53410158");

        // =================================================
        // MISSING SLOT
        // CHT sends: 4B 53 41 01 58
        // We append internally to:
        // FF ABCDE
        // =================================================

        if (buffer.size() >= missingSlot.size() &&
            buffer.left(missingSlot.size()) == missingSlot)
        {
            quint8 currentSlot =
                    m_selfTestSlots[
                        m_currentSelfTestSlotIndex];

            // ---------------------------------------------
            // Store missing-slot marker
            // ---------------------------------------------

            QByteArray oneSlotResult;

            oneSlotResult.append(missingSlot);
            oneSlotResult.append(endMarker);

            resultsOfSelfTestBytes.append(
                        oneSlotResult);

            emit selfTestSlotResult(
                        static_cast<int>(currentSlot),
                        oneSlotResult);
            // ---------------------------------------------
            // Add standard slot terminator
            // ---------------------------------------------

            resultsOfSelfTestBytes.append(
                        endMarker);

            emit executeWriteToNotes(
                QString("Self Test Slot %1 RX : SLOT_MISS")
                .arg(static_cast<int>(currentSlot)));

            // ---------------------------------------------
            // Clear current slot RX buffer
            // ---------------------------------------------

            buffer.clear();

            // ---------------------------------------------
            // Move to next slot
            // ---------------------------------------------

            m_currentSelfTestSlotIndex++;

            // ---------------------------------------------
            // All slots completed
            // ---------------------------------------------

            if (m_currentSelfTestSlotIndex
                    >= m_selfTestSlots.size())
            {
                m_selfTestRunning = false;

                emit executeWriteToNotes(
                    "Self Test Completed");

                emit selfTestResultsCompleted(
                            resultsOfSelfTestBytes);

                qDebug()
                    << "SELF TEST ALL RESULTS:"
                    << resultsOfSelfTestBytes
                           .toHex(' ')
                           .toUpper();

                return;
            }

            // ---------------------------------------------
            // Send next slot
            // ---------------------------------------------

            quint8 nextSlot =
                    m_selfTestSlots[
                        m_currentSelfTestSlotIndex];

            QByteArray command;

            command.append(
                        static_cast<quint8>(0x53));

            command.append(
                        static_cast<quint8>(0x4B));

            command.append(
                        static_cast<quint8>(0x39));

            command.append(nextSlot);

            quint8 checksum =
                    0x53 ^
                    0x4B ^
                    0x39 ^
                    nextSlot;

            command.append(checksum);

            pauseFor(800);

            emit selfTestSlotStarted(
                static_cast<int>(nextSlot));

            serial->write(command);

            QString txHex =
                    QString::fromLatin1(
                        command.toHex(' ')
                        .toUpper());

            emit executeWriteToNotes(
                QString("Self Test TX Slot %1 : %2")
                .arg(static_cast<int>(nextSlot))
                .arg(txHex));

            qDebug()
                << "Self Test TX Slot:"
                << static_cast<int>(nextSlot)
                << txHex;

            return;
        }

        // =================================================
        // NORMAL SLOT RESPONSE
        // =================================================

        int endIndex =
                buffer.indexOf(endMarker);

        if (endIndex >= 0)
        {
            // ---------------------------------------------
            // One complete slot response received
            // ---------------------------------------------

            int slotEnd =
                    endIndex + endMarker.size();

            QByteArray oneSlotResult =
                    buffer.left(slotEnd);

            // ---------------------------------------------
            // Store this slot's complete raw response
            // ---------------------------------------------

            quint8 currentSlot =
                    m_selfTestSlots[
                        m_currentSelfTestSlotIndex];

            resultsOfSelfTestBytes.append(
                        oneSlotResult);

            emit selfTestSlotResult(
                        static_cast<int>(currentSlot),
                        oneSlotResult);

            emit executeWriteToNotes(
                QString("Self Test Slot %1 RX : %2")
                .arg(static_cast<int>(currentSlot))
                .arg(QString::fromLatin1(
                         oneSlotResult
                         .toHex(' ')
                         .toUpper())));

            // ---------------------------------------------
            // Clear global buffer
            // ---------------------------------------------

            buffer.clear();

            // ---------------------------------------------
            // Move to next slot
            // ---------------------------------------------

            m_currentSelfTestSlotIndex++;

            // ---------------------------------------------
            // All slots completed
            // ---------------------------------------------

            if (m_currentSelfTestSlotIndex
                    >= m_selfTestSlots.size())
            {
                m_selfTestRunning = false;

                emit executeWriteToNotes(
                    "Self Test Completed");

                emit selfTestResultsCompleted(
                            resultsOfSelfTestBytes);

                qDebug()
                    << "SELF TEST ALL RESULTS:"
                    << resultsOfSelfTestBytes
                           .toHex(' ')
                           .toUpper();

                return;
            }

            // ---------------------------------------------
            // Send next slot
            // ---------------------------------------------

            quint8 nextSlot =
                    m_selfTestSlots[
                        m_currentSelfTestSlotIndex];

            QByteArray command;

            command.append(
                        static_cast<quint8>(0x53));

            command.append(
                        static_cast<quint8>(0x4B));

            command.append(
                        static_cast<quint8>(0x39));

            command.append(nextSlot);

            quint8 checksum =
                    0x53 ^
                    0x4B ^
                    0x39 ^
                    nextSlot;

            command.append(checksum);

            pauseFor(200);

            emit selfTestSlotStarted(
                static_cast<int>(nextSlot));

            serial->write(command);

            QString txHex =
                    QString::fromLatin1(
                        command.toHex(' ')
                        .toUpper());

            emit executeWriteToNotes(
                QString("Self Test TX Slot %1 : %2")
                .arg(static_cast<int>(nextSlot))
                .arg(txHex));

            qDebug()
                << "Self Test TX Slot:"
                << static_cast<int>(nextSlot)
                << txHex;
        }

        return;
    }

    // =====================================================
    // SELF TEST RECEPTION END
    // =====================================================


    const QByteArray ack =
            QByteArray::fromHex("41434BEEB6");

    // =====================================================
    // TWO WIRE RESULT RECEPTION
    // =====================================================

    if (m_receivingTwoWireResults)
    {
        const QByteArray endMarker = "ABCDE";

        // Number of results is already known from the table/test
        int expectedResults = m_twoWireExpectedResults;

        // --------------------------------------------------------
        // Receive results
        // --------------------------------------------------------
        while (m_twoWireResults.size() < expectedResults &&
               buffer.size() >= 4)
        {

            // --------------------------------------------------------
            // Check 24 24 24 24 failure condition
            // --------------------------------------------------------
            if (quint8(buffer[0]) == 0x24 &&
                quint8(buffer[1]) == 0x24 &&
                quint8(buffer[2]) == 0x24 &&
                quint8(buffer[3]) == 0x24)
            {
                qDebug() << "Two Wire Test Failed: 24 Condition Hits";

                emit executeWriteToNotes(
                    "Two Wire Test Failed: 24 Condition Hits");

                // Stop the test and reset controller state
                abortCommand();

                // Clear remaining received data
                buffer.clear();

                QMessageBox::critical(
                    nullptr,
                    "Two Wire Test Failed",
                    "Test Failed: 24 Condition Hits"
                );

                return;
            }

            QByteArray floatBytes = buffer.left(4);

            float value = 0.0f;

            memcpy(&value,
                   floatBytes.constData(),
                   sizeof(float));

            buffer.remove(0, 4);

            m_twoWireResults.append(value);

            int resultIndex =
                    m_twoWireResults.size() - 1;

            qDebug() << "Two Wire Result:"
                     << resultIndex + 1
                     << value;

            emit executeWriteToNotes(
                QString("Two Wire Result %1 : %2")
                    .arg(resultIndex + 1)
                    .arg(value));

            // ⭐ LIVE TABLE UPDATE
            emit twoWireResultReceived(
                value,
                resultIndex);
        }

        // --------------------------------------------------------
        // All expected results received.
        // Now wait for ABCDE.
        // --------------------------------------------------------
        if (m_twoWireResults.size() == expectedResults)
        {
            int endIndex = buffer.indexOf(endMarker);

            if (endIndex >= 0)
            {
                buffer.remove(
                    0,
                    endIndex + endMarker.size());

                m_receivingTwoWireResults = false;

                emit executeWriteToNotes(
                    "Two Wire Result Transmission Completed");

                qDebug()
                    << "ABCDE received - result reception complete";

                emit twoWireResultsCompleted();
            }
        }

        return;
    }



    // =====================================================
    // RELAY RESULT RECEPTION
    // =====================================================

    if (m_waitingForRelayResults)
    {
        const QByteArray endMarker = "ABCDE";
        const int expectedResults = m_relayExpectedResults;

        while (m_relayResultIndex < expectedResults)
        {
            // ------------------------------------------------
            // 1. NACK + binary relay ID
            // ------------------------------------------------
            if (buffer.startsWith("NACK"))
            {
                // Four ASCII bytes plus one binary ID byte
                if (buffer.size() < 5)
                    return;

                const quint8 relayId =
                    static_cast<quint8>(buffer[4]);

                const int currentResultIndex =
                    m_relayResultIndex;

                buffer.remove(0, 5);

                qDebug() << "Relay NACK received"
                         << "Hardware ID:" << relayId
                         << "Result index:" << currentResultIndex;

                emit executeWriteToNotes(
                    QString("Relay NACK received. Hardware ID: %1")
                        .arg(relayId));

                // Pass the raw zero-based hardware ID.
                emit relayNackReceived(
                    relayId, currentResultIndex);

                continue;
            }

            // Wait for at least four bytes for a float.
            if (buffer.size() < 4)
                return;

            // ------------------------------------------------
            // 2. Check 24 24 24 24 failure condition
            // ------------------------------------------------
            if (quint8(buffer[0]) == 0x24 &&
                quint8(buffer[1]) == 0x24 &&
                quint8(buffer[2]) == 0x24 &&
                quint8(buffer[3]) == 0x24)
            {
                qDebug() << "Relay Test Failed: 24 Condition Hits";

                emit executeWriteToNotes(
                    "Relay Test Failed: 24 Condition Hits");

                abortCommand();
                buffer.clear();

                QMessageBox::critical(
                    nullptr,
                    "Relay Test Failed",
                    "Test Failed: 24 Condition Hits");

                return;
            }

            // ------------------------------------------------
            // 3. Parse little-endian float
            // ------------------------------------------------
            const QByteArray floatBytes = buffer.left(4);

            const quint32 bits =
                    (quint32(quint8(floatBytes[3])) << 24) |
                    (quint32(quint8(floatBytes[2])) << 16) |
                    (quint32(quint8(floatBytes[1])) << 8)  |
                     quint32(quint8(floatBytes[0]));

            float value = 0.0f;
            memcpy(&value, &bits, sizeof(float));

            buffer.remove(0, 4);

            const int resultIndex = m_relayResultIndex++;

            qDebug() << "Relay Result:"
                     << resultIndex + 1 << value;

            emit executeWriteToNotes(
                QString("Relay Result %1 : %2")
                    .arg(resultIndex + 1)
                    .arg(value));

            emit relayResultReceived(resultIndex, value);
        }

        // ----------------------------------------------------
        // 4. Wait for ABCDE after expected results
        // ----------------------------------------------------
        const int endIndex = buffer.indexOf(endMarker);

        if (endIndex >= 0)
        {
            buffer.remove(0, endIndex + endMarker.size());

            m_waitingForRelayResults = false;

            emit executeWriteToNotes(
                "Relay Result Transmission Completed");

            qDebug() << "ABCDE received - Relay result reception complete";

            emit relayResultsCompleted();
        }

        return;
    }


    // =====================================================
    // DIODE RESULT RECEPTION
    // =====================================================

    if (m_waitingForDiodeResults)
    {

        // --------------------------------------------------------
        // Check 24 24 24 24 failure condition
        // --------------------------------------------------------
        if (buffer.size() >= 4 &&
            quint8(buffer[0]) == 0x24 &&
            quint8(buffer[1]) == 0x24 &&
            quint8(buffer[2]) == 0x24 &&
            quint8(buffer[3]) == 0x24)
        {
            qDebug() << "Diode Test Failed: 24 Condition Hits";

            emit executeWriteToNotes(
                "Diode Test Failed: 24 Condition Hits");

            abortCommand();

            buffer.clear();

            QMessageBox::critical(
                nullptr,
                "Diode Test Failed",
                "Test Failed: 24 Condition Hits"
            );

            return;
        }

        const QByteArray endMarker = "ABCDE";

        // Check whether the test terminator has arrived.
        int endIndex = buffer.indexOf(endMarker);

        // Process only bytes preceding the terminator.
        int dataEnd = (endIndex >= 0)
                          ? endIndex
                          : buffer.size();

        // Every Diode result is exactly 6 ASCII bytes.
        while (dataEnd >= 6)
        {
            QByteArray resultBytes = buffer.left(6);

            QString rawResult =
                QString::fromLatin1(resultBytes);

            QString result;

            if (rawResult == "PASS_C")
                result = "PASS";
            else if (rawResult == "PASS_R")
                result = "REVERSE";
            else if (rawResult == "FAIL_S")
                result = "SHORT";
            else if (rawResult == "FAIL_O")
                result = "OPEN";
            else if (rawResult == "FAIL_F")
                result = "FAIL";
            else
            {
                // Do not silently interpret unknown data
                qDebug() << "Unknown Diode result:"
                         << resultBytes.toHex(' ').toUpper();

                emit executeWriteToNotes(
                    QString("Unknown Diode result: %1")
                        .arg(QString(
                            resultBytes.toHex(' ').toUpper())));

                // Discard this six-byte result field.
                buffer.remove(0, 6);
                dataEnd -= 6;
                continue;
            }

            const int row = m_diodeResultsReceived++;

            qDebug() << "Diode Result:"
                     << row + 1 << result;

            emit executeWriteToNotes(
                QString("Diode Result %1: %2")
                    .arg(row + 1)
                    .arg(result));

            emit diodeResultReceived(row, result);

            buffer.remove(0, 6);
            dataEnd -= 6;
        }

        // If the complete terminator has arrived, finish the test.
        // Any incomplete result before it remains unprocessed above.
        if (endIndex >= 0)
        {
            if (!buffer.left(endMarker.size()).startsWith(endMarker))
            {
                // Defensive check; normally the marker starts at byte 0
                // after all complete results have been consumed.
                return;
            }

            buffer.remove(0, endMarker.size());

            m_waitingForDiodeResults = false;

            qDebug() << "ABCDE received - Diode test completed";

            emit executeWriteToNotes(
                "Diode Result Transmission Completed");

            emit diodeResultsCompleted();

            return;
        }

        // Keep any incomplete result bytes for the next readyRead().
        return;
    }

    // =====================================================
    // INSULATION/ISOLATION RESULT RECEPTION
    // =====================================================

    if (m_receivingInsulationResults)
    {

        // --------------------------------------------------------
        // Check 24 24 24 24 failure condition
        // --------------------------------------------------------
        if (buffer.size() >= 4 &&
            quint8(buffer[0]) == 0x24 &&
            quint8(buffer[1]) == 0x24 &&
            quint8(buffer[2]) == 0x24 &&
            quint8(buffer[3]) == 0x24)
        {
            qDebug() << "Insulation/Isolation Test Failed:"
                     << "24 Condition Hits";

            emit executeWriteToNotes(
                "Insulation/Isolation Test Failed: 24 Condition Hits");

            abortCommand();

            buffer.clear();

            QMessageBox::critical(
                nullptr,
                "Insulation/Isolation Test Failed",
                "Test Failed: 24 Condition Hits"
            );

            return;
        }

        const QByteArray netMarker = QByteArray::fromHex("2F2F");
        const QByteArray loomEndMarker = QByteArray::fromHex("FF00");
        const QByteArray failedMarker = QByteArray::fromHex("2E2E");
        const QByteArray endMarker = "ABCDE";

        // Process every complete loom already in the buffer.
        while (true)
        {
            int loomEndIndex = buffer.indexOf(loomEndMarker);
            int testEndIndex = buffer.indexOf(endMarker);

            // Count markers only in the current loom's data.
            // Do not count bytes belonging to the final terminator.
            int dataEndIndex = buffer.size();

            if (loomEndIndex >= 0)
                dataEndIndex = loomEndIndex;

            if (testEndIndex >= 0 &&
                testEndIndex < dataEndIndex)
            {
                dataEndIndex = testEndIndex;
            }

            QByteArray currentLoomData = buffer.left(dataEndIndex);

            int markerCount = 0;
            int pos = 0;

            while ((pos = currentLoomData.indexOf(netMarker, pos)) >= 0)
            {
                markerCount++;
                pos += netMarker.size();
            }

            int totalNets =
                m_insulationNetsPerLoom.value(m_currentInsulationLoom, 0);

            qDebug() << "Loom data bytes:" << currentLoomData.size()
                     << "Marker pairs:" << markerCount
                     << "Expected nets:" << totalNets;

            while (m_insulationNetMarkersSeen < markerCount &&
                   m_insulationNetMarkersSeen < totalNets)
            {
                m_insulationNetMarkersSeen++;

                qDebug() << "Loom:" << m_currentInsulationLoom + 1
                         << "Net marker:" << m_insulationNetMarkersSeen
                         << "Total nets:" << totalNets;

                emit insulationNetMarkerReceived(
                    m_currentInsulationLoom + 1,
                    totalNets);
            }

            // --------------------------------------------------------
            // Process a complete loom BEFORE completing the test.
            // --------------------------------------------------------

            if (loomEndIndex >= 0 &&
                (testEndIndex < 0 || loomEndIndex < testEndIndex))
            {
                QByteArray loomData = buffer.left(loomEndIndex);

                // Remove loom data + FF 00.
                buffer.remove(
                    0,
                    loomEndIndex + loomEndMarker.size());

                bool loomFailed = loomData.contains(failedMarker);

                if (!loomFailed)
                {
                    qDebug() << "Insulation Loom"
                             << m_currentInsulationLoom + 1
                             << "PASS";

                    emit executeWriteToNotes(
                        QString("Insulation Loom %1 : PASS")
                        .arg(m_currentInsulationLoom + 1));

                    emit insulationLoomPassed(
                        m_currentInsulationLoom + 1);
                }
                else
                {
                    qDebug() << "Insulation Loom"
                             << m_currentInsulationLoom + 1
                             << "FAIL";

                    emit executeWriteToNotes(
                        QString("Insulation Loom %1 : FAIL")
                        .arg(m_currentInsulationLoom + 1));

                    emit insulationLoomFailed(
                        m_currentInsulationLoom + 1,
                        loomData);
                }

                qDebug() << "INSULATION LOOM:"
                         << m_currentInsulationLoom + 1;

                qDebug() << "LOOM DATA:"
                         << loomData.toHex(' ').toUpper();

                qDebug() << "LOOM DATA SIZE:"
                         << loomData.size() << "Bytes";

                emit executeWriteToNotes(
                    QString("Insulation Loom %1 Result: %2")
                    .arg(m_currentInsulationLoom + 1)
                    .arg(QString::fromLatin1(
                        loomData.toHex(' ').toUpper())));

                emit executeWriteToNotes(
                    QString("Insulation Loom %1 Result Size: %2 Bytes")
                    .arg(m_currentInsulationLoom + 1)
                    .arg(loomData.size()));

                // Move to next loom.
                m_currentInsulationLoom++;
                m_insulationNetMarkersSeen = 0;

                // Process any other complete loom already buffered.
                continue;
            }

            // --------------------------------------------------------
            // FINAL TEST TERMINATOR
            // --------------------------------------------------------

            if (testEndIndex >= 0)
            {
                buffer.remove(
                    0,
                    testEndIndex + endMarker.size());

                m_receivingInsulationResults = false;

                qDebug() << "ABCDE received - test completed";

                emit executeWriteToNotes("Test Completed");
                emit insulationResultsCompleted();

                return;
            }

            // No complete loom or final terminator is available yet.
            return;
        }
    }




    // =====================================================
    // ACK PROCESSING
    // =====================================================

    while (buffer.size() >= ack.size())
    {
        if (buffer.left(5) == ack)
        {
            buffer.remove(0, 5);

            emit executeWriteToNotes(
                        "ACK Received");

            // =================================================
            // ACK for TWO WIRE START Packet
            // =================================================

            if (m_waitingForStartAck)
            {
                m_waitingForStartAck = false;

                m_currentPacket = 0;

                if (!m_packets.isEmpty())
                {
                    serial->write(m_packets[0]);

                    emit executeWriteToNotes(
                                QString("TX Packet %1")
                                .arg(1));

                    emit executeWriteToNotes(
                                m_packets[0]
                                .toHex(' ')
                                .toUpper());

                    m_waitingForPacketAck = true;
                }

                continue;
            }

            // =================================================
            // INSULATION/ISOLATION ACK FOR START PACKET
            // =================================================

            if (m_waitingForInsulationStartAck)
            {
                m_waitingForInsulationStartAck = false;

                m_currentInsulationPacket = 0;

                if (!m_insulationPackets.isEmpty())
                {
                    serial->write(
                        m_insulationPackets[0]);

                    emit executeWriteToNotes(
                        QString("Insulation TX Packet %1")
                            .arg(1));

                    emit executeWriteToNotes(
                        m_insulationPackets[0]
                            .toHex(' ')
                            .toUpper());

                    m_waitingForInsulationPacketAck = true;
                }

                continue;
            }

            // =================================================
            // ACK for DIODE START packet
            // =================================================
            if (m_waitingForDiodeStartAck)
            {
                m_waitingForDiodeStartAck = false;
                m_currentDiodePacket = 0;

                if (!m_diodePackets.isEmpty())
                {
                    serial->write(m_diodePackets[0]);

                    emit executeWriteToNotes("Diode TX Packet 1");
                    emit executeWriteToNotes(
                        QString(m_diodePackets[0].toHex(' ').toUpper()));

                    m_waitingForDiodePacketAck = true;
                }

                continue;
            }


            // =================================================
            // ACK for RELAY START packet
            // =================================================
            if (m_waitingForRelayStartAck)
            {
                m_waitingForRelayStartAck = false;
                m_currentRelayPacket = 0;

                emit executeWriteToNotes(
                    "Relay START ACK received");

                if (!m_relayPackets.isEmpty())
                {
                    serial->write(m_relayPackets[0]);

                    emit executeWriteToNotes(
                        "Relay TX Packet 1");

                    emit executeWriteToNotes(
                        QString(m_relayPackets[0]
                                    .toHex(' ').toUpper()));

                    m_waitingForRelayPacketAck = true;
                }
                else
                {
                    emit executeWriteToNotes(
                        "Relay transmission failed: No data packets.");

                    m_waitingForRelayResults = false;
                }

                continue;
            }

            // =================================================
            // ACK for TWO WIRE DATA Packet
            // =================================================


            if (m_waitingForPacketAck)
            {
                m_currentPacket++;

                if (m_currentPacket < m_packets.size())
                {
                    serial->write(
                                m_packets[m_currentPacket]);

                    emit executeWriteToNotes(
                                QString("TX Packet %1")
                                .arg(m_currentPacket + 1));

                    emit executeWriteToNotes(
                                m_packets[m_currentPacket]
                                .toHex(' ')
                                .toUpper());
                }
                else
                {
                    m_waitingForPacketAck = false;

                    emit executeWriteToNotes(
                                "Two Wire Transmission Completed");

                    qDebug()
                            << "Transmission Complete";

                    // =============================================
                    // NOW EXPECT FLOAT RESULTS FROM CHT
                    // =============================================

                    m_receivingTwoWireResults = true;

                    qDebug()
                            << "Waiting for Two Wire float results...";

                    emit executeWriteToNotes(
                                "Waiting for Two Wire float results...");
                }

                continue;
            }

            // =================================================
            // ACK for INSULATION/ISOLATION DATA Packet
            // =================================================

            if (m_waitingForInsulationPacketAck)
            {
                m_currentInsulationPacket++;

                if (m_currentInsulationPacket <
                    m_insulationPackets.size())
                {
                    serial->write(
                        m_insulationPackets[
                            m_currentInsulationPacket]);

                    emit executeWriteToNotes(
                        QString("Insulation TX Packet %1")
                            .arg(m_currentInsulationPacket + 1));

                    emit executeWriteToNotes(
                        m_insulationPackets[
                            m_currentInsulationPacket]
                            .toHex(' ')
                            .toUpper());
                }
                else
                {
                    m_waitingForInsulationPacketAck = false;

                    emit executeWriteToNotes(
                        "Insulation Transmission Completed");

                    qDebug()
                        << "Insulation Transmission Complete";

                    // =============================================
                    // NOW EXPECT INSULATION RESULTS FROM CHT
                    // =============================================

                    m_receivingInsulationResults = true;

                    qDebug()
                        << "Waiting for Insulation results...";

                    emit executeWriteToNotes(
                        "Waiting for Insulation results...");
                }

                continue;
            }

            // =================================================
            // ACK for DIODE DATA packet
            // =================================================
            if (m_waitingForDiodePacketAck)
            {
                m_currentDiodePacket++;

                if (m_currentDiodePacket < m_diodePackets.size())
                {
                    // Send the next packet only after receiving ACK
                    serial->write(m_diodePackets[m_currentDiodePacket]);

                    emit executeWriteToNotes(
                        QString("Diode TX Packet %1")
                            .arg(m_currentDiodePacket + 1));

                    emit executeWriteToNotes(
                        QString(m_diodePackets[m_currentDiodePacket]
                                    .toHex(' ').toUpper()));
                }
                else
                {
                    // All Diode packets acknowledged
                    m_waitingForDiodePacketAck = false;
                    m_waitingForDiodeResults = true;

                    emit executeWriteToNotes(
                        "Diode Transmission Completed");

                    emit executeWriteToNotes(
                        "Waiting for Diode results...");

                    qDebug() << "Diode transmission complete;"
                                " waiting for results.";
                }

                continue;
            }


            // =================================================
            // ACK for RELAY DATA packet
            // =================================================
            if (m_waitingForRelayPacketAck)
            {
                m_currentRelayPacket++;

                if (m_currentRelayPacket < m_relayPackets.size())
                {
                    // Send the next packet after receiving ACK.
                    serial->write(
                        m_relayPackets[m_currentRelayPacket]);

                    emit executeWriteToNotes(
                        QString("Relay TX Packet %1")
                            .arg(m_currentRelayPacket + 1));

                    emit executeWriteToNotes(
                        QString(
                            m_relayPackets[m_currentRelayPacket]
                                .toHex(' ').toUpper()));
                }
                else
                {
                    // All Relay packets acknowledged.
                    m_waitingForRelayPacketAck = false;
                    m_waitingForRelayResults = true;

                    emit executeWriteToNotes(
                        "Relay Transmission Completed");

                    emit executeWriteToNotes(
                        "Waiting for Relay results...");

                    qDebug()
                        << "Relay transmission complete;"
                        << "waiting for results.";
                }

                continue;
            }

        }
        else
        {
            // -------------------------------------------------
            // We are still in ACK mode.
            // Discard byte until ACK is found.
            // -------------------------------------------------

            buffer.remove(0, 1);
        }
    }
    //Ack While loop End
}
//Ready Read End

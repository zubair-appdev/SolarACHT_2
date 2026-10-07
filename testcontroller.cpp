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

    // Test Type
    packet.append(char(0x3F));

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

    // Clear received data
    buffer.clear();
    resultsOfSelfTestBytes.clear();
    m_twoWireResults.clear();

    emit executeWriteToNotes(
                "Test Aborted");
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
    // INSULATION RESULT RECEPTION
    // =====================================================

    if (m_receivingInsulationResults)
    {
        const QByteArray endMarker = "ABCDE";

        // --------------------------------------------------------
        // Wait until complete result data + ABCDE is received
        // --------------------------------------------------------

        int endIndex =
                buffer.indexOf(endMarker);

        if (endIndex < 0)
        {
            // ABCDE not received yet.
            // Keep everything in buffer.
            return;
        }

        // --------------------------------------------------------
        // Complete Insulation result data received
        // --------------------------------------------------------

        QByteArray resultData =
                buffer.left(endIndex);

        // Remove result data + ABCDE from buffer
        buffer.remove(
            0,
            endIndex + endMarker.size());

        // --------------------------------------------------------
        // Print raw result data
        // --------------------------------------------------------

        qDebug()
                << "INSULATION RESULT DATA:"
                << resultData.toHex(' ').toUpper();

        qDebug()
                << "INSULATION RESULT SIZE:"
                << resultData.size()
                << "Bytes";

        emit executeWriteToNotes(
            QString(
                "Insulation Result Data: %1")
            .arg(QString::fromLatin1(
                resultData.toHex(' ').toUpper())));

        emit executeWriteToNotes(
            QString(
                "Insulation Result Size: %1 Bytes")
            .arg(resultData.size()));

        // --------------------------------------------------------
        // ABCDE received
        // --------------------------------------------------------

        qDebug()
                << "ABCDE received - "
                   "Insulation result reception complete";

        emit executeWriteToNotes(
            "Insulation Result Transmission Completed");

        m_receivingInsulationResults = false;

        //emit insulationResultsCompleted(); continue from here .....

        return;
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
            // INSULATION ACK FOR START PACKET
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
            // ACK for INSULATION DATA Packet
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
}

#include "testcontroller.h"
#include <QDebug>
#include "loginclass.h"

TestController::TestController(QObject *parent)
    : QObject(parent)
{
    connect(&serial, &QSerialPort::readyRead,
            this, &TestController::onReadyRead);
}

TestController::~TestController()
{
    if (serial.isOpen())
        serial.close();
}

QStringList TestController::availablePorts()
{
    QStringList ports;
    ports << "Port";   // default placeholder

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ports << info.portName();

    return ports;
}

bool TestController::connectPort(const QString &portName)
{
    if (portName == "Port")
        return false;

    if (serial.isOpen())
        serial.close();

    serial.setPortName(portName);
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadWrite)) {
        emit portStatusChanged(false, "Failed to open port");
        return false;
    }

    emit portStatusChanged(true, "Port connected: " + portName);
    return true;
}

void TestController::disconnectPort()
{
    if (serial.isOpen()) {
        serial.close();
        emit portStatusChanged(false, "Port disconnected");
    }
}

bool TestController::isConnected() const
{
    return serial.isOpen();
}

void TestController::setLogger(LoginClass *logger)
{
    login = logger;
}


bool TestController::mapLogicalToHardware(const QVariantList &patchData,
                                          const QVariantList &harnessData)
{
    if (!login)
        return false;

    login->writeToNotes("================================================");
    login->writeToNotes("STEP-1 : Mapping Logical Connectors → Hardware Pins");
    login->writeToNotes("================================================");

    // Clear previous mappings
    k_sourceCon.clear();
    k_sourcePin.clear();
    k_destinationCon.clear();
    k_destinationPin.clear();

    // -------- Extract PATCH data --------
    QVector<QString> userCon, userPin, patchCon, patchPin;

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

    login->writeToNotes(QString("Patch rows loaded : %1").arg(userCon.size()));

    // -------- Extract HARNESS data --------
    QVector<QString> sourceCon, sourcePin, destCon, destPin, exp;

    for (const QVariant &v : harnessData) {
        QVariantMap m = v.toMap();
        sourceCon.append(m["sourceCon"].toString());
        sourcePin.append(m["sourcePin"].toString());
        destCon.append(m["destCon"].toString());
        destPin.append(m["destPin"].toString());
        exp.append(m["exp"].toString());
    }

    this->sourceCon = sourceCon;
    this->sourcePin = sourcePin;
    this->destCon = destCon;
    this->destPin = destPin;
    this->exp = exp;

    login->writeToNotes(QString("Harness rows loaded : %1").arg(sourceCon.size()));

    // -------- Mapping process --------
    bool mappingOk = true;
    QString errorLog;

    for (int i = 0; i < sourceCon.size(); ++i)
    {
        bool srcFound = false;
        bool dstFound = false;

        for (int j = 0; j < userCon.size(); ++j)
        {
            // Source mapping
            if (sourceCon[i] == userCon[j] &&
                sourcePin[i] == userPin[j])
            {
                k_sourceCon.append(patchCon[j]);
                k_sourcePin.append(patchPin[j]);
                srcFound = true;
            }

            // Destination mapping
            if (destCon[i] == userCon[j] &&
                destPin[i] == userPin[j])
            {
                k_destinationCon.append(patchCon[j]);
                k_destinationPin.append(patchPin[j]);
                dstFound = true;
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
        login->writeToNotes("❌ Mapping FAILED");
        login->writeToNotes(errorLog);
        return false;
    }

    // -------- Log successful mappings --------
    login->writeToNotes(QString("✔ Mapping SUCCESS | Total Connections: %1")
                            .arg(k_sourceCon.size()));

    for (int i = 0; i < k_sourceCon.size(); ++i)
    {
        login->writeToNotes(
            QString("MAP %1 : %2-%3  →  %4-%5")
                .arg(i + 1)
                .arg(k_sourceCon[i])
                .arg(k_sourcePin[i])
                .arg(k_destinationCon[i])
                .arg(k_destinationPin[i]));
    }

    login->writeToNotes("STEP-1 COMPLETED SUCCESSFULLY");
    return true;
}

QByteArray TestController::constructUARTPacketForTwoWire(
    const QVector<QString> &sourceCon,
    const QVector<QString> &sourcePin,
    const QVector<QString> &destCon,
    const QVector<QString> &destPin)
{
    QByteArray packet;

    packet.append(static_cast<char>(0x2F));
    packet.append(static_cast<char>(0x2F));
    packet.append(static_cast<char>(0xCC));

    for (int i = 0; i < sourceCon.size(); ++i) {
        uint8_t srcCon = sourceCon[i].mid(2).toUInt();
        uint8_t srcPin = sourcePin[i].toUInt();
        uint8_t dstCon = destCon[i].mid(2).toUInt();
        uint8_t dstPin = destPin[i].toUInt();

        packet.append(static_cast<char>(srcCon));
        packet.append(static_cast<char>(srcPin));
        packet.append(static_cast<char>(dstCon));
        packet.append(static_cast<char>(dstPin));
    }

    return packet;
}


bool TestController::runTwoWireContinuity(QString simulate)
{
    if (!serial.isOpen()) {
        login->writeToNotes("Two Wire: Serial port not open");
        return false;
    }

    qDebug()<<simulate<<" :simulate from QML to Qt";
    if(simulate == "Go")
    {
        m_simulate = "Go";
    }
    else
    {
        m_simulate = "No";
    }

    rxBuffer.clear();
    twoWireInProgress = true;
    activeTest = ActiveTest::TwoWire;

    login->writeToNotes("Starting Two Wire Continuity (event-driven)");

    QByteArray packet = constructUARTPacketForTwoWire(
        k_sourceCon,
        k_sourcePin,
        k_destinationCon,
        k_destinationPin);

    login->writeToNotes("Two Wire TX : " +
                        packet.toHex(' ').toUpper());

    serial.write(packet);
    return true;   // IMPORTANT: result comes via signal
}

void TestController::startTwoWireRun()
{
    if (!serial.isOpen()) {
        login->writeToNotes("❌ Cannot start Two Wire: port not open");
        return;
    }

    // Reuse existing buffer + flag
    rxBuffer.clear();
    twoWireInProgress = true;
    activeTest = ActiveTest::TwoWire;

    QByteArray cmd;
    cmd.append(char(0x5A));
    cmd.append(char(0x4B));
    cmd.append(char(0x81));

    login->writeToNotes("Two Wire RUN TX : " +
                        cmd.toHex(' ').toUpper());

    serial.write(cmd);
}

void TestController::generateTwoWirePDF(
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
    const QString &inspectedBy)
{
    // ------------------ FILE PATH ------------------
    QString outputDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + "/SOLAR_CHT_DOCS";
    QDir().mkpath(outputDir);

    QString filePath =
        outputDir + "/TwoWireContinuity_" +
        QDateTime::currentDateTime().toString("ddMMyyyy_hhmmss") + ".pdf";

    // ------------------ PRINTER ------------------
    QPrinter printer(QPrinter::PrinterResolution);
    printer.setResolution(96);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPainter painter(&printer);
    if (!painter.isActive()) return;

    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF pageRect = printer.pageLayout().paintRect();

    // ------------------ RATIO-BASED LAYOUT ------------------
    qreal margin = pageRect.width() * 0.07;
    qreal x = margin;
    qreal y = margin;

    qreal usableWidth = pageRect.width() - 2 * margin;
    qreal pageBottom  = pageRect.bottom() - margin;

    // ------------------ FONTS ------------------
    QFont titleFont("Helvetica", pageRect.width() * 0.025, QFont::Bold);
    QFont headerFont("Helvetica", pageRect.width() * 0.016, QFont::Bold);
    QFont textFont("Helvetica", pageRect.width() * 0.015);

    QFontMetricsF textFM(textFont);

    // ------------------ HEADER ------------------
    painter.setFont(titleFont);
    painter.drawText(QRectF(x, y, usableWidth, titleFont.pointSizeF() * 2),
                     Qt::AlignCenter,
                     "TWO WIRE CONTINUITY TEST REPORT");
    y += titleFont.pointSizeF() * 2.8;

    painter.setFont(headerFont);
    painter.drawText(x, y, "Project Name : " + projectName); y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Set No       : " + setNo);        y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Test Type    : " + testToRun);   y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Date         : " +
                               QDateTime::currentDateTime().toString("dd-MM-yyyy")); y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Time         : " +
                               QDateTime::currentDateTime().toString("hh:mm:ss"));
    y += headerFont.pointSizeF() * 2.5;

    QString finalResult = results.contains("Fail") ? "FAIL" : "PASS";
    painter.drawText(x, y, "Final Result : " + finalResult);
    y += headerFont.pointSizeF() * 2.5;

    // ------------------ TABLE SETUP ------------------
    QStringList headers = {
        "S.No", "Src Conn", "Src Pin",
        "Dst Conn", "Dst Pin",
        "Expected", "Measured", "Result"
    };

    QList<qreal> colRatio = {
        0.07,  // S.No
        0.33,  // Src Conn
        0.15,  // Src Pin
        0.33,  // Dst Conn
        0.15,  // Dst Pin
        0.10,  // Expected
        0.13,  // Measured
        0.12   // Result
    };

    QList<qreal> colWidth;
    for (qreal r : colRatio)
        colWidth.append(usableWidth * r);

    // ------------------ UNIFORM ROW HEIGHT (GLOBAL MAX) ------------------
    qreal baseRowHeight = textFM.height() * 1.6;

    for (int i = 0; i < sourceCon.size(); ++i)
    {
        QStringList row = {
            QString::number(i + 1),
            sourceCon[i],
            sourcePin[i],
            destinationCon[i],
            destinationPin[i],
            expectedValues[i],
            measuredValues[i],
            results[i]
        };

        for (int c = 0; c < row.size(); ++c) {
            QRectF textRect(0, 0, colWidth[c] - 8, 3000);
            qreal h = textFM.boundingRect(
                                textRect,
                                Qt::TextWordWrap,
                                row[c]).height() + 8;
            baseRowHeight = qMax(baseRowHeight, h);
        }
    }

    int pageNo = 1;
    int totalFails = 0;

    auto drawHeader = [&]() {
        painter.setFont(headerFont);
        painter.setBrush(QColor("#E6E6E6"));

        qreal xPos = x;
        for (int i = 0; i < headers.size(); ++i) {
            painter.drawRect(QRectF(xPos, y, colWidth[i], baseRowHeight));
            painter.drawText(QRectF(xPos, y, colWidth[i], baseRowHeight),
                             Qt::AlignCenter, headers[i]);
            xPos += colWidth[i];
        }
        painter.setBrush(Qt::NoBrush);
        painter.setFont(textFont);
        y += baseRowHeight;
    };

    drawHeader();

    // ------------------ TABLE DATA ------------------
    for (int i = 0; i < sourceCon.size(); ++i)
    {
        if (y + baseRowHeight > pageBottom) {
            painter.drawText(QRectF(x, pageBottom + 5, usableWidth, 12),
                             Qt::AlignCenter,
                             "Page " + QString::number(pageNo++));
            printer.newPage();
            y = margin;
            drawHeader();
        }

        QStringList row = {
            QString::number(i + 1),
            sourceCon[i],
            sourcePin[i],
            destinationCon[i],
            destinationPin[i],
            expectedValues[i],
            measuredValues[i],
            results[i]
        };

        if (results[i] == "Fail")
            totalFails++;

        if (i % 2 == 0)
            painter.setBrush(QColor("#FAFAFA"));

        qreal xPos = x;
        for (int c = 0; c < row.size(); ++c)
        {
            QRectF cell(xPos, y, colWidth[c], baseRowHeight);

            if (c == 7) {
                painter.setBrush(row[c] == "Pass"
                                     ? QColor("#A8E6A3")
                                     : QColor("#F4A6A6"));
            }

            painter.drawRect(cell);

            Qt::Alignment align =
                (c == 0 || c == 2 || c == 4 || c >= 5)
                    ? Qt::AlignCenter
                    : Qt::AlignLeft | Qt::AlignVCenter;

            painter.drawText(
                cell.adjusted(4, 3, -4, -3),
                align | Qt::TextWordWrap,
                row[c]);

            painter.setBrush(Qt::NoBrush);
            xPos += colWidth[c];
        }

        y += baseRowHeight;
    }

    y += baseRowHeight;

    // ------------------ FOOTER (SAFE & FLEXIBLE) ------------------
    QStringList footerLines = {
        "Test Performed By : " + performedBy,
        "Inspected By      : " + inspectedBy,
        "Total Failed Pins : " + QString::number(totalFails)
    };

    qreal footerHeight = footerLines.size() * baseRowHeight;

    if (y + footerHeight > pageBottom) {
        printer.newPage();
        y = margin;
    }

    painter.setFont(headerFont);
    for (const QString &line : footerLines) {
        painter.drawText(x, y, line);
        y += baseRowHeight;
    }

    painter.end();
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

bool TestController::runIsolation(const QString &simulate)
{
    login->writeToNotes("==============================================");
    login->writeToNotes("ISOLATION TEST STARTED");
    login->writeToNotes("==============================================");

    buildNetData();
    allUARTpacket.clear();

    for (const auto &net : allNetLists) {
        QVector<QByteArray> packetGroup;
        packetGroup.append(buildUARTPacket(net));
        allUARTpacket.append(packetGroup);
    }

    activeTest = ActiveTest::Isolation;
    isoState = IsoState::Idle;
    currentPacketIndex = 0;

    sendNextIsoPacket(simulate);
    return true;
}

void TestController::sendNextIsoPacket(const QString &simulate)
{
    // ================= TEST COMPLETED =================
    if (currentPacketIndex >= allUARTpacket.size())
    {
        login->writeToNotes("✔ Isolation / Insulation Test Completed");

        // -------- PDF GENERATION (ONLY ONCE) --------
        if (activeTest == ActiveTest::Isolation)
        {
            if (!isolationResult.isEmpty() &&
                isolationResult.contains("##ISO##"))
            {
                generateIsoInsuPdf(isolationResult, "ISOLATION");
            }
        }
        else if (activeTest == ActiveTest::Insulation)
        {
            if (!insulationResult.isEmpty() &&
                insulationResult.contains("##INSO##"))
            {
                generateIsoInsuPdf(insulationResult, "INSULATION");
            }
        }

        emit twoWireFinished(true, "Test Completed");

        activeTest = ActiveTest::None;
        return;
    }

    // ================= SEND NEXT NET =================
    rxBuffer.clear();

    const QVector<QByteArray> &packetGroup =
        allUARTpacket[currentPacketIndex];

    for (const QByteArray &segment : packetGroup)
    {
        login->writeToNotes(
            "TX NET : " + segment.toHex(' ').toUpper());

        if (simulate != "Go")
            serial.write(segment);
    }

    isoState = IsoState::WaitingEndAfterNet;
}


void TestController::handleIsoInsuRx()
{
    // =====================================================
    // 1️⃣ ZERO-BYTE (00000) HARDWARE DEAD DETECTION
    // =====================================================
    for (int i = 0; i <= rxBuffer.size() - 5; ++i) {
        if (rxBuffer.mid(i, 5) == QByteArray(5, 0x00)) {
            if (!zeroErrorShown) {
                zeroErrorShown = true;
                login->writeToNotes("❌ Hardware not responding (00000 detected)");
                abortIsoInsuTest("Hardware Not Responding");
            }
            return;
        }
    }

    // =====================================================
    // 2️⃣ BUNCH FAIL DETECTION
    // =====================================================
    static const QByteArray BUNCH_FAIL =
        QByteArray::fromHex("4B 53 42 5F 42 75 6E 63 68 5F 46 61 69 6C"); // KSB_Bunch_Fail

    if (rxBuffer.contains(BUNCH_FAIL)) {
        login->writeToNotes("❌ BUNCH FAIL detected from hardware");
        abortIsoInsuTest("Bunch Fail");
        return;
    }

    // =====================================================
    // 3️⃣ FIRST END (after NET data)
    // =====================================================
    if (isoState == IsoState::WaitingEndAfterNet &&
        rxBuffer.contains("END")) {

        login->writeToNotes("✔ END received after NET data");

        QByteArray testCmd = buildIsoInsuCommand();
        serial.write(testCmd);

        login->writeToNotes("TX CMD : " +
                            testCmd.toHex(' ').toUpper());

        rxBuffer.clear();
        isoState = IsoState::WaitingEndAfterCmd;
        return;
    }

    // =====================================================
    // 4️⃣ SECOND END (after TEST command)
    // =====================================================
    if (isoState == IsoState::WaitingEndAfterCmd &&
        rxBuffer.contains("END")) {

        login->writeToNotes("✔ END received after TEST command");

        rxBuffer.clear();
        isoState = IsoState::WaitingFinalResult;
        return;
    }

    // =====================================================
    // 5️⃣ FINAL RESULT (ABCDEB / ABCDEC)
    // =====================================================
    if (isoState == IsoState::WaitingFinalResult &&
        (rxBuffer.endsWith("ABCDEB") ||
         rxBuffer.endsWith("ABCDEC"))) {

        login->writeToNotes("✔ Final ISO/INSU result received");

        extractAndStoreResult(rxBuffer);

        rxBuffer.clear();
        isoState = IsoState::Idle;
        currentPacketIndex++;

        // Move to next NET after delay
        QTimer::singleShot(5000, this, [this]() {
            sendNextIsoPacket("No");
        });

        return;
    }
}


QByteArray TestController::buildIsoInsuCommand()
{
    QByteArray testCommand;

    login->writeToNotes("----- buildIsoInsuCommand() ENTER -----");

    login->writeToNotes(
        QString("currentPacketIndex = %1")
            .arg(currentPacketIndex));

    login->writeToNotes(
        QString("totalNetsEachPacket.size() = %1")
            .arg(totalNetsEachPacket.size()));

    login->writeToNotes(
        QString("voltageEachPacket.size() = %1")
            .arg(voltageEachPacket.size()));

    login->writeToNotes(
        QString("allUARTpacket.size() = %1")
            .arg(allUARTpacket.size()));

    // Header
    testCommand.append(char(0x5A));
    testCommand.append(char(0x4B));

    // Command ID
    if (activeTest == ActiveTest::Isolation) {
        testCommand.append(char(0x82));
        login->writeToNotes("Command = ISOLATION (0x82)");
    } else {
        testCommand.append(char(0x83));
        login->writeToNotes("Command = INSULATION (0x83)");
    }

    // Fixed bytes
    testCommand.append(char(0x00));
    testCommand.append(char(0x01));

    // 🔴 CRASH ZONE — LOG BEFORE ACCESS
    login->writeToNotes("About to read totalNetsEachPacket[currentPacketIndex]");
    quint16 netCount =
        static_cast<quint16>(totalNetsEachPacket[currentPacketIndex]);

    login->writeToNotes(
        QString("netCount = %1").arg(netCount));

    testCommand.append(char((netCount >> 8) & 0xFF));
    testCommand.append(char(netCount & 0xFF));

    // 🔴 CRASH ZONE — LOG BEFORE ACCESS
    login->writeToNotes("About to read voltageEachPacket[currentPacketIndex]");
    quint8 voltage =
        static_cast<quint8>(voltageEachPacket[currentPacketIndex]);

    login->writeToNotes(
        QString("voltage = %1").arg(voltage));

    testCommand.append(char(voltage));

    login->writeToNotes(
        "ISO/INS CMD TX : " +
        testCommand.toHex(' ').toUpper());

    login->writeToNotes("----- buildIsoInsuCommand() EXIT -----");

    return testCommand;
}



void TestController::extractAndStoreResult(const QByteArray &buffer)
{
    const QByteArray HASH = QByteArray::fromHex("23232323");   // ####
    const QByteArray ISO_END = QByteArray::fromHex("414243444542"); // ABCDEB
    const QByteArray INSO_END = QByteArray::fromHex("414243444543"); // ABCDEC

    int hashIndex = buffer.indexOf(HASH);

    if (hashIndex == -1) {
        login->writeToNotes("⚠ #### marker not found in RX buffer");
        return;
    }

    int startIndex = hashIndex + HASH.size();
    int endIndex = -1;

    // Determine end marker based on active test
    if (activeTest == ActiveTest::Isolation) {
        endIndex = buffer.indexOf(ISO_END);
    } else if (activeTest == ActiveTest::Insulation) {
        endIndex = buffer.indexOf(INSO_END);
    }

    if (endIndex == -1 || endIndex <= startIndex) {
        login->writeToNotes("⚠ End marker not found or invalid in RX buffer");
        return;
    }

    int length = endIndex - startIndex;
    QByteArray payload = buffer.mid(startIndex, length);

    // Store result
    if (activeTest == ActiveTest::Isolation) {
        isolationResult.append("##ISO##");
        isolationResult.append(payload);

        login->writeToNotes(
            "✔ Isolation result appended : " +
            payload.toHex(' ').toUpper()
            );
    }
    else if (activeTest == ActiveTest::Insulation) {
        insulationResult.append("##INSO##");
        insulationResult.append(payload);

        login->writeToNotes(
            "✔ Insulation result appended : " +
            payload.toHex(' ').toUpper()
            );
    }
}

void TestController::abortIsoInsuTest(const QString &reason)
{
    login->writeToNotes("❌ TEST ABORTED : " + reason);

    isoState = IsoState::Idle;
    activeTest = ActiveTest::None;
    currentPacketIndex = 0;

    rxBuffer.clear();

    emit twoWireFinished(false, reason);
}


bool TestController::runInsulation(const QString &simulate)
{
    login->writeToNotes("==============================================");
    login->writeToNotes("INSULATION TEST STARTED");
    login->writeToNotes("==============================================");

    buildNetData();

    for (const auto &net : allNetLists) {
        QByteArray pkt = buildUARTPacket(net);

        login->writeToNotes("Insulation TX : " + pkt.toHex(' ').toUpper());

        if (simulate != "Go") {
            serial.write(pkt);
        }

        QVector<QByteArray> packetGroup;
        packetGroup.append(pkt);
        allUARTpacket.append(packetGroup);
    }

    return true;
}

void TestController::prepareFixedGroupsForPDF()
{
    QVector<QVector<QVector<QPair<QString, QString>>>> localChangedAllFixedGroups;

    for (const auto &packet : allFixedGroups)
    {
        QVector<QVector<QPair<QString, QString>>> changedFixedGroups;

        for (const auto &group : packet)
        {
            QVector<QPair<QString, QString>> changedGroup;

            for (const auto &pair : group)
            {
                QString patchName = pair.first;
                int patchNumber   = pair.second;

                bool matchFound = false;

                for (int i = 0; i < patchCon.size(); ++i)
                {
                    if (patchCon[i] == patchName &&
                        patchPin[i].toInt() == patchNumber)
                    {
                        // Replace TPx,pin → UserCon,UserPin
                        changedGroup.append(
                            qMakePair(userCon[i], userPin[i])
                            );
                        matchFound = true;
                        break;
                    }
                }

                // fallback (should rarely happen)
                if (!matchFound)
                {
                    changedGroup.append(
                        qMakePair(patchName,
                                  QString::number(patchNumber))
                        );
                }
            }

            changedFixedGroups.append(changedGroup);
        }

        localChangedAllFixedGroups.append(changedFixedGroups);
    }

    // Store into member
    this->changedAllFixedGroups = localChangedAllFixedGroups;

    // ---- LOG (instead of qDebug) ----
    login->writeToNotes("PDF Mapping: changedAllFixedGroups prepared");

    for (int pkt = 0; pkt < changedAllFixedGroups.size(); ++pkt)
    {
        login->writeToNotes(
            QString("Packet %1").arg(pkt + 1));

        const auto &groups = changedAllFixedGroups[pkt];
        for (int grp = 0; grp < groups.size(); ++grp)
        {
            login->writeToNotes(
                QString("  Net %1").arg(grp + 1));

            for (const auto &p : groups[grp])
            {
                login->writeToNotes(
                    QString("    (%1 , %2)")
                        .arg(p.first, p.second));
            }
        }
    }
}


void TestController::generateIsoInsuPdf(
    const QByteArray &resultData,
    const QString &testName)
{
    // ================= FILE PATH =================
    QString outputDir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + "/SOLAR_CHT_DOCS";
    QDir().mkpath(outputDir);

    QString filePath =
        outputDir + "/" + testName + "_TEST_REPORT_" +
        QDateTime::currentDateTime().toString("ddMMyyyy_hhmmss") + ".pdf";

    // ================= PRINTER =================
    QPrinter printer(QPrinter::PrinterResolution);
    printer.setResolution(96);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPainter painter(&printer);
    if (!painter.isActive())
        return;

    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF pageRect = printer.pageLayout().paintRect();

    // ================= LAYOUT =================
    qreal margin = pageRect.width() * 0.07;
    qreal x = margin;
    qreal y = margin;

    qreal usableWidth = pageRect.width() - 2 * margin;
    qreal pageBottom  = pageRect.bottom() - margin;

    // ================= FONTS =================
    QFont titleFont("Helvetica", pageRect.width() * 0.025, QFont::Bold);
    QFont headerFont("Helvetica", pageRect.width() * 0.016, QFont::Bold);
    QFont textFont("Helvetica", pageRect.width() * 0.015);

    QFontMetricsF textFM(textFont);

    // ================= HEADER =================
    painter.setFont(titleFont);
    painter.drawText(
        QRectF(x, y, usableWidth, titleFont.pointSizeF() * 2),
        Qt::AlignCenter,
        testName + " TEST REPORT");

    y += titleFont.pointSizeF() * 2.8;

    painter.setFont(headerFont);
    painter.drawText(x, y, "Project Name : " + login->getProjectName()); y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Set No       : " + login->getSetNo());        y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Test Type    : " + login->getTestType());     y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Date         : " +
                               QDateTime::currentDateTime().toString("dd-MM-yyyy")); y += headerFont.pointSizeF() * 1.8;
    painter.drawText(x, y, "Time         : " +
                               QDateTime::currentDateTime().toString("hh:mm:ss"));

    y += headerFont.pointSizeF() * 2.5;

    // ================= RESULT DECODING =================
    auto extractFailedNets =
        [](const QByteArray &data, const QByteArray &marker)
        -> QList<QVector<uint16_t>>
    {
        QList<QVector<uint16_t>> all;
        QByteArray ff00 = QByteArray::fromHex("FF00");

        int pos = 0;
        while ((pos = data.indexOf(marker, pos)) != -1) {
            int start = pos + marker.size();
            int end = data.indexOf(marker, start);
            if (end == -1)
                end = data.size();

            QByteArray block = data.mid(start, end - start);
            int cut = block.indexOf(ff00);

            QVector<uint16_t> nets;
            if (cut != -1) {
                QByteArray payload = block.left(cut);
                for (int i = 0; i + 3 < payload.size(); i += 4) {
                    uint16_t a = quint8(payload[i]) |
                                 (quint8(payload[i + 1]) << 8);
                    uint16_t b = quint8(payload[i + 2]) |
                                 (quint8(payload[i + 3]) << 8);
                    if (b < a) std::swap(a, b);
                    nets.append(a);
                    nets.append(b);
                }
            }
            all.append(nets);
            pos = end;
        }
        return all;
    };

    QList<QVector<uint16_t>> failedNets =
        extractFailedNets(
            resultData,
            testName == "ISOLATION" ? QByteArray("##ISO##")
                                    : QByteArray("##INSO##"));

    bool hasFailure = false;
    for (const auto &v : failedNets)
        if (!v.isEmpty()) { hasFailure = true; break; }

    painter.drawText(
        x, y,
        "Final Result : " + QString(hasFailure ? "FAIL" : "PASS"));

    y += headerFont.pointSizeF() * 2.5;

    prepareFixedGroupsForPDF();

    // ================= TABLE SETUP =================
    QStringList headers = {
        "Net\nNo",
        "Source Net",
        "Destination Net",
        "Fail Net",
        "Expected",
        "Result",
        "Remarks"
    };

    QList<qreal> colRatio = { 0.08, 0.38, 0.14, 0.10, 0.10, 0.10, 0.10 };
    QList<qreal> colWidth;
    for (qreal r : colRatio)
        colWidth.append(usableWidth * r);

    qreal baseRowHeight = textFM.height() * 1.6;
    int pageNo = 1;

    auto drawHeader = [&]() {
        painter.setFont(headerFont);
        painter.setBrush(QColor("#E6E6E6"));

        qreal xPos = x;
        for (int i = 0; i < headers.size(); ++i) {
            painter.drawRect(QRectF(xPos, y, colWidth[i], baseRowHeight));
            painter.drawText(
                QRectF(xPos, y, colWidth[i], baseRowHeight),
                Qt::AlignCenter | Qt::TextWordWrap,
                headers[i]);
            xPos += colWidth[i];
        }
        painter.setBrush(Qt::NoBrush);
        painter.setFont(textFont);
        y += baseRowHeight;
    };

    drawHeader();

    // ================= TABLE DATA =================
    for (int pkt = 0; pkt < changedAllFixedGroups.size(); ++pkt)
    {
        const auto &groups = changedAllFixedGroups[pkt];
        QVector<uint16_t> pktFails =
            pkt < failedNets.size() ? failedNets[pkt]
                                    : QVector<uint16_t>();

        for (int netIdx = 0; netIdx < groups.size(); ++netIdx)
        {
            if (y + baseRowHeight > pageBottom) {
                painter.drawText(
                    QRectF(x, pageBottom + 5, usableWidth, 12),
                    Qt::AlignCenter,
                    "Page " + QString::number(pageNo++));
                printer.newPage();
                y = margin;
                drawHeader();
            }

            const auto &group = groups[netIdx];

            QString srcNet;
            for (const auto &p : group)
                srcNet += "( \"" + p.first + "\" , \"" + p.second + "\" )\n";

            QString dstNet = (netIdx + 1 >= groups.size())
                                 ? "-"
                                 : QString::number(netIdx + 2) + "...";

            QString failNet;
            for (int i = 0; i + 1 < pktFails.size(); i += 2)
                if (pktFails[i] == netIdx + 1)
                    failNet += QString::number(pktFails[i + 1]) + "\n";

            failNet = failNet.trimmed();
            QString testResult = failNet.isEmpty() ? "Pass" : "Fail";
            if (failNet.isEmpty()) failNet = "-";

            QStringList row = {
                QString::number(netIdx + 1),
                srcNet.trimmed(),
                dstNet,
                failNet,
                ">20MΩ",
                testResult,
                ""
            };

            qreal xPos = x;
            for (int c = 0; c < row.size(); ++c)
            {
                QRectF cell(xPos, y, colWidth[c], baseRowHeight);

                if (c == 5) {
                    painter.setBrush(
                        testResult == "Fail"
                            ? QColor("#F4A6A6")
                            : QColor("#A8E6A3"));
                }

                painter.drawRect(cell);
                painter.drawText(
                    cell.adjusted(4, 3, -4, -3),
                    Qt::AlignCenter | Qt::TextWordWrap,
                    row[c]);

                painter.setBrush(Qt::NoBrush);
                xPos += colWidth[c];
            }
            y += baseRowHeight;
        }
    }

    // ================= FOOTER =================
    y += baseRowHeight;
    painter.setFont(headerFont);

    painter.drawText(x, y,
                     "Test Performed By : " + login->getPerformedBy());
    y += baseRowHeight;

    painter.drawText(x, y,
                     "Inspected By      : " + login->getInspectedBy());

    painter.end();
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}



void TestController::onReadyRead()
{
    QByteArray data = serial.readAll();
    rxBuffer.append(data);

    login->writeToNotes("RX chunk : " +
                        data.toHex(' ').toUpper());

    // ================= TWO WIRE (UNCHANGED) =================
    if (activeTest == ActiveTest::TwoWire) {

        if (rxBuffer.contains("END")) {
            login->writeToNotes("✔ Two Wire ACK received (END)");
            rxBuffer.clear();
            startTwoWireRun();
            return;
        }

        if (rxBuffer.contains("ABCDEA")) {
            twoWireInProgress = false;
            activeTest = ActiveTest::None;

            login->writeToNotes("✔ Two Wire Test Completed (ABCDEA)");

            QByteArray finalData = rxBuffer;
            rxBuffer.clear();
            processTwoWireResult(finalData);
            return;
        }

        return;
    }

    // ================= ISOLATION / INSULATION =================
    if (activeTest == ActiveTest::Isolation ||
        activeTest == ActiveTest::Insulation) {

        handleIsoInsuRx();
        return;
    }
}

void TestController::buildNetData()
{
    login->writeToNotes("==============================================");
    login->writeToNotes("STEP-2 : NET CREATION (Isolation / Insulation)");
    login->writeToNotes("==============================================");

    allNetLists.clear();
    allFixedGroups.clear();

    // -------- Step 1: Build source-destination pairs --------
    QVector<QPair<QPair<QString, int>, QPair<QString, int>>> pairs;

    for (int i = 0; i < k_sourceCon.size(); ++i) {
        pairs.append(qMakePair(
            qMakePair(k_sourceCon[i], k_sourcePin[i].toInt()),
            qMakePair(k_destinationCon[i], k_destinationPin[i].toInt())
            ));
    }

    // -------- Step 2: Build fixed groups (same logic as old code) --------
    QVector<QVector<QPair<QString, int>>> fixedGroups;

    for (const auto &p : pairs)
    {
        QVector<int> groupIndices;

        for (int j = 0; j < fixedGroups.size(); ++j) {
            if (fixedGroups[j].contains(p.first) ||
                fixedGroups[j].contains(p.second)) {
                groupIndices.append(j);
            }
        }

        if (groupIndices.isEmpty()) {
            fixedGroups.append({p.first, p.second});
        } else {
            int main = groupIndices[0];
            for (int k = 1; k < groupIndices.size(); ++k) {
                fixedGroups[main] += fixedGroups[groupIndices[k]];
                fixedGroups[groupIndices[k]].clear();
            }

            if (!fixedGroups[main].contains(p.first))
                fixedGroups[main].append(p.first);
            if (!fixedGroups[main].contains(p.second))
                fixedGroups[main].append(p.second);
        }
    }

    fixedGroups.erase(
        std::remove_if(fixedGroups.begin(), fixedGroups.end(),
                       [](const QVector<QPair<QString,int>> &g){
                           return g.isEmpty();
                       }),
        fixedGroups.end()
        );

    // -------- Step 3: STORE PER-PACKET FIXED GROUPS --------
    // IMPORTANT: ISO/INSU treats EACH group as ONE PACKET
    for (int i = 0; i < fixedGroups.size(); ++i)
    {
        QVector<QVector<QPair<QString, int>>> packetGroups;
        packetGroups.append(fixedGroups[i]);
        allFixedGroups.append(packetGroups);

        // -------- NET LIST --------
        QVector<int> netList;
        netList.append(0);                              // net number
        netList.append(fixedGroups[i].size() * 2);      // length

        for (const auto &node : fixedGroups[i]) {
            netList.append(node.first.mid(2).toInt());
            netList.append(node.second);
        }

        allNetLists.append(netList);

        // -------- METADATA (MISSING PART) --------
        totalNetsEachPacket.append(1);   // ONE NET per packet (ISO/INSU logic)

        int voltage = 96;                // default = 500V
        // If later you add voltage selection logic, update here
        voltageEachPacket.append(voltage);
    }


    login->writeToNotes(
        QString("✔ Total Packets Created : %1")
            .arg(allNetLists.size())
        );

    for (int i = 0; i < allNetLists.size(); ++i)
    {
        QStringList dump;
        for (int v : allNetLists[i])
            dump << QString::number(v);

        login->writeToNotes(
            QString("NET[%1] = %2")
                .arg(i)
                .arg(dump.join(" "))
            );
    }

}


QByteArray TestController::buildUARTPacket(const QVector<int> &netList)
{
    QByteArray packet;
    packet.append(QByteArray::fromHex("2F2F"));
    packet.append(static_cast<char>(0xDD));

    int i = 0;
    while (i + 1 < netList.size()) {
        uint16_t netNo = netList[i++];
        uint16_t len   = netList[i++];

        packet.append(netNo >> 8);
        packet.append(netNo & 0xFF);
        packet.append(len >> 8);
        packet.append(len & 0xFF);

        for (int j = 0; j < len; ++j)
            packet.append(static_cast<char>(netList[i++]));
    }

    return packet;
}



void TestController::processTwoWireResult(const QByteArray &zRecvByte)
{
    login->writeToNotes("Processing Two Wire result packet");

    qDebug()<<m_simulate<<" :m_simulate";

    if(m_simulate == "No")
    {
        // ---------------- A. Extract payload ----------------
        if (zRecvByte.size() < 9) {
            login->writeToNotes("❌ Invalid Two Wire packet size");
            emit twoWireFinished(false, "Error");
            return;
        }

        QByteArray finalTwoWireBytes =
            zRecvByte.mid(3, zRecvByte.size() - 9);

        login->writeToNotes("Payload bytes : " +
                            finalTwoWireBytes.toHex(' ').toUpper());

        // ---------------- B. HEX → FLOAT ----------------
        QVector<float> resistanceValues =
            convertHexToFloatVector(finalTwoWireBytes);

        login->writeToNotes(QString("Resistance count : %1")
                                .arg(resistanceValues.size()));

        // ---------------- C. FORMAT ----------------
        QVector<QString> formattedResistances =
            formatResistanceValues(resistanceValues);

        // ---------------- D. PASS / FAIL ----------------
        QVector<QString> results;
        results.reserve(formattedResistances.size());

        for (int i = 0; i < formattedResistances.size(); ++i)
        {
            bool ok = false;

            QStringList parts =
                formattedResistances[i].split(" ");

            if (parts.size() < 2) {
                results.append("Invalid");
                continue;
            }

            float numericValue = parts.first().toFloat(&ok);
            QString unit = parts.last();

            if (!ok) {
                results.append("Fail");
                continue;
            }

            // Convert to ohms
            float resistanceInOhms = numericValue;
            if (unit.contains("k", Qt::CaseInsensitive))
                resistanceInOhms *= 1e3;
            else if (unit.contains("M", Qt::CaseInsensitive))
                resistanceInOhms *= 1e6;
            else if (unit.contains("G", Qt::CaseInsensitive))
                resistanceInOhms *= 1e9;

            if (i >= exp.size()) {
                results.append("Invalid");
                continue;
            }

            QString expStr = exp[i].trimmed();
            if (expStr.isEmpty()) {
                results.append("Invalid");
                continue;
            }

            QChar op = expStr[0];
            int expected = expStr.mid(1).toInt(&ok);
            if (!ok) {
                results.append("Invalid");
                continue;
            }

            bool pass =
                (op == '<' && resistanceInOhms < expected) ||
                (op == '>' && resistanceInOhms > expected);

            results.append(pass ? "Pass" : "Fail");
        }

        // ---------------- E. FINAL PDF ----------------
        generateTwoWirePDF(
            sourceCon,
            sourcePin,
            destCon,
            destPin,
            exp,
            formattedResistances,
            results,
            login->getProjectName(),
            login->getSetNo(),
            login->getTestType(),
            login->getPerformedBy(),
            login->getInspectedBy()
            );

        login->writeToNotes("📄 Two Wire PDF generated successfully");
        emit twoWireFinished(true,"Two wire test completed");
    }

    if(m_simulate == "Go")
    {
        //only testing purpose for two wire continuity -------------------------
        // Fill dummy values for testing
        QVector<QString> formattedResistances;
        QVector<QString> results;
        for (int i = 0; i < sourceCon.size(); ++i) {
            // Just alternate dummy values for demonstration
            if (i % 2 == 0) {
                formattedResistances.append("6.22 MO");
                results.append("Pass");
            } else {
                formattedResistances.append("5.81 MO");
                results.append("Fail");
            }
        }
        //-------------------------------------------------------------------------

        // ---------------- E. FINAL PDF ----------------
        generateTwoWirePDF(
            sourceCon,
            sourcePin,
            destCon,
            destPin,
            exp,
            formattedResistances,
            results,
            login->getProjectName(),
            login->getSetNo(),
            login->getTestType(),
            login->getPerformedBy(),
            login->getInspectedBy()
            );

        login->writeToNotes("📄 Two Wire Simulated PDF generated successfully");
        emit twoWireFinished(true,"Two wire test completed");
    }
}




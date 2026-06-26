#include "testcontroller.h"
#include "mainwindow.h"

TestController::TestController(QObject *parent) : QObject(parent)
{
 qDebug()<<"hello";

 serial = new QSerialPort(this);


}
TestController::~TestController()
{

}
void TestController::welcome()
{
    emit infor();
    qDebug()<<"#####"
    ;
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
    serial->setBaudRate(921600);
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
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 921600";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 921600");
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

    m_obj->writeToNotes("================================================");
    m_obj->writeToNotes("STEP-1 : Mapping Logical Connectors → Hardware Pins");
    m_obj->writeToNotes("================================================");

      emit executeWriteToNotes("===========================================");
      emit executeWriteToNotes("STEP-1:Mapping Logical Connectors->Hardware Pins");
      emit executeWriteToNotes("=============================================");

    // Clear previous mappings
    k_sourceCon.clear();
    k_sourcePin.clear();
    k_destinationCon.clear();
    k_destinationPin.clear();

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

    emit executeWriteToNotes(QString("Harness rows loaded : %1").arg(sourceCon.size()));

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

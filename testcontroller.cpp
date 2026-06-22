#include "testcontroller.h"

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

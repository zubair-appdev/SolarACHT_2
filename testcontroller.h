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

signals:
    void infor();
     void portOpening(const QString &);

private:
    QSerialPort *serial;
    QByteArray buffer;
};

#endif // TESTCONTROLLER_H

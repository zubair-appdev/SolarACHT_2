#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <QQmlContext>
#include <QStandardPaths>

#include "loginclass.h"
#include "testcontroller.h"


int main(int argc, char *argv[])
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Maytech Systems");
    QCoreApplication::setApplicationName("ACHT");

    QQmlApplicationEngine engine;

    LoginClass loginHandler;
    engine.rootContext()->setContextProperty("loginHandler",&loginHandler);

    TestController testController;
    engine.rootContext()->setContextProperty("testController", &testController);

    testController.setLogger(&loginHandler);


    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

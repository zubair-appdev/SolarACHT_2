//#include "LoginClass.h"

//QFile MainWindow::logFile;
//QTextStream MainWindow::logStream;

//LoginClass::LoginClass(QObject *parent)
//    : QObject(parent)
//{
//    resetLogFile();
//    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
//                 "     Application Started");
//    //#################################################

//    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");

//    initializeDatabase();
//}
//LoginClass::~LoginClass()
//{
//    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
//                 "     Application Closed");

//    closeLogFile();
//}
//void LoginClass::writeToNotes(const QString &message)
//{
//    if (!logFile.isOpen()) {
//        qCritical() << "Log file is not open.";
//        return;
//    }

//    // Add a timestamp for each entry
//    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
//    logStream << "[" << timestamp << "] " << message << Qt::endl;
//    logStream.flush(); // Ensure immediate write to disk
//}

//void LoginClass::resetLogFile()
//{
//    // Close the log file if it is open
//    if (logFile.isOpen()) {
//        logStream.flush();
//        logFile.close();
//    }

//    // Check if the file exists and delete it
//    QFile::remove("debug_notes.txt");

//    // Reinitialize the log file
//    initializeLogFile();
//}

//void LoginClass::initializeLogFile()
//{
//    if (!logFile.isOpen()) {
//        logFile.setFileName("debug_notes.txt");
//        if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
//            qCritical() << "Failed to open log file.";
//        } else {
//            logStream.setDevice(&logFile);
//        }
//    }
//}
//void LoginClass::closeLogFile()
//{
//    if (logFile.isOpen()) {
//        logStream.flush();
//        logFile.close();
//    }
//}
//bool LoginClass::initializeDatabase()
//{
//    for (const QString &driver : QSqlDatabase::drivers())
//        qDebug() << "Available driver:" << driver;

//    qDebug() << "Qt plugin paths:" << QCoreApplication::libraryPaths();

//    // Connect to SQLite
//    if (QSqlDatabase::contains("mainConnection"))
//        db = QSqlDatabase::database("mainConnection");
//    else
//        db = QSqlDatabase::addDatabase("QSQLITE", "mainConnection");

//    QStringList list = QSqlDatabase::connectionNames();
//    for (const QString &name : list)
//        qDebug() << "Active connection:" << name;

//    db.setDatabaseName("ACHT.db");

//    if (!db.open()) {
//        qWarning() << "Failed to open database:" << db.lastError().text();
//        writeToNotes("Failed To Open Database "+db.lastError().text());
//        return false;
//    }

//    qDebug() << "Database file:" << db.databaseName()
//             << "isOpen:" << db.isOpen()
//             << "connection name:" << db.connectionName();

//    writeToNotes("Database file: " + db.databaseName()
//                 + " isOpen : " + db.isOpen()
//                 + " connection name :" + db.connectionName());

//    // Create table if not exists
//    QSqlQuery query(db);
//    QString createTable =
//        "CREATE TABLE IF NOT EXISTS loginData ("
//        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
//        "username TEXT NOT NULL UNIQUE, "
//        "password TEXT NOT NULL, "
//        "role TEXT NOT NULL)";

//    if (!query.exec(createTable)) {
//        qWarning() << "Failed to create table: " << query.lastError().text();
//        writeToNotes("Failed to create table: " + query.lastError().text());
//        return false;
//    }

//    return true;
//}

//bool LoginClass::saveUser(const QString &username, const QString &password, const QString &role)
//{
//    if (!db.isOpen() && !initializeDatabase()) {
//        qWarning() << "Database not open!";
//        return false;
//    }

//    QSqlQuery query(db);
//    query.prepare("INSERT INTO loginData (username, password, role) VALUES (:username, :password, :role)");
//    query.bindValue(":username", username);
//    query.bindValue(":password", password);
//    query.bindValue(":role", role);

//    if (!query.exec()) {
//        qWarning() << "Insert failed:" << query.lastError().text();
//        writeToNotes("Insert failed:" + query.lastError().text());
//        return false;
//    }

//    qDebug() << "User saved:" << username << role;
//    writeToNotes("User saved: " + username +" "+ role);
//    return true;
//}

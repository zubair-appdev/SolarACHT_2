#include "loginclass.h"

LoginClass::LoginClass(QObject *parent)
    : QObject{parent}
{
    //reset previous notes #Notes things : Logging file
    resetLogFile();
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Started");
    //#################################################

    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");

    initializeDatabase();
}

LoginClass::~LoginClass()
{
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Closed");

    closeLogFile();
}

bool LoginClass::initializeDatabase()
{
    for (const QString &driver : QSqlDatabase::drivers())
        qDebug() << "Available driver:" << driver;

    qDebug() << "Qt plugin paths:" << QCoreApplication::libraryPaths();

    // Connect to SQLite
    if (QSqlDatabase::contains("mainConnection"))
        db = QSqlDatabase::database("mainConnection");
    else
        db = QSqlDatabase::addDatabase("QSQLITE", "mainConnection");

    QStringList list = QSqlDatabase::connectionNames();
    for (const QString &name : list)
        qDebug() << "Active connection:" << name;

    db.setDatabaseName("ACHT.db");

    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        writeToNotes("Failed To Open Database "+db.lastError().text());
        return false;
    }

    qDebug() << "Database file:" << db.databaseName()
             << "isOpen:" << db.isOpen()
             << "connection name:" << db.connectionName();

    writeToNotes("Database file: " + db.databaseName()
                 + " isOpen : " + db.isOpen()
                 + " connection name :" + db.connectionName());

    // Create table if not exists
    QSqlQuery query(db);
    QString createTable =
        "CREATE TABLE IF NOT EXISTS loginData ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT NOT NULL UNIQUE, "
        "password TEXT NOT NULL, "
        "role TEXT NOT NULL)";

    if (!query.exec(createTable)) {
        qWarning() << "Failed to create table: " << query.lastError().text();
        writeToNotes("Failed to create table: " + query.lastError().text());
        return false;
    }

    return true;
}

bool LoginClass::saveUser(const QString &username, const QString &password, const QString &role)
{
    if (!db.isOpen() && !initializeDatabase()) {
        qWarning() << "Database not open!";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO loginData (username, password, role) VALUES (:username, :password, :role)");
    query.bindValue(":username", username);
    query.bindValue(":password", password);
    query.bindValue(":role", role);

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
        writeToNotes("Insert failed:" + query.lastError().text());
        return false;
    }

    qDebug() << "User saved:" << username << role;
    writeToNotes("User saved: " + username +" "+ role);
    return true;
}

void LoginClass::writeToNotes(const QString &message)
{
    if (!logFile.isOpen()) {
        qCritical() << "Log file is not open.";
        return;
    }

    // Add a timestamp for each entry
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logStream << "[" << timestamp << "] " << message << Qt::endl;
    logStream.flush(); // Ensure immediate write to disk
}

void LoginClass::resetLogFile()
{
    // Close the log file if it is open
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }

    // Check if the file exists and delete it
    QFile::remove("debug_notes.txt");

    // Reinitialize the log file
    initializeLogFile();
}

void LoginClass::initializeLogFile()
{
    if (!logFile.isOpen()) {
        logFile.setFileName("debug_notes.txt");
        if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open log file.";
        } else {
            logStream.setDevice(&logFile);
        }
    }
}

void LoginClass::closeLogFile()
{
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}

bool LoginClass::validateAdmin(const QString &username, const QString &password)
{
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM loginData WHERE role = 'Admin' AND username = :u AND password = :p");
    query.bindValue(":u", username);
    query.bindValue(":p", password);

    if (!query.exec()) {
        qWarning() << "validateAdmin query failed:" << query.lastError();
        return false;
    }

    if (query.next()) {
        return query.value(0).toInt() > 0;   // true if ANY admin matches
    }

    return false;
}

bool LoginClass::updatePassword(const QString &username,
                                const QString &newPassword,
                                const QString &role)
{
    if (!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare("UPDATE loginData SET password=:pw "
                  "WHERE username=:u AND role=:r");

    query.bindValue(":pw", newPassword);
    query.bindValue(":u", username);
    query.bindValue(":r", role);

    if (!query.exec()) {
        qWarning() << "updatePassword failed:" << query.lastError();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QString LoginClass::loginUser(const QString &username, const QString &password)
{
    if (!db.isOpen())
        return "";

    QSqlQuery q(db);
    q.prepare("SELECT role FROM loginData "
              "WHERE username=:u AND password=:p");
    q.bindValue(":u", username);
    q.bindValue(":p", password);

    if (!q.exec())
        return "";

    if (q.next())
        return q.value(0).toString();   // return role: "Admin" or "User"

    return "";   // login failed
}


bool LoginClass::processCsvFile(const QString &fileUrl,
                                const QString &cableNameFromUI,
                                const QString &type)
{
    QString localPath = QUrl(fileUrl).toLocalFile();

    if (localPath.isEmpty()) {
        writeToNotes("❌ Invalid CSV File URL");
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        writeToNotes("❌ Failed to open file: " + localPath);
        return false;
    }

    QTextStream stream(&file);

    // 🔥 IMPORTANT: Clear data ONLY when reading PATCH CSV
    if (type == "patch") {
        cableList.clear();
        userConList.clear();
        userPinList.clear();
        patchConList.clear();
        patchPinList.clear();
        writeToNotes("🔄 Cleared previous patch data (fresh upload)");
    }

    // Harness temp buffers
    QVector<QString> cableTemp;
    QVector<QString> sourceConTemp, sourcePinTemp;
    QVector<QString> destConTemp, destPinTemp;
    QVector<QString> expTemp, voltTemp;

    // ⭐ IMPORTANT LOG ENTRY
    writeToNotes("Processing CSV → Cable: " + cableNameFromUI +
                 ", Type: " + type +
                 ", File: " + localPath);

    int lineNumber = 0;

    while (!stream.atEnd()) {

        QString line = stream.readLine().trimmed();
        lineNumber++;

        if (line.isEmpty()) continue;

        QStringList parts = line.split(",");

        if (type == "patch") {

            if (parts.size() != 5) {
                writeToNotes("❌ Malformed PATCH line " + QString::number(lineNumber));
                return false;
            }

            QString pCon = parts[3].trimmed();
            QString pPinStr = parts[4].trimmed();

            if (!validatePatchLine(lineNumber, pCon, pPinStr))
                return false;

            // ⭐ LOG SUCCESSFUL PATCH LINE
            writeToNotes("PATCH Line OK (" + QString::number(lineNumber) + "): " +
                         parts[0].trimmed() + ", " +
                         parts[1].trimmed() + ", " +
                         parts[2].trimmed() + ", " +
                         pCon + ", " + pPinStr);

            cableList.append(parts[0].trimmed());
            userConList.append(parts[1].trimmed());
            userPinList.append(parts[2].trimmed());
            patchConList.append(pCon);
            patchPinList.append(pPinStr.toInt());

        }
        else if (type == "harness") {

            if (parts.size() != 7) {
                writeToNotes("❌ Malformed HARNESS line " + QString::number(lineNumber));
                return false;
            }

            cableTemp.append(parts[0].trimmed());
            sourceConTemp.append(parts[1].trimmed());
            sourcePinTemp.append(parts[2].trimmed());
            destConTemp.append(parts[3].trimmed());
            destPinTemp.append(parts[4].trimmed());
            expTemp.append(parts[5].trimmed());
            voltTemp.append(parts[6].trimmed());
        }
    }

    file.close();

    // 1️⃣ SAVE PATCH DATA HERE
    if (type == "patch") {
        if (!savePatchToDb(cableNameFromUI)) {
            writeToNotes("❌ Failed to save PATCH data to DB");
            return false;
        }
        writeToNotes("💾 Saved PATCH data to DB for cable: " + cableNameFromUI);
    }

    // Run final harness validations
    if (type == "harness") {
        if (!validateHarnessData(cableTemp,
                                 sourceConTemp, sourcePinTemp,
                                 destConTemp, destPinTemp,
                                 expTemp, voltTemp))
            return false;

        if (!saveHarnessToDb(cableNameFromUI,
                             cableTemp,
                             sourceConTemp, sourcePinTemp,
                             destConTemp, destPinTemp,
                             expTemp, voltTemp))
        {
            writeToNotes("❌ Failed to save HARNESS data to DB");
            return false;
        }

        writeToNotes("💾 Saved HARNESS data to DB for cable: " + cableNameFromUI);
    }

    writeToNotes("✅ CSV processed successfully → Cable: " +
                 cableNameFromUI + ", Rows: " +
                 QString::number(cableList.size()));

    return true;
}


bool LoginClass::validatePatchLine(int lineNumber,
                                   const QString &pCon,
                                   const QString &pPinStr)
{
    QRegularExpression tpRegex("^TP([1-9]|1[0-6])$");

    if (!tpRegex.match(pCon).hasMatch()) {
        writeToNotes(QString(" Invalid PatchCon at line %1: %2 (must be TP1–TP16)")
                         .arg(lineNumber).arg(pCon));
        return false;
    }

    bool ok;
    int pPin = pPinStr.toInt(&ok);
    if (!ok || pPin < 1 || pPin > 64) {
        writeToNotes(QString(" Invalid PatchPin at line %1: %2 (must be 1–64)")
                         .arg(lineNumber).arg(pPinStr));
        return false;
    }

    return true;
}


bool LoginClass::validateHarnessData(const QVector<QString> &cableTemp,
                                     const QVector<QString> &sourceConTemp,
                                     const QVector<QString> &sourcePinTemp,
                                     const QVector<QString> &destConTemp,
                                     const QVector<QString> &destPinTemp,
                                     const QVector<QString> &expTemp,
                                     const QVector<QString> &voltTemp)
{
    QVector<QString> sourceMiss, destMiss;
    QString extraLog;

    for (int i = 0; i < sourceConTemp.size(); i++)
    {
        int matchSrc = 0, matchDst = 0;

        for (int j = 0; j < userConList.size(); j++) {
            if (sourceConTemp[i] == userConList[j] &&
                sourcePinTemp[i] == userPinList[j])
                matchSrc++;

            if (destConTemp[i] == userConList[j] &&
                destPinTemp[i] == userPinList[j])
                matchDst++;
        }

        if (matchSrc == 0)
            sourceMiss.append(sourceConTemp[i] + "-" + sourcePinTemp[i]);

        if (matchDst == 0)
            destMiss.append(destConTemp[i] + "-" + destPinTemp[i]);

        if (matchSrc > 1)
            extraLog += QString("⚠ EXTRA source append at index %1: %2-%3\n")
                            .arg(i).arg(sourceConTemp[i]).arg(sourcePinTemp[i]);

        if (matchDst > 1)
            extraLog += QString("⚠ EXTRA destination append at index %1: %2-%3\n")
                            .arg(i).arg(destConTemp[i]).arg(destPinTemp[i]);
    }

    if (!extraLog.isEmpty()) {
        writeToNotes(extraLog);
        return false;
    }

    if (!sourceMiss.isEmpty() || !destMiss.isEmpty()) {
        QString missingLog =
            "Missing Source: " +
            QStringList(sourceMiss.begin(), sourceMiss.end()).join(", ") + " | " +
            "Missing Destination: " +
            QStringList(destMiss.begin(), destMiss.end()).join(", ");

        writeToNotes("❌ " + missingLog);
        return false;
    }

    // Validate expValue
    for (int i = 0; i < expTemp.size(); i++) {
        QString v = expTemp[i];

        if ((!v.startsWith("<") && !v.startsWith(">")) ||
            v.mid(1).toInt() < 1 || v.mid(1).toInt() > 100)
        {
            writeToNotes(QString("❌ Invalid expValue at index %1: %2")
                             .arg(i).arg(v));
            return false;
        }
    }

    // Validate Voltage
    for (int i = 0; i < voltTemp.size(); i++) {
        QString v = voltTemp[i];

        if (v != "250V" && v != "500V") {
            writeToNotes(QString("❌ Invalid Voltage at index %1: %2")
                             .arg(i).arg(v));
            return false;
        }
    }

    // ---------------------------------------------------------
    // ⭐ PRINT ALL HARNESS DATA AFTER SUCCESSFUL VALIDATION
    // ---------------------------------------------------------
    writeToNotes("######## HARNESS DATA (VALIDATED) ########");

    for (int i = 0; i < sourceConTemp.size(); i++) {

        QString log = QString(
                          "HARNESS Line OK (%1): Cable: %2 , SRC: %3-%4 → DEST: %5-%6 , exp: %7 , volt: %8")
                          .arg(i + 1)
                          .arg(cableTemp[i])              // <--- NEW
                          .arg(sourceConTemp[i])
                          .arg(sourcePinTemp[i])
                          .arg(destConTemp[i])
                          .arg(destPinTemp[i])
                          .arg(expTemp[i])
                          .arg(voltTemp[i]);

        writeToNotes(log);
    }

    writeToNotes("###########################################");
    writeToNotes("✅ Harness data validated successfully.");

    return true;
}


bool LoginClass::savePatchToDb(const QString &cableName)
{
    QString tableName = cableName + "_patch";
    tableName.replace(" ", "_");

    QSqlQuery q(db);

    QString create =
        "CREATE TABLE IF NOT EXISTS " + tableName + " ("
                                                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                    "cable TEXT,"
                                                    "userCon TEXT,"
                                                    "userPin TEXT,"
                                                    "patchCon TEXT,"
                                                    "patchPin INTEGER"
                                                    ")";

    if (!q.exec(create)) {
        writeToNotes("❌ Failed to create table " + tableName + ": " + q.lastError().text());
        return false;
    }

    // Clear old data
    q.exec("DELETE FROM " + tableName);

    // Insert new rows
    q.prepare("INSERT INTO " + tableName +
              "(cable, userCon, userPin, patchCon, patchPin) VALUES (?, ?, ?, ?, ?)");

    for (int i = 0; i < cableList.size(); i++) {
        q.addBindValue(cableList[i]);
        q.addBindValue(userConList[i]);
        q.addBindValue(userPinList[i]);
        q.addBindValue(patchConList[i]);
        q.addBindValue(patchPinList[i]);

        if (!q.exec()) {
            writeToNotes("❌ Insert failed in " + tableName + ": " + q.lastError().text());
            return false;
        }
    }

    writeToNotes("✅ Patch data stored in table: " + tableName);
    return true;
}


bool LoginClass::saveHarnessToDb(const QString &cableName,
                                 const QVector<QString> &cableTemp,
                                 const QVector<QString> &sourceConTemp,
                                 const QVector<QString> &sourcePinTemp,
                                 const QVector<QString> &destConTemp,
                                 const QVector<QString> &destPinTemp,
                                 const QVector<QString> &expTemp,
                                 const QVector<QString> &voltTemp)
{
    QString tableName = cableName + "_harness";
    tableName.replace(" ", "_");

    QSqlQuery q(db);

    QString create =
        "CREATE TABLE IF NOT EXISTS " + tableName + " ("
                                                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                    "cable TEXT,"
                                                    "sourceCon TEXT,"
                                                    "sourcePin TEXT,"
                                                    "destCon TEXT,"
                                                    "destPin TEXT,"
                                                    "exp TEXT,"
                                                    "volt TEXT"
                                                    ")";

    if (!q.exec(create)) {
        writeToNotes("❌ Failed to create table " + tableName + ": " + q.lastError().text());
        return false;
    }

    q.exec("DELETE FROM " + tableName);

    q.prepare("INSERT INTO " + tableName +
              "(cable, sourceCon, sourcePin, destCon, destPin, exp, volt) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");

    for (int i = 0; i < sourceConTemp.size(); i++) {

        q.addBindValue(cableTemp[i]);
        q.addBindValue(sourceConTemp[i]);
        q.addBindValue(sourcePinTemp[i]);
        q.addBindValue(destConTemp[i]);
        q.addBindValue(destPinTemp[i]);
        q.addBindValue(expTemp[i]);
        q.addBindValue(voltTemp[i]);

        if (!q.exec()) {
            writeToNotes("❌ Insert failed in " + tableName + ": " + q.lastError().text());
            return false;
        }
    }

    writeToNotes("✅ Harness data stored in table: " + tableName);
    return true;
}


QStringList LoginClass::getCableNames()
{
    QStringList list;
    list.append("Files");   // First fixed item


    // Get all tables
    QStringList tables = db.tables();

    QSet<QString> uniqueNames;

    for (const QString &t : tables) {

        if (t.endsWith("_patch")) {
            uniqueNames.insert(t.left(t.length() - 6)); // remove 6 chars "_patch"
        }
        else if (t.endsWith("_harness")) {
            uniqueNames.insert(t.left(t.length() - 8)); // remove 8 chars "_harness"
        }
    }

    // Convert back to list and sort alphabetically
    QStringList sorted = uniqueNames.values();
    sorted.sort();

    list.append(sorted);

    return list;
}


QVariantList LoginClass::getPatchData(const QString &cableName)
{
    QVariantList rows;

    if (!db.isOpen()) {
        writeToNotes("getPatchData: DB not open");
        return rows;
    }

    QString tableName = cableName + "_patch";

    if (!db.tables().contains(tableName)) {
        writeToNotes("getPatchData: Table not found → " + tableName);
        return rows;
    }

    QString sql = "SELECT cable, userCon, userPin, patchCon, patchPin FROM " + tableName;

    QSqlQuery q(db);
    if (!q.exec(sql)) {
        writeToNotes("getPatchData: query failed → " + q.lastError().text());
        return rows;
    }

    while (q.next()) {
        QVariantMap m;
        m["cable"]    = q.value(0).toString();
        m["userCon"]  = q.value(1).toString();
        m["userPin"]  = q.value(2).toString();
        m["patchCon"] = q.value(3).toString();
        m["patchPin"] = q.value(4).toInt();
        rows.append(m);
    }

    writeToNotes(QString("getPatchData: %1 rows for %2").arg(rows.size()).arg(cableName));
    return rows;
}

QVariantList LoginClass::getHarnessData(const QString &cableName)
{
    QVariantList rows;

    if (!db.isOpen()) {
        writeToNotes("getHarnessData: DB not open");
        return rows;
    }

    QString tableName = cableName + "_harness";

    if (!db.tables().contains(tableName)) {
        writeToNotes("getHarnessData: Table not found → " + tableName);
        return rows;
    }

    QString sql =
        "SELECT cable, sourceCon, sourcePin, destCon, destPin, exp, volt "
        "FROM " + tableName;

    QSqlQuery q(db);
    if (!q.exec(sql)) {
        writeToNotes("getHarnessData: query failed → " + q.lastError().text());
        return rows;
    }

    while (q.next()) {
        QVariantMap m;
        m["cable"]     = q.value(0).toString();
        m["sourceCon"] = q.value(1).toString();
        m["sourcePin"] = q.value(2).toString();
        m["destCon"]   = q.value(3).toString();
        m["destPin"]   = q.value(4).toString();
        m["exp"]       = q.value(5).toString();
        m["volt"]      = q.value(6).toString();
        rows.append(m);
    }

    writeToNotes(QString("getHarnessData: %1 rows for %2").arg(rows.size()).arg(cableName));
    return rows;
}

bool LoginClass::deleteCable(const QString &cableName)
{
    if (!db.isOpen()) {
        writeToNotes("❌ deleteCable: DB not open");
        return false;
    }

    QString patchTable  = cableName + "_patch";
    QString harnessTable = cableName + "_harness";

    QSqlQuery q(db);

    // Delete patch table if exists
    if (db.tables().contains(patchTable)) {
        if (!q.exec("DROP TABLE " + patchTable)) {
            writeToNotes("❌ Failed to delete table " + patchTable + ": " + q.lastError().text());
            return false;
        }
        writeToNotes("🗑 Deleted table → " + patchTable);
    }

    // Delete harness table if exists
    if (db.tables().contains(harnessTable)) {
        if (!q.exec("DROP TABLE " + harnessTable)) {
            writeToNotes("❌ Failed to delete table " + harnessTable + ": " + q.lastError().text());
            return false;
        }
        writeToNotes("🗑 Deleted table → " + harnessTable);
    }

    writeToNotes("✅ Cable deleted successfully → " + cableName);
    return true;
}

void LoginClass::prepareTestData(
    const QString &cableName,
    const QString &setNo,
    const QString &performedBy,
    const QString &inspectedBy,
    const QString &projectName,
    const QString &notes,
    const QString &testToRun,
    const QVariantList &patchData,
    const QVariantList &harnessData)
{

    // ✅ STORE METADATA
    m_setNo       = setNo;
    m_performedBy = performedBy;
    m_inspectedBy = inspectedBy;
    m_projectName = projectName;
    m_testType    = testToRun;

    writeToNotes("=== TEST DATA RECEIVED FROM QML ===");
    writeToNotes("Cable     : " + cableName);
    writeToNotes("Set No    : " + setNo);
    writeToNotes("Performed : " + performedBy);
    writeToNotes("Inspected : " + inspectedBy);
    writeToNotes("Project   : " + projectName);
    writeToNotes("Notes     : " + notes);
    writeToNotes("Test Type : " + testToRun);

    writeToNotes(QString("Patch Data Count: %1").arg(patchData.size()));
    writeToNotes(QString("Harness Data Count: %1").arg(harnessData.size()));

    // TODO: Next Step — store into vectors for packet formation
}


















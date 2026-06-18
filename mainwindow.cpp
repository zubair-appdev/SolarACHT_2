#include "mainwindow.h"
#include "ui_mainwindow.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

#include <QToolButton>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->lineEdit_userName->setPlaceholderText("👤 Type your username");
    ui->lineEdit_password->setPlaceholderText("🔒 Enter your password");
    ui->lineEdit_password->setEchoMode(QLineEdit::Password);
    ui->lineEdit_masterKeyRecPass->setEchoMode(QLineEdit::Password);
    ui->lineEdit_changePassword->setEchoMode(QLineEdit::Password);
    ui->lineEdit_confimPassword->setEchoMode(QLineEdit::Password);
    ui->lineEdit_adminPass->setEchoMode(QLineEdit::Password);

    ui->lineEdit_masterKey->setEchoMode(QLineEdit::Password);
    QToolButton *eyeBtn = new QToolButton(ui->lineEdit_password);
    eyeBtn->setCursor(Qt::PointingHandCursor);
    eyeBtn->setStyleSheet("border: none;");
    eyeBtn->setIcon(QIcon(":/new/prefix1/images/eye.png"));

    eyeBtn->setFixedSize(20,20);

    QHBoxLayout *layout = new QHBoxLayout(ui->lineEdit_password);
    layout->addWidget(eyeBtn, 0, Qt::AlignRight);
    layout->setContentsMargins(0, 0, 5, 0);
    ui->lineEdit_password->setLayout(layout);


    connect(eyeBtn, &QToolButton::clicked, this, [=]() mutable
    {
        if(ui->lineEdit_password->echoMode() == QLineEdit::Password)
        {
            ui->lineEdit_password->setEchoMode(QLineEdit::Normal);
            eyeBtn->setIcon(QIcon(":/new/prefix1/images/eyeOff.png"));
        }
        else
        {
            ui->lineEdit_password->setEchoMode(QLineEdit::Password);
            eyeBtn->setIcon(QIcon(":/new/prefix1/images/eye.png"));
        }
    });

    resetLogFile();
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Started");
    //#################################################

    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");


    initializeDatabase();
    ui->stackedWidget->setCurrentIndex(0);
    ui->lineEdit_newPassword->setEchoMode(QLineEdit::Password);
}
MainWindow::~MainWindow()
{
    delete ui;
}
bool MainWindow::initializeDatabase()
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
void MainWindow::writeToNotes(const QString &message)
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

void MainWindow::resetLogFile()
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

void MainWindow::initializeLogFile()
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

void MainWindow::closeLogFile()
{
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}


void MainWindow::on_pushButton_Save_clicked()
{
    QString username = ui->lineEdit_newUser->text().trimmed();
    QString password = ui->lineEdit_newPassword->text().trimmed();

    if(username.isEmpty() || password.isEmpty())
    {
       QMessageBox::information(this,"Empty","Fill all the fields");
        return;
    }

    QSqlQuery query;

    query.prepare("INSERT INTO users(username, password) "
                  "VALUES(:username, :password)");

    query.bindValue(":username", username);
    query.bindValue(":password", password);

    if(query.exec())
    {
        QMessageBox::information(this,"success","User saved successfully");
    }
    else
    {
        QMessageBox::information(this,"exists","Username already exists");
    }

}

void MainWindow::on_pushButton_add_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::on_pushButton_login_clicked()
{
    if (!db.isOpen())
        return;

    QString username = ui->lineEdit_userName->text().trimmed();

    QString password =ui->lineEdit_password->text().trimmed();

    QSqlQuery q(db);

    q.prepare("SELECT role FROM loginData "
              "WHERE username=:u AND password=:p");

    q.bindValue(":u", username);
    q.bindValue(":p", password);

    if (!q.exec())
    {
        qDebug() << q.lastError().text();
        return;
    }

    if (q.next())
    {
        role = q.value(0).toString();

        qDebug() << "Login successful. Role:" << role;

        ui->label_welcome->setText(
                    "Welcome, " + username + "!");

        ui->label_role->setText(
                    "Role: " + role);

        bool isAdmin = (role == "admin");

        ui->pushButton_deleteUsers->setVisible(isAdmin);
        ui->pushButton_dataEntry->setVisible(isAdmin);
        currentAdmin = username;
        ui->stackedWidget->setCurrentIndex(4);
    }
    else
    {
        QMessageBox::warning(this,
                             "Login Failed",
                             "Invalid username or password");
        return;
    }
    ui->lineEdit_userName->clear();
    ui->lineEdit_password->clear();
}

void MainWindow::on_pushButton_forward_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->lineEdit_newUser->clear();
    ui->lineEdit_newPassword->clear();
}

void MainWindow::on_pushButton_forward_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_forgetPassword_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
}

void MainWindow::on_pushButton_userForgPass_clicked()
{
    ui->stackedWidget->setCurrentIndex(6);
    role="user";
}

void MainWindow::on_pushButton_adminForgPass_clicked()
{
    ui->stackedWidget->setCurrentIndex(7);
    role="admin";
}

void MainWindow::on_pushButton_saveUser_clicked()
{
    if (!db.isOpen() && !initializeDatabase())
    {
        QMessageBox::warning(this,
                             "Database Error",
                             "Unable to open database");
        return;
    }

    QString username =
            ui->lineEdit_newUser->text().trimmed();

    QString password =
            ui->lineEdit_newPassword->text().trimmed();



    if(username.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this,
                             "Empty Fields",
                             "Please fill all fields");
        return;
    }


    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO loginData "
        "(username, password, role) "
        "VALUES "
        "(:username, :password, :role)"
    );

    query.bindValue(":username", username);
    query.bindValue(":password", password);
    query.bindValue(":role", role);

    if(query.exec())
    {
        QMessageBox::information(this,"Success","User saved successfully");
        writeToNotes("User saved: " +username +" " +role);
    }
    else
    {
        QMessageBox::warning(this,"Insert Failed","User already exists");
         writeToNotes( "Insert failed: "+query.lastError().text());
    }
}

void MainWindow::on_pushButton_addUser_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
    role="user";
    ui->label_addNewUser->setText("Create User Account");
}

void MainWindow::on_pushButton_addAdmin_clicked()
{
    ui->stackedWidget->setCurrentIndex(3);
    role="admin";
    ui->label_addNewUser->setText("Create Admin Account");
}

void MainWindow::on_pushButton_validateMasterKey_clicked()
{
   QString masterKey =ui->lineEdit_masterKey->text().trimmed();
   if(masterKey == "")
   {
       QMessageBox::information(this,"Empty Field","Enter master key first");
       return;
   }
   if(masterKey=="maytech1234")
   {
       ui->stackedWidget->setCurrentIndex(2);
       role ="admin";
       ui->lineEdit_masterKey->clear();
   }
   else
   {
    QMessageBox::information(this,"Invalid","Wrong master key entered");
   }

}

void MainWindow::on_pushButton_backToLogin_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_valAdminForUserPass_clicked()
{

    if (!db.isOpen())
        return;
    QString username = ui->lineEdit_admin->text().trimmed();
    QString password = ui->lineEdit_adminPass->text().trimmed();

    QSqlQuery query(db);
    query.prepare(
        "SELECT role FROM loginData "
        "WHERE role='admin' "
        "AND username=:u "
        "AND password=:p");
    query.bindValue(":u", username);
    query.bindValue(":p", password);

    if(query.exec() && query.next())
    {
        role = "user";

        ui->stackedWidget->setCurrentIndex(8);

        ui->label_titleForChangePassword
                ->setText("Recover User Account");
    }
    else
    {
        QMessageBox::warning(this,
                             "Invalid",
                             "Invalid username or password");
    }
}

void MainWindow::on_pushButton_update_clicked()
{
    QString username = ui->lineEdit_usernameExist->text().trimmed();
    QString password = ui->lineEdit_changePassword->text().trimmed();
    QString confirmPassword = ui->lineEdit_confimPassword->text().trimmed();

    if(username.isEmpty() || password.isEmpty() ||confirmPassword.isEmpty())
    {
        QMessageBox::information(this,"Empty Fields","Fill all the fields");
        return;
    }


    if(password != confirmPassword)
    {
        QMessageBox::warning(this,
                             "Password Mismatch",
                             "Password and Confirm Password do not match.");
        return;
    }
    if (!db.isOpen())
        return;

    QSqlQuery query(db);
    query.prepare("UPDATE loginData SET password=:pw "
                  "WHERE username=:u AND role=:r");

    query.bindValue(":pw", password);
    query.bindValue(":u", username);
    query.bindValue(":r", role);

    qDebug()<<username<<"*********";
    qDebug()<<role<<"**************";

    if (!query.exec()) {
        qWarning() << "updatePassword failed:" << query.lastError();
        return;
     }
    if(query.numRowsAffected() == 0)
    {
        QString msg = QString("Username not found in %1").arg(role);
        QMessageBox::warning(this,"Invalid User",msg);
        return;
    }
    QMessageBox::information(this,"Success","Password updated successfully");
}
void MainWindow::on_pushButton_home_clicked()
{
  ui->stackedWidget->setCurrentIndex(0);
  ui->lineEdit_usernameExist->clear();
  ui->lineEdit_changePassword->clear();
  ui->lineEdit_confimPassword->clear();
}

void MainWindow::on_pushButton_recAdminPass_clicked()
{
    QString masterKey =ui->lineEdit_masterKeyRecPass->text().trimmed();
    if(masterKey == "")
    {
        QMessageBox::information(this,"Empty Field","Enter master key first");
        return;
    }
    if(masterKey=="maytech1234")
    {

        role ="admin";
        ui->stackedWidget->setCurrentIndex(8);
        ui->label_titleForChangePassword->setText("Recover Admin Account");
        ui->lineEdit_masterKeyRecPass->clear();
    }
    else
    {
     QMessageBox::information(this,"Invalid","Wrong master key entered");
    }

}

//void MainWindow::clearAllLineEdits()
//{
//    QList<QLineEdit*> lineEdits =
//            this->findChildren<QLineEdit*>();

//    for(QLineEdit *lineEdit : lineEdits)
//    {
//        lineEdit->clear();
//    }
//}

void MainWindow::on_pushButton_3_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
    ui->lineEdit_masterKeyRecPass->clear();
}

void MainWindow::on_pushButton_gotoRoleSelect_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->lineEdit_masterKey->clear();

}

void MainWindow::on_pushButton_userPasswordPageBack_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
    ui->lineEdit_admin->clear();
    ui->lineEdit_adminPass->clear();
}

void MainWindow::on_pushButton_backToLogin_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_logOut_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_deleteUsers_clicked()
{
    loadUsers();
    ui->stackedWidget->setCurrentIndex(9);


}
void MainWindow::loadUsers()
{
    ui->tableWidget_users->setRowCount(0);
    ui->tableWidget_users->setColumnCount(2);
    ui->tableWidget_users->setHorizontalHeaderLabels({"Username","Role"});
    ui->tableWidget_users->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_users->verticalHeader()->setVisible(false);

    ui->tableWidget_users->setShowGrid(false);
    ui->tableWidget_users->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget_users->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QSqlQuery query(db);
    query.prepare("SELECT username, role FROM loginData");

    if(!query.exec())
    {
       qDebug() << query.lastError().text();
       return;
     }
    int row = 0;

    while(query.next())
    {
        qDebug() << query.value(0).toString()
                  << query.value(1).toString();
        ui->tableWidget_users->insertRow(row);

        ui->tableWidget_users->setItem(
                    row,
                    0,
                    new QTableWidgetItem(query.value(0).toString()));

        ui->tableWidget_users->setItem(
                    row,
                    1,
                    new QTableWidgetItem(query.value(1).toString()));

        row++;
    }
}
void MainWindow::on_pushButton_deleteSelected_clicked()
{
    int row =
        ui->tableWidget_users->currentRow();

    if(row < 0)
    {
        QMessageBox::information(
                    this,
                    "Select User",
                    "Please select a user.");
        return;
    }

    QString username =
        ui->tableWidget_users
            ->item(row,0)
            ->text();

    if(username == currentAdmin)
    {
        QMessageBox::warning(
                    this,
                    "Not Allowed",
                    "You cannot delete your own account.");
        return;
    }

    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Confirm Delete",
                "Delete user \"" +
                username +
                "\" ?");

    if(reply != QMessageBox::Yes)
        return;

    QSqlQuery query(db);

    query.prepare(
        "DELETE FROM loginData "
        "WHERE username=:u");

    query.bindValue(":u", username);

    if(query.exec())
    {
        loadUsers();

        QMessageBox::information(
                    this,
                    "Success",
                    "User deleted successfully.");
    }
}

void MainWindow::on_pushButton_delTomain_clicked()
{
    ui->stackedWidget->setCurrentIndex(4);
}

void MainWindow::on_pushButton_dataEntry_clicked()
{
    ui->stackedWidget->setCurrentIndex(10);
}

void MainWindow::on_pushButton_uploadPatch_clicked()
{
      openCsvFile("patch");
}


void MainWindow::on_pushButton_uploadCable_clicked()
{
    openCsvFile("harness");
}
void MainWindow::openCsvFile(const QString &fileType)
{
    QString fileName =
            QFileDialog::getOpenFileName(
                this,
                "Select Data File",
                "",
                "Data Files (*.csv *.xls *.xlsx);;CSV Files (*.csv);;Excel Files (*.xls *.xlsx)");
    if(fileName.isEmpty())
        return;
    QString fileToProcess = fileName;

    QFileInfo info(fileName);

    if (info.suffix().compare("xlsx", Qt::CaseInsensitive) == 0)
    {
        fileToProcess = convertExcelToCsv(fileName);

        if (fileToProcess.isEmpty())
        {
            QMessageBox::critical(
                this,
                "Error",
                "Failed to convert Excel file.");

            return;
        }
    }

    bool success =processCsvFile( fileToProcess, ui->lineEdit_cableName->text(),fileType);
    if(success)
    {
        ui->comboBox_fileNames->clear();
        ui->comboBox_fileNames->addItems(getCableNames());
        QMessageBox::information(this,"Success","CSV uploaded successfully!");

    }
    else
    {
        QMessageBox::information(this,"Error","Failed to process CSV file");
    }

}
bool MainWindow::processCsvFile(const QString &fileName,
                                const QString &cableNameFromUI,
                                const QString &type)
{

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        writeToNotes("❌ Failed to open file: " + fileName);
        return false;
    }

    QTextStream stream(&file);

    // IMPORTANT: Clear data ONLY when reading PATCH CSV
    if (type == "patch") {
        cableList.clear();
        userConList.clear();
        userPinList.clear();
        patchConList.clear();
        patchPinList.clear();
        writeToNotes("Cleared previous patch data (fresh upload)");
    }

    // Harness temp buffers
    QVector<QString> cableTemp;
    QVector<QString> sourceConTemp, sourcePinTemp;
    QVector<QString> destConTemp, destPinTemp;
    QVector<QString> expTemp, voltTemp;

    // ⭐ IMPORTANT LOG ENTRY
    writeToNotes("Processing CSV → Cable: " + cableNameFromUI +
                 ", Type: " + type +
                 ", File: " + fileName);

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

bool MainWindow::savePatchToDb(const QString &cableName)
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
bool MainWindow::validateHarnessData(const QVector<QString> &cableTemp,
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
    // PRINT ALL HARNESS DATA AFTER SUCCESSFUL VALIDATION
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
bool MainWindow::saveHarnessToDb(const QString &cableName,
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
bool MainWindow::validatePatchLine(int lineNumber,
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
QStringList MainWindow::getCableNames()
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
QString MainWindow::convertExcelToCsv(const QString &xlsxFile)
{
    QXlsx::Document xlsx(xlsxFile);

    if (!xlsx.load())
    {
        qDebug() << "Failed to load Excel file";
        return QString();
    }

    QString csvPath =
            QDir::tempPath() + "/" +
            QFileInfo(xlsxFile).completeBaseName() +
            ".csv";

    QFile csvFile(csvPath);

    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Failed to create CSV file";
        return QString();
    }

    QTextStream out(&csvFile);

    QXlsx::CellRange range = xlsx.dimension();

    int firstRow = range.firstRow();
    int lastRow  = range.lastRow();

    int firstCol = range.firstColumn();
    int lastCol  = range.lastColumn();

    for (int row = firstRow; row <= lastRow; ++row)
    {
        QStringList fields;

        for (int col = firstCol; col <= lastCol; ++col)
        {
            QVariant value = xlsx.read(row, col);

            QString text = value.toString();

            // Escape quotes for CSV
            text.replace("\"", "\"\"");

            // Wrap in quotes if needed
            if (text.contains(',') ||
                text.contains('"') ||
                text.contains('\n'))
            {
                text = "\"" + text + "\"";
            }

            fields << text;
        }

        out << fields.join(',') << "\n";
    }

    csvFile.close();

    qDebug() << "CSV created:" << csvPath;

    return csvPath;
}

void MainWindow::on_pushButton_backToModes_clicked()
{
    ui->stackedWidget->setCurrentIndex(4);
}

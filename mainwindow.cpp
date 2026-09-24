#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "testcontroller.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

#include <QToolButton>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    test = new TestController(this);
    test->setLogger(this);

    ui->lineEdit_userName->setPlaceholderText("👤 Type your username");
    ui->lineEdit_password->setPlaceholderText("🔒 Enter your password");
    ui->lineEdit_password->setEchoMode(QLineEdit::Password);

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



    ui->comboBox_ports->addItems(test->availablePorts());
    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));
    connect(test,&TestController::portOpening,this,&MainWindow::portStatus);
    connect(test,&TestController::executeWriteToNotes,this,&MainWindow::writeToNotes);

    test->welcome();
    resetLogFile();
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Started");
    //#################################################

    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");

    initializeDatabase();

    //patch model
    patchModel = new QSqlTableModel(this,db);
    ui->tableView_patch->setModel(patchModel);
    ui->tableView_patch->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    patchModel->setEditStrategy(QSqlTableModel::OnManualSubmit);

    //harness model
    harnessModel = new QSqlTableModel(this,db);
    ui->tableView_harness->setModel(harnessModel);
    ui->tableView_harness->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    harnessModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    ui->tableView_harness->verticalHeader()->setVisible(true);

    ui->tableView_patch->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableView_patch->setSelectionMode(QAbstractItemView::SingleSelection);
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

    QByteArray hashedPassword =QCryptographicHash::hash(password.toUtf8(),QCryptographicHash::Sha256).toHex();

    QSqlQuery q(db);

    q.prepare("SELECT role FROM loginData "
              "WHERE username=:u AND password=:p");

    q.bindValue(":u", username);
    q.bindValue(":p", hashedPassword);

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
    QByteArray hashedPassword =QCryptographicHash::hash(password.toUtf8(),QCryptographicHash::Sha256).toHex();

    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO loginData "
        "(username, password, role) "
        "VALUES "
        "(:username, :password, :role)"
    );

    query.bindValue(":username", username);
    query.bindValue(":password", hashedPassword);
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
    ui->stackedWidget->setCurrentWidget(ui->page_userPassword);
    role="user";
    ui->label_addNewUser->setText("Create User Account");
     adminPermissionFlag = true;
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

    // Hash password using SHA-256
    QByteArray hashedPassword =
            QCryptographicHash::hash(
                password.toUtf8(),
                QCryptographicHash::Sha256
                ).toHex();

    QSqlQuery query(db);

    query.prepare(
                "SELECT role FROM loginData "
                "WHERE role='admin' "
                "AND username=:u "
                "AND password=:p"
                );

    query.bindValue(":u", username);
    query.bindValue(":p", hashedPassword);

    if (query.exec() && query.next())
    {
        role = "user";

        if (adminPermissionFlag)
        {
            // Create User Page
            ui->stackedWidget->setCurrentIndex(2);
            adminPermissionFlag = false;
        }
        else
        {
            // Forgot Password Validation Page
            ui->stackedWidget->setCurrentIndex(8);
        }

        ui->label_titleForChangePassword
                ->setText("Recover User Account");

        ui->lineEdit_admin->clear();
        ui->lineEdit_adminPass->clear();
    }
    else
    {
        QMessageBox::warning(
                    this,
                    "Invalid",
                    "Invalid username or password"
                    );
    }
}


void MainWindow::on_pushButton_update_clicked()
{
    QString username = ui->lineEdit_usernameExist->text().trimmed();
    QString password = ui->lineEdit_changePassword->text().trimmed();
    QString confirmPassword = ui->lineEdit_confimPassword->text().trimmed();

    if (username.isEmpty() || password.isEmpty() || confirmPassword.isEmpty())
    {
        QMessageBox::information(
                    this,
                    "Empty Fields",
                    "Fill all the fields"
                    );
        return;
    }

    if (password != confirmPassword)
    {
        QMessageBox::warning(
                    this,
                    "Password Mismatch",
                    "Password and Confirm Password do not match."
                    );
        return;
    }

    if (!db.isOpen())
        return;

    // Hash the new password using SHA-256
    QByteArray hashedPassword =
            QCryptographicHash::hash(
                password.toUtf8(),
                QCryptographicHash::Sha256
                ).toHex();

    QSqlQuery query(db);

    query.prepare(
                "UPDATE loginData SET password=:pw "
                "WHERE username=:u AND role=:r"
                );

    query.bindValue(":pw", hashedPassword);
    query.bindValue(":u", username);
    query.bindValue(":r", role);

    qDebug() << "Updating password for:" << username;
    qDebug() << "Role:" << role;

    if (!query.exec())
    {
        qWarning() << "updatePassword failed:"
                   << query.lastError();
        return;
    }

    if (query.numRowsAffected() == 0)
    {
        QString msg = QString("Username not found in %1").arg(role);

        QMessageBox::warning(
                    this,
                    "Invalid User",
                    msg
                    );
        return;
    }

    QMessageBox::information(
                this,
                "Success",
                "Password updated successfully"
                );
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
    ui->comboBox_fileNames->clear();
    ui->comboBox_fileNames->addItems(getCableNames());
    ui->stackedWidget->setCurrentIndex(10);
}

void MainWindow::on_pushButton_uploadPatch_clicked()
{
      if(ui->lineEdit_cableName->text().trimmed().isEmpty())
      {
          QMessageBox::information(this,"Field Empty","Enter cable name");
          return;
      }
       ui->textEdit_ErrorLog->clear();
      QString tableName =
                  ui->lineEdit_cableName->text().trimmed() + "_patch";

          tableName.replace(" ", "_");

          // CHECK TABLE EXISTS
          if(db.tables().contains(tableName))
          {
              QMessageBox::warning(
                          this,
                          "Table Exists",
                          "Cable name already exists.");

              return;
          }

      openCsvFile("patch");
}


void MainWindow::on_pushButton_uploadCable_clicked()
{
    if(ui->lineEdit_cableName->text().trimmed().isEmpty())
    {
        QMessageBox::information(this,"Field Empty","Enter cable name");
        return;
    }

    ui->textEdit_ErrorLog->clear();

    QString tableName =
                ui->lineEdit_cableName->text().trimmed() + "_harness";

        tableName.replace(" ", "_");

        // CHECK TABLE EXISTS
        if(db.tables().contains(tableName))
        {
            QMessageBox::warning(this,"Table Exists", "Cable name already exists");

            return;
        }
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

    bool success = processCsvFile( fileToProcess, ui->lineEdit_cableName->text(),fileType);
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
         ui->textEdit_ErrorLog->append("❌ Failed to open file: " + fileName);
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
    QVector<QString> expTemp;
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

        if (std::all_of(parts.begin(), parts.end(),
                        [](const QString &s){ return s.trimmed().isEmpty(); }))
        {
            break; // use continue if you want to move further after empty line
        }

        if (type == "patch") {
            if (parts.size() != 5) {
                writeToNotes("Malformed PATCH line " + QString::number(lineNumber));
                ui->textEdit_ErrorLog->append("Malformed PATCH line " + QString::number(lineNumber));
                return false;
            }

            QString pCon = parts[3].trimmed();
            QString pPinStr = parts[4].trimmed();

            if (!validatePatchLine(lineNumber, pCon, pPinStr))
                return false;

            //-------------------------------------------------
            // Convert Logical TP(1-16) -> Physical TP(1-32)
            //-------------------------------------------------

            int tpNo = pCon.section('-', 1).toInt();
            int pPin = pPinStr.toInt();

            if (pPin <= 64)
            {
                tpNo = (tpNo * 2) - 1;
            }
            else
            {
                tpNo = tpNo * 2;
                pPin -= 64;
            }

            pCon = QString("TP-%1").arg(tpNo);
            pPinStr = QString::number(pPin);

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
        else if (type == "harness")
        {
\
            if (parts.size() != 6)
            {
                writeToNotes("❌ Malformed HARNESS line " + QString::number(lineNumber));
                 ui->textEdit_ErrorLog->append("❌ Malformed HARNESS line " + QString::number(lineNumber));
                return false;
            }
            cableTemp.append(parts[0].trimmed());
            sourceConTemp.append(parts[1].trimmed());
            sourcePinTemp.append(parts[2].trimmed());
            destConTemp.append(parts[3].trimmed());
            destPinTemp.append(parts[4].trimmed());
            expTemp.append(parts[5].trimmed());

        }
    }

    file.close();

    // 1️⃣ SAVE PATCH DATA HERE
    if (type == "patch") {
        if (!savePatchToDb(cableNameFromUI)) {
            writeToNotes("❌ Failed to save PATCH data to DB");
            ui->textEdit_ErrorLog->append("❌ Failed to save PATCH data to DB");
            return false;
        }
        writeToNotes("💾 Saved PATCH data to DB for cable: " + cableNameFromUI);
    }

    // Run final harness validations
    if (type == "harness")
    {
        if (!validateHarnessData(cableTemp,
                                 sourceConTemp, sourcePinTemp,
                                 destConTemp, destPinTemp,
                                 expTemp))
            return false;

        if (!saveHarnessToDb(cableNameFromUI,
                             cableTemp,
                             sourceConTemp, sourcePinTemp,
                             destConTemp, destPinTemp,
                             expTemp))
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

    if (!q.exec(create))
    {
        writeToNotes("❌ Failed to create table "
                     + tableName + ": "
                     + q.lastError().text());
        ui->textEdit_ErrorLog->append("❌ Failed to create table "
                                      + tableName + ": "
                                      + q.lastError().text());


        return false;
    }

    // Clear old data
    if(!q.exec("DELETE FROM " + tableName))
    {
        writeToNotes("❌ Failed to clear table "
                     + tableName);
        ui->textEdit_ErrorLog->append("❌ Failed to clear table "
                                      + tableName);

        return false;
    }

    // ⭐ START TRANSACTION
    db.transaction();

    q.prepare("INSERT INTO " + tableName +
              "(cable, userCon, userPin, patchCon, patchPin) "
              "VALUES (?, ?, ?, ?, ?)");

    for (int i = 0; i < cableList.size(); i++)
    {
        q.addBindValue(cableList[i]);
        q.addBindValue(userConList[i]);
        q.addBindValue(userPinList[i]);
        q.addBindValue(patchConList[i]);
        q.addBindValue(patchPinList[i]);

        if (!q.exec())
        {
            db.rollback();

            writeToNotes("❌ Insert failed in "
                         + tableName + ": "
                         + q.lastError().text());
            ui->textEdit_ErrorLog->append("❌ Insert failed in "
                                          + tableName + ": "
                                          + q.lastError().text());

            return false;
        }
    }

    // ⭐ COMMIT ONCE
    db.commit();

    writeToNotes("✅ Patch data stored in table: "
                 + tableName);

    return true;
}
bool MainWindow::validateHarnessData(const QVector<QString> &cableTemp,
                                     const QVector<QString> &sourceConTemp,
                                     const QVector<QString> &sourcePinTemp,
                                     const QVector<QString> &destConTemp,
                                     const QVector<QString> &destPinTemp,
                                     const QVector<QString> &expTemp)
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

//        if (matchSrc > 1)
//            extraLog += QString("⚠ EXTRA source append at index %1: %2-%3\n")
//                            .arg(i).arg(sourceConTemp[i]).arg(sourcePinTemp[i]);

//        if (matchDst > 1)
//            extraLog += QString("⚠ EXTRA destination append at index %1: %2-%3\n")
//                            .arg(i).arg(destConTemp[i]).arg(destPinTemp[i]);
    }

    if (!extraLog.isEmpty()) {
        ui->textEdit_ErrorLog->append(extraLog);
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
        ui->textEdit_ErrorLog->append("❌ " + missingLog);
        return false;
    }

    // Validate expValue
    for (int i = 0; i < expTemp.size(); i++) {
        QString v = expTemp[i];

        if ((!v.startsWith("<") && !v.startsWith(">")) ||
            v.mid(1).toInt() < 1 || v.mid(1).toInt() > 100)
        {
            writeToNotes(QString("Invalid expValue at index %1: %2")
                             .arg(i).arg(v));
            ui->textEdit_ErrorLog->append(
                "Invalid expValue at index " + QString::number(i) +
                ": " +v
            );
            return false;
        }
    }

    // Validate Voltage
//    for (int i = 0; i < voltTemp.size(); i++) {
//        QString v = voltTemp[i];

//        if (v != "250V" && v != "500V") {
//            writeToNotes(QString("❌ Invalid Voltage at index %1: %2")
//                             .arg(i).arg(v));
//            return false;
//        }
//    }

    // ---------------------------------------------------------
    // PRINT ALL HARNESS DATA AFTER SUCCESSFUL VALIDATION
    // ---------------------------------------------------------
    writeToNotes("######## HARNESS DATA (VALIDATED) ########");

    for (int i = 0; i < sourceConTemp.size(); i++) {

        QString log = QString(
                          "HARNESS Line OK (%1): Cable: %2 , SRC: %3-%4 → DEST: %5-%6 , exp: %7")
                          .arg(i + 1)
                          .arg(cableTemp[i])              // <--- NEW
                          .arg(sourceConTemp[i])
                          .arg(sourcePinTemp[i])
                          .arg(destConTemp[i])
                          .arg(destPinTemp[i])
                          .arg(expTemp[i]);

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
                                 const QVector<QString> &expTemp)
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
        "exp TEXT"
        ")";

    if (!q.exec(create)) {
        writeToNotes("❌ Failed to create table " + tableName + ": " +
                     q.lastError().text());

        ui->textEdit_ErrorLog->append("❌ Failed to create table " + tableName + ": " +
                                      q.lastError().text());
        return false;
    }

    if (!q.exec("DELETE FROM " + tableName)) {
        writeToNotes("❌ Failed to clear table " + tableName + ": " +
                     q.lastError().text());
        ui->textEdit_ErrorLog->append("❌ Failed to clear table " + tableName + ": " +
                                      q.lastError().text());
        return false;
    }

    if (!db.transaction()) {
        writeToNotes("❌ Failed to start transaction: " +
                     db.lastError().text());
        ui->textEdit_ErrorLog->append("❌ Failed to clear table " + tableName + ": " +
                                      q.lastError().text());
        return false;
    }

    q.prepare("INSERT INTO " + tableName +
              " (cable, sourceCon, sourcePin, destCon, destPin, exp) "
              "VALUES (?, ?, ?, ?, ?, ?)");

    for (int i = 0; i < sourceConTemp.size(); i++)
    {
        q.addBindValue(cableTemp[i]);
        q.addBindValue(sourceConTemp[i]);
        q.addBindValue(sourcePinTemp[i]);
        q.addBindValue(destConTemp[i]);
        q.addBindValue(destPinTemp[i]);
        q.addBindValue(expTemp[i]);

        if (!q.exec())
        {
            db.rollback();

            writeToNotes("❌ Insert failed in " + tableName + ": " +
                         q.lastError().text());
            ui->textEdit_ErrorLog->append("❌ Insert failed in " + tableName + ": " +
                                          q.lastError().text());

            return false;
        }
    }

    if (!db.commit()) {
        writeToNotes("❌ Failed to commit transaction: " +
                     db.lastError().text());
        ui->textEdit_ErrorLog->append("❌ Failed to commit transaction: " +
                                      db.lastError().text());
        return false;
    }

    writeToNotes("✅ Harness data stored in table: " + tableName);
    return true;
}

bool MainWindow::validatePatchLine(int lineNumber,
                                   const QString &pCon,
                                   const QString &pPinStr)
{
    // Valid TP-1 to TP-16
    QRegularExpression tpRegex("^TP-?(?:[1-9]|1[0-6])$");

    if (!tpRegex.match(pCon).hasMatch())
    {
        writeToNotes(QString("Invalid PatchCon at line %1: %2 (must be TP-1 to TP-16)")
                         .arg(lineNumber)
                         .arg(pCon));
        ui->textEdit_ErrorLog->append(QString(
                                          "Invalid PatchCon at line %1: %2 (must be TP-1)")
                                      .arg(lineNumber)
                                      .arg(pCon));
        return false;
    }

    bool ok = false;
    int pPin = pPinStr.toInt(&ok);

    if (!ok || pPin < 1 || pPin > 128)
    {
        writeToNotes(QString("Invalid PatchPin at line %1: %2 (must be 1–128)")
                         .arg(lineNumber)
                         .arg(pPinStr));
        ui->textEdit_ErrorLog->append(QString(
                                          "Invalid PatchPin at line %1: %2 (must be 1–128)")
                                      .arg(lineNumber)
                                      .arg(pPinStr));

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
    out.setCodec("UTF-8");

    QXlsx::CellRange range = xlsx.dimension();
    int firstRow = range.firstRow();
    int lastRow  = range.lastRow();

    int firstCol = range.firstColumn();
    int lastCol  = range.lastColumn();
    qDebug()<<"last column:"<<lastCol;

    for (int row = firstRow; row <= lastRow; ++row)
    {
        QStringList fields;

        for (int col = firstCol; col <= lastCol; ++col)
        {
            QVariant value = xlsx.read(row, col);

            QString text = value.toString();

            // Remove line breaks
            text.replace("\r", " ");
            text.replace("\n", " ");

            // Replace commas with spaces
            text.replace(",", " ");

            // Optional: remove quotes too
            text.replace("\"", "");

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

void MainWindow::on_pushButton_getDetails_clicked()
{
    QString tableName = ui->comboBox_fileNames->currentText();
    ui->pushButton_patchCancel->hide();

    if(tableName.isEmpty())
    {
        QMessageBox::information(this,"Missing file Name","Select file from dropdown");
        return;
    }
    if(tableName=="Files")
    {
        QMessageBox::information(this,"Invalid","Select valid fileName");
        return;
    }

    ui->tabWidget->setCurrentIndex(0);
    ui->stackedWidget->setCurrentIndex(11);

    loadPatchTable(tableName+"_patch");
    ui->label_fileName->setText(tableName);
    loadHarnessTable(tableName+"_harness");
    ui->label_fileNameH->setText(tableName);

}

void MainWindow::on_tabWidget_tabBarClicked(int index)
{
    if(index == 2)
    {
        ui->stackedWidget->setCurrentIndex(10);
    }
}

void MainWindow::loadPatchTable(const QString &tableName)
{
    qDebug() << tableName << "**********";

    patchModel->setTable(tableName);

    if(!patchModel->select())
    {
        qDebug() << patchModel->lastError().text();
        return;
    }

    while(patchModel->canFetchMore())
        patchModel->fetchMore();

    QStandardItemModel *displayModel = new QStandardItemModel(this);

    displayModel->setHorizontalHeaderLabels(
                {"ID","Cable","User Con","User Pin","Patch Con","Patch Pin"});

    for(int row = 0; row < patchModel->rowCount(); ++row)
    {
        QList<QStandardItem*> items;

        for(int col = 0; col < patchModel->columnCount(); ++col)
        {
            QString value = patchModel->index(row,col).data().toString();

            if(col == 4)       // Patch Con
            {
                int tpNo = value.section('-',1).toInt();

                if(tpNo % 2)
                    tpNo = (tpNo + 1) / 2;
                else
                    tpNo /= 2;

                value = QString("TP-%1").arg(tpNo);
            }
            else if(col == 5)  // Patch Pin
            {
                int pin = value.toInt();

                QString dbCon = patchModel->index(row,4).data().toString();
                int tpNo = dbCon.section('-',1).toInt();

                if(tpNo % 2 == 0)
                    pin += 64;

                value = QString::number(pin);
            }

            items.append(new QStandardItem(value));
        }

        displayModel->appendRow(items);
    }

    ui->tableView_patch->setModel(displayModel);

    ui->tableView_patch->hideColumn(0);

    displayModel->setHeaderData(1,Qt::Horizontal,"Cable");
    displayModel->setHeaderData(2,Qt::Horizontal,"User Con");
    displayModel->setHeaderData(3,Qt::Horizontal,"User Pin");
    displayModel->setHeaderData(4,Qt::Horizontal,"Patch Con");
    displayModel->setHeaderData(5,Qt::Horizontal,"Patch Pin");
}

void MainWindow::loadHarnessTable(const QString &tableName)
{
    harnessModel->setTable(tableName);

    harnessModel->select();

    harnessModel->setHeaderData( 1, Qt::Horizontal,"Cable");
    harnessModel->setHeaderData(2,Qt::Horizontal,"Source Con");
    harnessModel->setHeaderData(3,Qt::Horizontal,"Source Pin");
    harnessModel->setHeaderData(4,Qt::Horizontal,"Dest Con");
    harnessModel->setHeaderData(5,Qt::Horizontal, "Dest Pin");
    harnessModel->setHeaderData(6,Qt::Horizontal,"Exp");
    harnessModel->setHeaderData(7,Qt::Horizontal,"Volt");
    ui->tableView_harness->hideColumn(0);
}

void MainWindow::on_pushButton_editTableView_clicked()
{
    ui->tableView_harness->setEditTriggers(
            QAbstractItemView::DoubleClicked |
            QAbstractItemView::SelectedClicked |
            QAbstractItemView::EditKeyPressed
        );
    QMessageBox::information(this,"Editable","Now you can edit table");
}

void MainWindow::on_pushButton_saveChanges_clicked()
{

    if(harnessModel->submitAll())
    {
        QMessageBox::information(this,"Success", "Changes saved successfully");
      // Disable editing again
        ui->tableView_harness->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    else
    {
        QMessageBox::warning(this,"Error",harnessModel->lastError().text());
    }
}

void MainWindow::on_pushButton_revertChanges_clicked()
{
    harnessModel->revertAll();
    ui->tableView_harness->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::on_pushButton_patchEdit_clicked()
{
    ui->pushButton_patchCancel->setVisible(true);
    ui->tableView_patch->setEditTriggers(
            QAbstractItemView::DoubleClicked |
            QAbstractItemView::SelectedClicked |
            QAbstractItemView::EditKeyPressed
        );
    QMessageBox::information(this,"Editable","Now you can edit table");
}

void MainWindow::on_pushButton_patchSave_clicked()
{

    if(patchModel->submitAll())
    {
        patchModel->select();
        QMessageBox::information(this,"Success","Changes saved successfully");
       // Disable editing again
        ui->tableView_patch->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
    else
    {
        QMessageBox::warning(this, "Error", patchModel->lastError().text());
    }
}

void MainWindow::on_pushButton_patchCancel_clicked()
{
   patchModel->revertAll();
    ui->tableView_patch->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::on_pushButton_delCable_clicked()
{
    qDebug()<<ui->comboBox_fileNames->currentIndex()<<"";
    if(ui->comboBox_fileNames->currentIndex()==0)
    {
        QMessageBox::information(this,"Missing Selection","Please select a cable to delete.");
        return;
    }
    QString selectedFile = ui->comboBox_fileNames->currentText();

    QMessageBox::StandardButton reply;
    QString msg=QString("Do you want to Delete Cable %1").arg(selectedFile);
    reply = QMessageBox::question(this,"Confirm",msg,QMessageBox::Yes|QMessageBox::No);
    if(reply == QMessageBox::Yes)
    {
        writeToNotes("calling del is problem");
        bool del =deleteCable(selectedFile);
        writeToNotes("del");
        if(del)
        {
            QMessageBox::information(this,"Success","Cable deleted successfully");
            ui->comboBox_fileNames->clear();
            ui->comboBox_fileNames->addItems(getCableNames());
            ui->comboBox_fileNames->setCurrentIndex(0);
        }
        else
        {
            QMessageBox::information(this,"Failed","Failed to delete");
        }
    }
    else
    {
        QMessageBox::information(this,"Failed","Delete cancelled");
    }
}
bool MainWindow::deleteCable(const QString &cableName)
{
    if (!db.isOpen())
    {
        writeToNotes("DB not open");
        return false;
    }

    QString patchTable = cableName + "_patch";

    QString harnessTable = cableName + "_harness";

    // Clear loaded tables from models
    if(patchModel)
    {
        patchModel->clear();
        patchModel->setTable("");
    }

    if(harnessModel)
    {
        harnessModel->clear();
        harnessModel->setTable("");
    }

    QApplication::processEvents();

    QSqlQuery q(db);

    // Drop patch table
    if (!q.exec("DROP TABLE IF EXISTS \"" + patchTable + "\""))
    {
        writeToNotes(q.lastError().text());

        return false;
    }

    // Drop harness table
    if (!q.exec("DROP TABLE IF EXISTS \"" +harnessTable + "\""))
    {
        writeToNotes(q.lastError().text());

        return false;
    }

    writeToNotes("Cable deleted");

    return true;
}

QVector<QByteArray> MainWindow::constructUARTPacketsForTwoWire(
    const QVector<QString> &sourceCon,
    const QVector<QString> &sourcePin,
    const QVector<QString> &destCon,
    const QVector<QString> &destPin)
{
    QVector<QByteArray> packets;

    constexpr int MAX_PACKET_SIZE = 1024;
    constexpr int HEADER_SIZE     = 6;
    constexpr int ENTRY_SIZE      = 4;

    const int maxEntriesPerPacket = (MAX_PACKET_SIZE - HEADER_SIZE) / ENTRY_SIZE;

    quint16 packetNo = 1;

    for (int start = 0; start < sourceCon.size(); start += maxEntriesPerPacket)
    {
        QByteArray packet;

        //-------------------------------------------------
        // Header
        //-------------------------------------------------

        packet.append(char(0x2F));
        packet.append(char(0x2F));

        // Packet Number (MSB first)
        packet.append(char((packetNo >> 8) & 0xFF));
        packet.append(char(packetNo & 0xFF));

        // Reserve Packet Length (filled later)
        packet.append(char(0x00));
        packet.append(char(0x00));

        //-------------------------------------------------
        // Payload
        //-------------------------------------------------

        int end = qMin(start + maxEntriesPerPacket,
                       sourceCon.size());

        for (int i = start; i < end; ++i)
        {
            if (!sourceCon[i].startsWith("TP-") ||
                !destCon[i].startsWith("TP-"))
            {
                QMessageBox::critical(
                    this,
                    "Invalid Connector Format",
                    QString("Invalid connector format at row %1.\n\n"
                            "Expected format: TP-<number>\n\n"
                            "Source      : %2\n"
                            "Destination : %3")
                        .arg(i + 1)
                        .arg(sourceCon[i])
                        .arg(destCon[i]));

                return {};
            }

            quint8 srcCon = static_cast<quint8>(
                                sourceCon[i].section('-', 1, 1).toUInt());

            quint8 srcPin = static_cast<quint8>(sourcePin[i].toUInt());

            quint8 dstCon = static_cast<quint8>(
                                destCon[i].section('-', 1, 1).toUInt());

            quint8 dstPin = static_cast<quint8>(destPin[i].toUInt());

            packet.append(char(srcCon));
            packet.append(char(srcPin));
            packet.append(char(dstCon));
            packet.append(char(dstPin));
        }

        //-------------------------------------------------
        // Update Packet Length
        //-------------------------------------------------

        quint16 packetLength = packet.size() - 6;

        packet[4] = char((packetLength >> 8) & 0xFF);
        packet[5] = char(packetLength & 0xFF);

        packets.append(packet);

        packetNo++;
    }

    return packets;
}

QByteArray MainWindow::constructTwoWireStartPacket(quint16 totalPackets)
{
    QByteArray packet;

    packet.append(char(0x53));
    packet.append(char(0x4B));
    packet.append(char(0x32));

    // Float Voltage
    float voltage = static_cast<float>(ui->doubleSpinBox_voltage->value());

    union
    {
        float f;
        quint8 b[4];
    } data;

    data.f = voltage;

    packet.append(char(data.b[3]));
    packet.append(char(data.b[2]));
    packet.append(char(data.b[1]));
    packet.append(char(data.b[0]));

    // Total packet count
    packet.append(char((totalPackets >> 8) & 0xFF));
    packet.append(char(totalPackets & 0xFF));

    // XOR checksum
    quint8 checksum = 0;

    for(char c : packet)
        checksum ^= static_cast<quint8>(c);

    packet.append(char(checksum));

    return packet;
}

void MainWindow::portStatus(const QString &data)
{
    if(data.startsWith("Serial object is not initialized/port not selected"))
    {
        QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 921600"))
    {
        QMessageBox::information(this,"Success",data);
    }

    if(data.startsWith("Failed to open port"))
    {

        QMessageBox::critical(this,"Error",data);
    }

}

void MainWindow::onPortSelected(const QString &portName)
{
  test->setPORTNAME(portName);
}

void MainWindow::on_pushButton_test_clicked()
{
    ui->stackedWidget->setCurrentIndex(12);
    ui->comboBox_files->clear();
    ui->comboBox_files->addItems(getCableNames());
}

void MainWindow::on_pushButton_run_clicked()
{
    if(!test->isConnected())
    {
        QMessageBox::information(this,"Port not connected","Please connect to hardware before running test");
        return;
    }

    if(ui->comboBox_files->currentIndex()==0||ui->lineEdit_inspectedBy->text()==""
            ||ui->lineEdit_setNo->text()==""||ui->lineEdit_performedBy->text()==""
            ||ui->lineEdit_projectName->text()==""||ui->lineEdit_notes->text()==""
            ||ui->comboBox_test->currentIndex()==0)
    {
        QMessageBox::information(this,"Empty Fields","Please fill all details,select file and choose test");
        return;
    }

    else
    {
      QString cableName = ui->comboBox_files->currentText();
      QVariantList patch = getPatchData(cableName);
      QVariantList harness = getHarnessData(cableName);
      if(patch.length() ==0||harness.length()==0)
      {
          QMessageBox::information(this,"Data Missing","No Patch/Harness data found for this cable!");
          return;
      }
      m_setNo       = ui->lineEdit_setNo->text();
      m_performedBy = ui->lineEdit_performedBy->text();
      m_inspectedBy = ui->lineEdit_inspectedBy->text();
      m_projectName = ui->lineEdit_projectName->text();
      m_testType    = ui->comboBox_test->currentText();


      writeToNotes("=== TEST DATA RECEIVED FROM ");
      writeToNotes("Cable     : " + cableName);
      writeToNotes("Set No    : " + m_setNo);
      writeToNotes("Performed : " + m_performedBy);
      writeToNotes("Inspected : " + m_inspectedBy);
      writeToNotes("Project   : " + m_projectName);
      //writeToNotes("Notes     : " + notes);
      writeToNotes("Test Type : " + m_testType);

      if (!test->mapLogicalToHardware(patch, harness)) {
          QMessageBox::information(this,"Mapping Failed","Patch to Harness mapping failed.");
          return;
      }


      // Two Wire BLOCK Start------------------------

      if(ui->comboBox_test->currentText() == "Two Wire Continuity")
      {
        writeToNotes("Starting Two Wire Continuity");

        QVector<QByteArray> packets =
                constructUARTPacketsForTwoWire(
                    test->get_k_SourceCon(),
                    test->get_k_SourcePin(),
                    test->get_k_DestinationCon(),
                    test->get_k_DestinationPin());


        QByteArray startPacket =
                constructTwoWireStartPacket(packets.size());

        test->startTwoWireTransmission(startPacket,
                                       packets);

      }

      // Two Wire BLOCK End------------------------


      // Insulation Packet logic start ------------------------------------------------

      if(ui->comboBox_test->currentText() == "Insulation")
      {
          writeToNotes("======================================");
          writeToNotes("STEP-1 : DIVIDING HARNESS INTO LOOMS");
          writeToNotes("======================================");

          QMap<QString, QVector<HarnessConnection>> looms;

          QVector<QString> cable     = test->get_Cable();
          QVector<QString> sourceCon = test->get_k_SourceCon();
          QVector<QString> sourcePin = test->get_k_SourcePin();
          QVector<QString> destCon   = test->get_k_DestinationCon();
          QVector<QString> destPin   = test->get_k_DestinationPin();
          QVector<QString> exp       = test->get_expResistance();

          for(int i = 0; i < cable.size(); ++i)
          {
              HarnessConnection row;

              row.cable     = cable[i];
              row.sourceCon = sourceCon[i];
              row.sourcePin = sourcePin[i];
              row.destCon   = destCon[i];
              row.destPin   = destPin[i];
              row.exp       = exp[i];

              looms[cable[i]].append(row);
          }

          // Printing Looms
          for(auto it = looms.begin(); it != looms.end(); ++it)
          {
              writeToNotes("");
              writeToNotes(QString("******** %1 ********").arg(it.key()));

              const QVector<HarnessConnection> &rows = it.value();

              for(const HarnessConnection &r : rows)
              {
                  writeToNotes(QString("%1 : %2  --->  %3 : %4")
                               .arg(r.sourceCon)
                               .arg(r.sourcePin)
                               .arg(r.destCon)
                               .arg(r.destPin));
              }
          }

          writeToNotes("");
          writeToNotes(QString("Total Looms : %1").arg(looms.size()));

          // Final storer
          QVector<QByteArray> loomPackets;

          //Loom wise net creation

          quint16 loomNumber = 0;

          for(auto it = looms.begin(); it != looms.end(); ++it)
          {
              writeToNotes("");
              writeToNotes(QString("========================================"));
              writeToNotes(QString("PROCESSING LOOM : %1").arg(it.key()));
              writeToNotes(QString("========================================"));

              const QVector<HarnessConnection> &rows = it.value();

              // ---------------------------------------------------
              // STEP-2 : Create endpoint pairs (same as old net logic)
              // ---------------------------------------------------

              QVector<QPair<QPair<QString,int>,QPair<QString,int>>> pairs;

              for(const HarnessConnection &r : rows)
              {
                  pairs.append(
                      qMakePair(
                          qMakePair(r.sourceCon, r.sourcePin.toInt()),
                          qMakePair(r.destCon,   r.destPin.toInt())
                      )
                  );
              }

              // ---------------------------------------------------
              // STEP-3 : Merge into Nets (same algorithm as before)
              // ---------------------------------------------------

              QVector<QVector<QPair<QString,int>>> fixedGroups;

              for(int i=0;i<pairs.size();++i)
              {
                  QPair<QString,int> first  = pairs[i].first;
                  QPair<QString,int> second = pairs[i].second;

                  QVector<int> groupIndices;

                  for(int j=0;j<fixedGroups.size();++j)
                  {
                      if(fixedGroups[j].contains(first) ||
                         fixedGroups[j].contains(second))
                      {
                          groupIndices.append(j);
                      }
                  }

                  if(groupIndices.isEmpty())
                  {
                      QVector<QPair<QString,int>> newGroup;
                      newGroup.append(first);
                      newGroup.append(second);

                      fixedGroups.append(newGroup);
                  }
                  else
                  {
                      int mainIndex = groupIndices[0];

                      for(int k=1;k<groupIndices.size();++k)
                      {
                          int mergeIndex = groupIndices[k];

                          fixedGroups[mainIndex] += fixedGroups[mergeIndex];
                          fixedGroups[mergeIndex].clear();
                      }

                      if(!fixedGroups[mainIndex].contains(first))
                          fixedGroups[mainIndex].append(first);

                      if(!fixedGroups[mainIndex].contains(second))
                          fixedGroups[mainIndex].append(second);
                  }
              }

              // Remove merged empty groups
              fixedGroups.erase(
                          std::remove_if(
                              fixedGroups.begin(),
                              fixedGroups.end(),
                              [](const QVector<QPair<QString,int>> &g)
                              {
                                  return g.isEmpty();
                              }),
                          fixedGroups.end());

              // ---------------------------------------------------
              // STEP-4 : Print Nets
              // ---------------------------------------------------

              writeToNotes(QString("TOTAL NETS : %1").arg(fixedGroups.size()));

              for(int net=0; net<fixedGroups.size(); ++net)
              {
                  writeToNotes("");
                  writeToNotes(QString("NET-%1").arg(net));

                  for(const auto &point : fixedGroups[net])
                  {
                      writeToNotes(
                                  QString("   %1 : %2")
                                  .arg(point.first)
                                  .arg(point.second));
                  }
              }

              // ---------------------------------------------------
              // STEP-4 : Print Packet Structure
              // ---------------------------------------------------

              int connectorDataLength = 2;    // uint16 No Of Nets

              writeToNotes("");
              writeToNotes(QString("LOOM : %1").arg(it.key()));
              writeToNotes(QString("TOTAL NETS : %1").arg(fixedGroups.size()));

              // Calculate Connector Data Length
              for(int net = 0; net < fixedGroups.size(); ++net)
              {
                  int netDataLength = fixedGroups[net].size() * 2; // connector + pin

                  connectorDataLength +=
                          2 +                 // Net No
                          2 +                 // Net Data Length
                          netDataLength;      // Net Data
              }

              writeToNotes(QString("Connector Data Length : %1 Bytes")
                           .arg(connectorDataLength));

              writeToNotes(QString("Number Of Nets : %1")
                           .arg(fixedGroups.size()));

              writeToNotes("");

              for(int net = 0; net < fixedGroups.size(); ++net)
              {
                  int netDataLength = fixedGroups[net].size() * 2;

                  writeToNotes(QString("------------ NET %1 ------------").arg(net));
                  writeToNotes(QString("Net Number      : %1").arg(net));
                  writeToNotes(QString("Net Data Length : %1 Bytes").arg(netDataLength));

                  for(const auto &point : fixedGroups[net])
                  {
                      writeToNotes(QString("   %1 : %2")
                                   .arg(point.first)
                                   .arg(point.second));
                  }

                  writeToNotes("");
              }

              //Loom packets storage start -----------------------------------
              QByteArray loomPacket;

              auto appendUInt16 = [&](quint16 value)
              {
                  // MSB first (same as your previous protocol)
                  loomPacket.append(char((value >> 8) & 0xFF));
                  loomPacket.append(char(value & 0xFF));
              };

              appendUInt16(loomNumber);
              appendUInt16(connectorDataLength);
              appendUInt16(fixedGroups.size());

              for(int net = 0; net < fixedGroups.size(); ++net)
              {
                  int netDataLength = fixedGroups[net].size() * 2;

                  appendUInt16(net);

                  appendUInt16(netDataLength);

                  for(const auto &point : fixedGroups[net])
                  {
                      QString connector = point.first;

                      // TP-1 -> 1
                      // TP-12 -> 12

                      quint8 connectorNo =
                              connector.mid(3).toUInt();

                      quint8 pin =
                              static_cast<quint8>(point.second);

                      loomPacket.append(char(connectorNo));
                      loomPacket.append(char(pin));
                  }
              }

              loomPackets.append(loomPacket);

              writeToNotes(QString("Serialized Loom %1 : %2 bytes")
                           .arg(loomNumber)
                           .arg(loomPacket.size()));

              writeToNotes(loomPacket.toHex(' '));

              loomNumber++;

              //Loom packets storage end -----------------------------------
          }

          //UART Packet Creation of 1024 starts ------------------------

          writeToNotes("");
          writeToNotes("======================================");
          writeToNotes("STEP-5 : BUILDING UART PACKETS");
          writeToNotes("======================================");

          QVector<QByteArray> uartPackets;

          QByteArray currentPacket;

          quint16 packetNumber = 0;

          auto appendUInt16Packet = [&](QByteArray &packet, quint16 value)
          {
              packet.append(char((value >> 8) & 0xFF));
              packet.append(char(value & 0xFF));
          };

          currentPacket.append(char(0x2F));
          currentPacket.append(char(0x2F));

          appendUInt16Packet(currentPacket, packetNumber);

          // Placeholder for Packet Length
          appendUInt16Packet(currentPacket, 0);

          const int MAX_PACKET_SIZE = 1024;

          for(const QByteArray &loom : loomPackets)
          {
              if(currentPacket.size() + loom.size() > MAX_PACKET_SIZE)
              {
                  quint16 packetLength = currentPacket.size() - 6;

                  currentPacket[4] = char((packetLength >> 8) & 0xFF);
                  currentPacket[5] = char(packetLength & 0xFF);

                  uartPackets.append(currentPacket);

                  packetNumber++;

                  currentPacket.clear();

                  currentPacket.append(char(0x2F));
                  currentPacket.append(char(0x2F));

                  appendUInt16Packet(currentPacket, packetNumber);

                  appendUInt16Packet(currentPacket, 0);
              }

              currentPacket.append(loom);
          }

          if(currentPacket.size() > 6)
          {
              quint16 packetLength = currentPacket.size() - 6;

              currentPacket[4] = char((packetLength >> 8) & 0xFF);
              currentPacket[5] = char(packetLength & 0xFF);

              uartPackets.append(currentPacket);
          }

          writeToNotes("");

          for(int i = 0; i < uartPackets.size(); i++)
          {
              writeToNotes(QString("--------------------------------"));
              writeToNotes(QString("UART Packet %1").arg(i));
              writeToNotes(QString("Size : %1 Bytes").arg(uartPackets[i].size()));
              writeToNotes(uartPackets[i].toHex(' '));
          }

          //UART Packet Creation of 1024 ends --------------------------


      }

      // Insulation Packet logic end ------------------------------------------------

    }
}

QVariantList MainWindow::getPatchData(const QString &cableName)
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

QVariantList MainWindow::getHarnessData(const QString &cableName)
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
        "SELECT cable, sourceCon, sourcePin, destCon, destPin, exp "
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

        rows.append(m);
    }

    writeToNotes(QString("getHarnessData: %1 rows for %2")
                     .arg(rows.size())
                     .arg(cableName));

    return rows;
}


void MainWindow::on_pushButton_back_clicked()
{
    ui->stackedWidget->setCurrentIndex(4);
}

void MainWindow::on_pushButton_addRow_clicked()
{
    ui->tableView_patch->setEditTriggers(
            QAbstractItemView::DoubleClicked |
            QAbstractItemView::SelectedClicked |
            QAbstractItemView::EditKeyPressed
    );

    int row = patchModel->rowCount();

    patchModel->insertRow(row);

    ui->tableView_patch->selectRow(row);

    ui->tableView_patch->scrollTo(patchModel->index(row,0));
}
void MainWindow::on_pushButton_delRow_clicked()
{

    QModelIndex index = ui->tableView_patch->currentIndex();

    if(!index.isValid())
    {
        QMessageBox::information(this, "Selection","Select row to delete");
        return;
    }

    int row = index.row();

    patchModel->removeRow(row);
    if(patchModel->submitAll())
    {
        QMessageBox::information(this,"Success", "Row deleted successfully");
    }
    else
    {
        QMessageBox::warning(this,"Error",patchModel->lastError().text());
    }
}

void MainWindow::on_pushButton_addHarnessRow_clicked()
{
    ui->tableView_harness->setEditTriggers(
            QAbstractItemView::DoubleClicked |
            QAbstractItemView::SelectedClicked |
            QAbstractItemView::EditKeyPressed
    );

    int row = harnessModel->rowCount();

    harnessModel->insertRow(row);

    ui->tableView_harness->selectRow(row);

    ui->tableView_harness->scrollTo(harnessModel->index(row,0));
}

void MainWindow::on_pushButton_delHarnessRow_clicked()
{
    QModelIndex index =
            ui->tableView_harness->currentIndex();

    if(!index.isValid())
    {
        QMessageBox::information(this, "Selection","Select row to delete");
        return;
    }

    int row = index.row();

    harnessModel->removeRow(row);
    if(harnessModel->submitAll())
    {
        QMessageBox::information(this,"Success", "Row deleted successfully");
    }
    else
    {
        QMessageBox::warning(this,"Error",harnessModel->lastError().text());
    }
}

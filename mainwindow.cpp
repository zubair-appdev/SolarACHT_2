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

    // For Live Data Upload
    connect(test,
            &TestController::twoWireResultReceived,
            this,
            [this](float value, int index)
    {
        if (!m_twoWireTestModel)
            return;

        if (index < 0 ||
            index >= m_twoWireTestModel->rowCount())
            return;

        // =====================================================
        // 1. Update Measured value
        // =====================================================

        // Column 7 = Measured
        m_twoWireTestModel->setData(
            m_twoWireTestModel->index(index, 7),
            QString::number(value, 'f', 3));

        // =====================================================
        // 2. Select current row
        // =====================================================

        ui->tableView_twoWireTest->selectRow(index);

        // =====================================================
        // 3. Scroll to current row
        // =====================================================

        ui->tableView_twoWireTest->scrollTo(
            m_twoWireTestModel->index(index, 0),
            QAbstractItemView::PositionAtBottom);

        // =====================================================
        // 4. Update progress bar
        // =====================================================

        int totalRows =
                m_twoWireTestModel->rowCount();

        ui->progressBar_twoWire->setMaximum(totalRows);
        ui->progressBar_twoWire->setValue(index + 1);
    });

    // Two wire test completed vector
    connect(test,
            &TestController::twoWireResultsCompleted,
            this,
            &MainWindow::processTwoWireResults);


    // Self test results signals
    connect(test,
            &TestController::selfTestSlotResult,
            this,
            &MainWindow::onSelfTestSlotResult);

    connect(test,
            &TestController::selfTestResultsCompleted,
            this,
            &MainWindow::onSelfTestResultsCompleted);

    connect(test,
            &TestController::selfTestSlotStarted,
            this,
            &MainWindow::onSelfTestSlotStarted);

    // Calibration test results signals
    connect(test,
            &TestController::calibrationSlotReceived,
            this,
            &MainWindow::onCalibrationSlotReceived);

    connect(test,
            &TestController::calibrationSlotMissing,
            this,
            &MainWindow::onCalibrationSlotMissing);

    connect(test,
            &TestController::calibrationCompleted,
            this,
            &MainWindow::onCalibrationCompleted);

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

    // ---------------------------------------------------------
    // Store database in user's writable AppData folder
    // ---------------------------------------------------------

    QString appDataPath =
            QStandardPaths::writableLocation(
                QStandardPaths::AppLocalDataLocation);

    // Create directory if it doesn't exist
    if (!QDir().mkpath(appDataPath))
    {
        qWarning() << "Failed to create AppData directory:"
                   << appDataPath;

        writeToNotes("Failed to create AppData directory: "
                     + appDataPath);

        return false;
    }

    QString dbPath =
            appDataPath + "/ACHT.db";

    db.setDatabaseName(dbPath);

    // ---------------------------------------------------------
    // Open database
    // ---------------------------------------------------------

    if (!db.open())
    {
        qWarning() << "Failed to open database:"
                   << db.lastError().text();

        writeToNotes("Failed To Open Database "
                     + db.lastError().text());

        return false;
    }

    qDebug() << "Database file:" << db.databaseName()
             << "isOpen:" << db.isOpen()
             << "connection name:" << db.connectionName();

    writeToNotes("Database file: " + db.databaseName()
                 + " isOpen : " + QString::number(db.isOpen())
                 + " connection name : " + db.connectionName());

    // ---------------------------------------------------------
    // Create loginData table if it doesn't exist
    // ---------------------------------------------------------

    QSqlQuery query(db);

    QString createTable =
            "CREATE TABLE IF NOT EXISTS loginData ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "username TEXT NOT NULL UNIQUE, "
            "password TEXT NOT NULL, "
            "role TEXT NOT NULL)";

    if (!query.exec(createTable))
    {
        qWarning() << "Failed to create table:"
                   << query.lastError().text();

        writeToNotes("Failed to create table: "
                     + query.lastError().text());

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

    // =====================================================
    // Validate expValue
    //
    // Comparison:
    // <2
    // >5
    // <2K
    // >5K
    // <9M
    // >9M
    //
    // Range:
    // 20K ± 3
    // 20K ± 3K
    // 100 ± 5
    // 9M ± 2M
    //
    // =====================================================

    QRegularExpression expRegex(
        R"(^(?:[<>][0-9]+[KM]?|[0-9]+[KM]?\s*±\s*[0-9]+[KM]?)$)",
        QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < expTemp.size(); i++)
    {
        QString v = expTemp[i].trimmed();

        if (!expRegex.match(v).hasMatch())
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

void MainWindow::populateTwoWireTestTable(
        const QVariantList &harness)
{
    // =====================================================
    // Create model if it doesn't exist
    // =====================================================

    if (!m_twoWireTestModel)
    {
        m_twoWireTestModel =
                new QStandardItemModel(this);

        ui->tableView_twoWireTest->setModel(
                    m_twoWireTestModel);
    }

    // =====================================================
    // Clear previous data
    // =====================================================

    m_twoWireTestModel->clear();

    // =====================================================
    // Set columns
    // =====================================================

    m_twoWireTestModel->setColumnCount(10);

    m_twoWireTestModel->setHorizontalHeaderLabels({
        "S.No",
        "Cable",
        "Source Connector",
        "Source Pin",
        "Destination Connector",
        "Destination Pin",
        "Expected",
        "Measured",
        "Result",
        "Remarks"
    });

    // =====================================================
    // Make header text bold
    // =====================================================

    QFont headerFont =
            ui->tableView_twoWireTest->horizontalHeader()->font();

    headerFont.setBold(true);

    ui->tableView_twoWireTest->horizontalHeader()
            ->setFont(headerFont);

    // =====================================================
    // Fill harness data
    // =====================================================

    for (int row = 0; row < harness.size(); ++row)
    {
        QVariantMap data =
                harness[row].toMap();

        // -------------------------------------------------
        // S.No
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    0,
                    new QStandardItem(
                        QString::number(row + 1)));

        // -------------------------------------------------
        // Cable
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    1,
                    new QStandardItem(
                        data["cable"].toString()));

        // -------------------------------------------------
        // Source Connector
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    2,
                    new QStandardItem(
                        data["sourceCon"].toString()));

        // -------------------------------------------------
        // Source Pin
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    3,
                    new QStandardItem(
                        data["sourcePin"].toString()));

        // -------------------------------------------------
        // Destination Connector
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    4,
                    new QStandardItem(
                        data["destCon"].toString()));

        // -------------------------------------------------
        // Destination Pin
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    5,
                    new QStandardItem(
                        data["destPin"].toString()));

        // -------------------------------------------------
        // Expected
        // -------------------------------------------------

        QString expected =
                formatTwoWireExpected(
                    data["exp"].toString());

        m_twoWireTestModel->setItem(
                    row,
                    6,
                    new QStandardItem(expected));

        // -------------------------------------------------
        // Measured
        // Initially empty
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    7,
                    new QStandardItem(""));

        // -------------------------------------------------
        // Result
        // Initially empty
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    8,
                    new QStandardItem(""));

        // -------------------------------------------------
        // Remarks
        // Initially empty
        // -------------------------------------------------

        m_twoWireTestModel->setItem(
                    row,
                    9,
                    new QStandardItem(""));
    }

    // =====================================================
    // Table behavior
    // =====================================================

    ui->tableView_twoWireTest->setSelectionBehavior(
                QAbstractItemView::SelectRows);

    ui->tableView_twoWireTest->setSelectionMode(
                QAbstractItemView::SingleSelection);

    ui->tableView_twoWireTest->setEditTriggers(
                QAbstractItemView::NoEditTriggers);

    ui->tableView_twoWireTest->setAlternatingRowColors(
                true);

    // =====================================================
    // All columns:
    // Equal width + stretch to entire table
    // =====================================================

    ui->tableView_twoWireTest->horizontalHeader()
            ->setSectionResizeMode(
                QHeaderView::Stretch);

    // =====================================================
    // Row height
    // =====================================================

    ui->tableView_twoWireTest->verticalHeader()
            ->setDefaultSectionSize(28);
}

void MainWindow::portStatus(const QString &data)
{
    if(data.startsWith("Serial object is not initialized/port not selected"))
    {
        QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 115200"))
    {
        QMessageBox::information(this,"Success",data);
    }

    if(data.startsWith("Failed to open port"))
    {

        QMessageBox::critical(this,"Error",data);
    }

}

void MainWindow::processTwoWireResults()
{
    if (!m_twoWireTestModel)
        return;

    // Get results from TestController
    const QVector<float> &results =
            test->getTwoWireResults();

    int rowCount =
            m_twoWireTestModel->rowCount();

    int resultCount =
            qMin(results.size(), rowCount);

    for (int row = 0; row < resultCount; ++row)
    {
        float measuredOhms =
                results[row];

        // =============================================
        // Convert measured value to readable format
        // =============================================

        QString measuredText;

        if (qAbs(measuredOhms) >= 1000000000.0)
        {
            measuredText =
                    QString::number(
                        measuredOhms / 1000000000.0,
                        'f',
                        3)
                    + "GΩ";
        }
        else if (qAbs(measuredOhms) >= 1000000.0)
        {
            measuredText =
                    QString::number(
                        measuredOhms / 1000000.0,
                        'f',
                        3)
                    + "MΩ";
        }
        else if (qAbs(measuredOhms) >= 1000.0)
        {
            measuredText =
                    QString::number(
                        measuredOhms / 1000.0,
                        'f',
                        3)
                    + "KΩ";
        }
        else
        {
            measuredText =
                    QString::number(
                        measuredOhms,
                        'f',
                        3)
                    + "Ω";
        }

        // =============================================
        // Update Measured column
        // Column 7 = Measured
        // =============================================

        m_twoWireTestModel->setData(
                    m_twoWireTestModel->index(row, 7),
                    measuredText);

        // =============================================
        // Get Expected
        // Column 6 = Expected
        // =============================================

        QString expected =
                m_twoWireTestModel
                ->index(row, 6)
                .data()
                .toString()
                .trimmed();

        // =============================================
        // Compare
        // =============================================

        bool pass =
                checkTwoWireExpected(
                    measuredOhms,
                    expected);

        // =============================================
        // Result
        // Column 8 = Result
        // =============================================

        QString result =
                pass ? "PASS" : "FAIL";

        QModelIndex resultIndex =
                m_twoWireTestModel->index(row, 8);

        m_twoWireTestModel->setData(
                    resultIndex,
                    result);

        // =============================================
        // PASS / FAIL background
        // =============================================

        if (pass)
        {
            m_twoWireTestModel->setData(
                        resultIndex,
                        QBrush(Qt::green),
                        Qt::BackgroundRole);
        }
        else
        {
            m_twoWireTestModel->setData(
                        resultIndex,
                        QBrush(Qt::red),
                        Qt::BackgroundRole);
        }

        // =============================================
        // Remarks
        // Column 9 = Remarks
        // =============================================

        QString remarks;

        if (pass)
        {
            remarks = "Within expected range";
        }
        else
        {
            remarks = "Measured value out of range";
        }

        m_twoWireTestModel->setData(
                    m_twoWireTestModel->index(row, 9),
                    remarks);
    }

    writeToNotes(
        QString("Two Wire comparison completed. "
                "Results: %1")
        .arg(resultCount));
}

bool MainWindow::checkTwoWireExpected(
        float measuredOhms,
        const QString &expected)
{
    QString value =
            expected.trimmed();

    // =================================================
    // < / > format
    //
    // <2Ω
    // >10Ω
    // <2KΩ
    // >10MΩ
    // >2GΩ
    // =================================================

    QRegularExpression comparisonRegex(
        R"(^([<>])\s*([0-9]+(?:\.[0-9]+)?)\s*([KMG]?)Ω$)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match =
            comparisonRegex.match(value);

    if (match.hasMatch())
    {
        QString operation =
                match.captured(1);

        double limit =
                match.captured(2).toDouble();

        QString unit =
                match.captured(3).toUpper();

        // ---------------------------------------------
        // Convert expected value to Ohms
        // ---------------------------------------------

        if (unit == "K")
            limit *= 1000.0;

        else if (unit == "M")
            limit *= 1000000.0;

        else if (unit == "G")
            limit *= 1000000000.0;

        // ---------------------------------------------
        // Compare
        // ---------------------------------------------

        if (operation == "<")
            return measuredOhms < limit;

        if (operation == ">")
            return measuredOhms > limit;
    }

    // =================================================
    // Range format
    //
    // 20KΩ ± 3Ω
    // 20KΩ ± 3KΩ
    // 100Ω ± 5Ω
    // 9MΩ ± 2MΩ
    // 2GΩ ± 500MΩ
    // =================================================

    QRegularExpression rangeRegex(
        R"(^([0-9]+(?:\.[0-9]+)?)([KMG]?)Ω\s*±\s*([0-9]+(?:\.[0-9]+)?)([KMG]?)Ω$)",
        QRegularExpression::CaseInsensitiveOption);

    match = rangeRegex.match(value);

    if (match.hasMatch())
    {
        double nominal =
                match.captured(1).toDouble();

        QString nominalUnit =
                match.captured(2).toUpper();

        double tolerance =
                match.captured(3).toDouble();

        QString toleranceUnit =
                match.captured(4).toUpper();

        // ---------------------------------------------
        // Convert nominal to Ohms
        // ---------------------------------------------

        if (nominalUnit == "K")
            nominal *= 1000.0;

        else if (nominalUnit == "M")
            nominal *= 1000000.0;

        else if (nominalUnit == "G")
            nominal *= 1000000000.0;

        // ---------------------------------------------
        // Convert tolerance to Ohms
        // ---------------------------------------------

        if (toleranceUnit == "K")
            tolerance *= 1000.0;

        else if (toleranceUnit == "M")
            tolerance *= 1000000.0;

        else if (toleranceUnit == "G")
            tolerance *= 1000000000.0;

        // ---------------------------------------------
        // Calculate range
        // ---------------------------------------------

        double lowerLimit =
                nominal - tolerance;

        double upperLimit =
                nominal + tolerance;

        return measuredOhms >= lowerLimit &&
               measuredOhms <= upperLimit;
    }

    // =================================================
    // Invalid expected format
    // =================================================

    return false;
}

QString MainWindow::formatTwoWireExpected(
        const QString &expected)
{
    QString value =
            expected.trimmed();

    // -------------------------------------------------
    // Range format:
    // 20K ± 3
    // 20K ± 3K
    // 9M ± 2M
    // 2G ± 500M
    // -------------------------------------------------

    QRegularExpression rangeRegex(
        R"(^([0-9]+(?:\.[0-9]+)?[KMG]?)\s*±\s*([0-9]+(?:\.[0-9]+)?[KMG]?)$)",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match =
            rangeRegex.match(value);

    if (match.hasMatch())
    {
        QString nominal =
                match.captured(1);

        QString tolerance =
                match.captured(2);

        return nominal + "Ω ± "
                + tolerance + "Ω";
    }

    // -------------------------------------------------
    // Comparison format:
    // <2
    // >5M
    // <10K
    // >2G
    // -------------------------------------------------

    QRegularExpression comparisonRegex(
        R"(^([<>])\s*([0-9]+(?:\.[0-9]+)?[KMG]?)$)",
        QRegularExpression::CaseInsensitiveOption);

    match =
            comparisonRegex.match(value);

    if (match.hasMatch())
    {
        QString operation =
                match.captured(1);

        QString number =
                match.captured(2);

        return operation
                + number
                + "Ω";
    }

    // -------------------------------------------------
    // Already formatted / unknown format
    // -------------------------------------------------

    return value;
}

void MainWindow::onSelfTestSlotResult(
        int slotNumber,
        const QByteArray &slotResult)
{
    qDebug()
        << "MainWindow Self Test Slot:"
        << slotNumber
        << slotResult.toHex(' ').toUpper();

    writeToNotes(
        QString("Self Test Slot %1 Result : %2")
        .arg(slotNumber)
        .arg(QString::fromLatin1(
                 slotResult.toHex(' ').toUpper())));

    // =================================================
    // FIND RELAY BOARD
    // =================================================

    int boardNumber = -1;

    if (slotNumber == 1 || slotNumber == 2)
        boardNumber = 1;
    else if (slotNumber == 3 || slotNumber == 4)
        boardNumber = 2;
    else if (slotNumber == 5 || slotNumber == 6)
        boardNumber = 3;
    else if (slotNumber == 7 || slotNumber == 8)
        boardNumber = 4;
    else if (slotNumber == 9 || slotNumber == 10)
        boardNumber = 5;
    else if (slotNumber == 11 || slotNumber == 12)
        boardNumber = 6;
    else if (slotNumber == 13 || slotNumber == 14)
        boardNumber = 7;
    else if (slotNumber == 15 || slotNumber == 16)
        boardNumber = 8;
    else if (slotNumber == 17 || slotNumber == 18)
        boardNumber = 9;
    else if (slotNumber == 19 || slotNumber == 20)
        boardNumber = 10;
    else if (slotNumber == 21 || slotNumber == 22)
        boardNumber = 11;
    else if (slotNumber == 23 || slotNumber == 24)
        boardNumber = 12;
    else if (slotNumber == 25 || slotNumber == 26)
        boardNumber = 13;
    else if (slotNumber == 27 || slotNumber == 28)
        boardNumber = 14;
    else if (slotNumber == 29 || slotNumber == 31)
        boardNumber = 15;

    if (boardNumber < 1 ||
        boardNumber > 15)
    {
        return;
    }

    // =================================================
    // GET CORRESPONDING TEXT EDIT
    // =================================================

    QTextEdit *textEdit = nullptr;

    switch (boardNumber)
    {
    case 1:
        textEdit = ui->textEdit_1;
        break;

    case 2:
        textEdit = ui->textEdit_2;
        break;

    case 3:
        textEdit = ui->textEdit_3;
        break;

    case 4:
        textEdit = ui->textEdit_4;
        break;

    case 5:
        textEdit = ui->textEdit_5;
        break;

    case 6:
        textEdit = ui->textEdit_6;
        break;

    case 7:
        textEdit = ui->textEdit_7;
        break;

    case 8:
        textEdit = ui->textEdit_8;
        break;

    case 9:
        textEdit = ui->textEdit_9;
        break;

    case 10:
        textEdit = ui->textEdit_10;
        break;

    case 11:
        textEdit = ui->textEdit_11;
        break;

    case 12:
        textEdit = ui->textEdit_12;
        break;

    case 13:
        textEdit = ui->textEdit_13;
        break;

    case 14:
        textEdit = ui->textEdit_14;
        break;

    case 15:
        textEdit = ui->textEdit_15;
        break;
    }

    if (!textEdit)
        return;

    // =================================================
    // CURRENT SLOT IS BEING PROCESSED
    // =================================================

    textEdit->setStyleSheet(
        "background-color: #FFF59D;");

    // =================================================
    // REMOVE FF ABCDE
    // =================================================

    QByteArray data = slotResult;

    const QByteArray endMarker =
            QByteArray::fromHex(
                "FF4142434445");

    int endIndex =
            data.indexOf(endMarker);

    if (endIndex >= 0)
    {
        data.remove(
            endIndex,
            endMarker.size());
    }

    // =================================================
    // CHECK SLOT MISS
    // =================================================

    const QByteArray missingMarker =
            QByteArray::fromHex(
                "4B53410158");

    bool slotMissing =
            (data == missingMarker);

    bool currentSlotHasFailedPins = false;

    if (slotMissing)
    {
        m_selfTestBoardMissing[
            boardNumber - 1] = true;

        writeToNotes(
            QString("Relay Board %1 : SLOT %2 MISS")
            .arg(boardNumber)
            .arg(slotNumber));
    }
    else
    {
        // =================================================
        // PARSE FAILED PINS
        // =================================================
        //
        // Example:
        //
        // 02 20
        //
        // Connector = 02
        // Pin       = 20
        //
        // For second 64-pin command:
        //
        // Pin 1  -> 65
        // Pin 2  -> 66
        // ...
        // Pin 64 -> 128
        //
        // =================================================

        bool secondHalf =
                (slotNumber % 2 == 0) ||
                (boardNumber == 15 &&
                 slotNumber == 31);

        int failedPinsBefore =
                m_selfTestFailedPins[
                    boardNumber - 1].size();


        for (int i = 0;
             i + 1 < data.size();
             i += 2)
        {
            quint8 connector =
                    static_cast<quint8>(
                        data.at(i));

            quint8 pin =
                    static_cast<quint8>(
                        data.at(i + 1));

            Q_UNUSED(connector);

            int physicalPin =
                    static_cast<int>(pin);

            if (secondHalf)
            {
                physicalPin += 64;
            }

            m_selfTestFailedPins[
                boardNumber - 1].append(
                    physicalPin);
        }

        currentSlotHasFailedPins =
                m_selfTestFailedPins[
                    boardNumber - 1].size()
                > failedPinsBefore;

    }

    // =================================================
    // DETERMINE WHETHER THIS IS SECOND SLOT
    // =================================================

    bool secondSlot =
            (slotNumber % 2 == 0) ||
            (boardNumber == 15 &&
             slotNumber == 31);

    // =================================================
    // FIRST 64-PIN SLOT
    // =================================================

    if (!secondSlot)
    {
        if (slotMissing)
        {
            textEdit->setStyleSheet(
                "background-color: #FFCDD2;");

            textEdit->append(
                QString("SLOT %1 : SLOT MISS")
                .arg(slotNumber));

            writeToNotes(
                QString("SLOT %1 : SLOT MISS")
                .arg(slotNumber));
        }
        else if (currentSlotHasFailedPins)
        {
            textEdit->append(
                QString("SLOT %1 FAIL")
                .arg(slotNumber));

            writeToNotes(
                QString("SLOT %1 FAIL")
                .arg(slotNumber));
        }
        else
        {
            textEdit->append(
                QString("SLOT %1 Pass")
                .arg(slotNumber));

            writeToNotes(
                QString("SLOT %1 Pass")
                .arg(slotNumber));
        }

        return;
    }

    // =================================================
    // BOTH 64-PIN SLOTS COMPLETED
    // =================================================

    const QVector<int> &failedPins =
            m_selfTestFailedPins[
                boardNumber - 1];

    // =================================================
    // SECOND SLOT RESULT
    // =================================================

    if (slotMissing)
    {
        textEdit->append(
            QString("SLOT %1 : SLOT MISS")
            .arg(slotNumber));
    }
    else if (currentSlotHasFailedPins)
    {
        textEdit->append(
            QString("SLOT %1 FAIL")
            .arg(slotNumber));
    }
    else
    {
        textEdit->append(
            QString("SLOT %1 Pass")
            .arg(slotNumber));
    }

    bool boardFailed =
            m_selfTestBoardMissing[
                boardNumber - 1] ||
            !failedPins.isEmpty();

    // =================================================
    // BOARD PASS
    // =================================================

    if (!boardFailed)
    {
        textEdit->setStyleSheet(
            "background-color: #A5D6A7;");

        textEdit->append("PASS");

        writeToNotes(
            QString("Relay Board %1 : PASS")
            .arg(boardNumber));
    }

    // =================================================
    // BOARD FAIL
    // =================================================

    else
    {
        textEdit->setStyleSheet(
            "background-color: #FFCDD2;");

        QStringList pinStrings;

        for (int pin : failedPins)
        {
            pinStrings.append(
                QString::number(pin));
        }

        QString resultText = "FAIL";

        if (!pinStrings.isEmpty())
        {
            resultText +=
                QString("\nFailed Pins: %1")
                .arg(pinStrings.join(", "));
        }

        if (m_selfTestBoardMissing[
                boardNumber - 1])
        {
            resultText += "\nSLOT MISS";
        }

        textEdit->append(resultText);

        writeToNotes(
            QString("Relay Board %1 : FAIL")
            .arg(boardNumber));

        if (!pinStrings.isEmpty())
        {
            writeToNotes(
                QString("Failed Pins: %1")
                .arg(pinStrings.join(", ")));
        }

        if (m_selfTestBoardMissing[
                boardNumber - 1])
        {
            writeToNotes(
                "SLOT MISS detected");
        }
    }
}

void MainWindow::onSelfTestResultsCompleted(
        const QByteArray &allResults)
{
    qDebug()
        << "MainWindow Self Test ALL RESULTS:"
        << allResults.toHex(' ').toUpper();

    writeToNotes(
        "Self Test All Results Received");

    QMessageBox::information(
        this,
        "Self Test",
                "Self Test Completed.");
}

void MainWindow::onSelfTestSlotStarted(int slotNumber)
{
    int boardNumber = -1;

    if (slotNumber == 1 || slotNumber == 2)
        boardNumber = 1;
    else if (slotNumber == 3 || slotNumber == 4)
        boardNumber = 2;
    else if (slotNumber == 5 || slotNumber == 6)
        boardNumber = 3;
    else if (slotNumber == 7 || slotNumber == 8)
        boardNumber = 4;
    else if (slotNumber == 9 || slotNumber == 10)
        boardNumber = 5;
    else if (slotNumber == 11 || slotNumber == 12)
        boardNumber = 6;
    else if (slotNumber == 13 || slotNumber == 14)
        boardNumber = 7;
    else if (slotNumber == 15 || slotNumber == 16)
        boardNumber = 8;
    else if (slotNumber == 17 || slotNumber == 18)
        boardNumber = 9;
    else if (slotNumber == 19 || slotNumber == 20)
        boardNumber = 10;
    else if (slotNumber == 21 || slotNumber == 22)
        boardNumber = 11;
    else if (slotNumber == 23 || slotNumber == 24)
        boardNumber = 12;
    else if (slotNumber == 25 || slotNumber == 26)
        boardNumber = 13;
    else if (slotNumber == 27 || slotNumber == 28)
        boardNumber = 14;
    else if (slotNumber == 29 || slotNumber == 31)
        boardNumber = 15;

    if (boardNumber < 1 || boardNumber > 15)
        return;

    QTextEdit *textEdit = nullptr;

    switch (boardNumber)
    {
    case 1:  textEdit = ui->textEdit_1;  break;
    case 2:  textEdit = ui->textEdit_2;  break;
    case 3:  textEdit = ui->textEdit_3;  break;
    case 4:  textEdit = ui->textEdit_4;  break;
    case 5:  textEdit = ui->textEdit_5;  break;
    case 6:  textEdit = ui->textEdit_6;  break;
    case 7:  textEdit = ui->textEdit_7;  break;
    case 8:  textEdit = ui->textEdit_8;  break;
    case 9:  textEdit = ui->textEdit_9;  break;
    case 10: textEdit = ui->textEdit_10; break;
    case 11: textEdit = ui->textEdit_11; break;
    case 12: textEdit = ui->textEdit_12; break;
    case 13: textEdit = ui->textEdit_13; break;
    case 14: textEdit = ui->textEdit_14; break;
    case 15: textEdit = ui->textEdit_15; break;
    }

    if (!textEdit)
        return;

    textEdit->setStyleSheet(
        "background-color: #FFF59D;");

    if (textEdit->toPlainText().isEmpty())
    {
        textEdit->append(
            QString("Relay Board %1")
            .arg(boardNumber));
    }

    textEdit->append(
        QString("SLOT %1 Testing...")
                .arg(slotNumber));
}

void MainWindow::onCalibrationSlotReceived(
    int slotNumber,
    const QByteArray &slotData)
{
    qDebug() << "Calibration Slot"
             << slotNumber
             << "received:"
             << slotData.size()
             << "bytes";

    if (slotData.size() != 256)
    {
        qDebug() << "Calibration ERROR:"
                 << "Invalid slot data size.";

        writeToNotes(
            QString("Calibration Slot %1 : Invalid data")
                .arg(slotNumber));

        return;
    }

    /*
     * Each slot contains 64 float values.
     * 4 bytes per float.
     *
     * Slot sequence:
     * 1  -> rows   0 - 63
     * 2  -> rows  64 - 127
     * ...
     * 29 -> rows 1792 - 1855
     * 31 -> rows 1856 - 1919
     */

    int slotIndex =
        test->calibrationSlotIndex();

    int startRow =
        slotIndex * 64;

    for (int i = 0; i < 64; ++i)
    {
        int byteOffset = i * 4;
        int row = startRow + i;

        float value =
            calibrationBytesToFloat(
                slotData,
                byteOffset);

        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem(
                QString::number(
                    value,
                    'f',
                    6)));

        qDebug() << "Calibration"
                 << "Slot:" << slotNumber
                 << "Index:" << i
                 << "Row:" << row
                 << "Value:" << value;
    }

    // Scroll to the latest received values
      ui->tableView_Calibration
          ->scrollTo(
              m_calibrationModel->index(
                  startRow + 63,
                  2),
              QAbstractItemView::PositionAtBottom);

      writeToNotes(
          QString("Calibration Slot %1 received - 64 values")
              .arg(slotNumber));
}

void MainWindow::onCalibrationSlotMissing(
    int slotNumber)
{
    qDebug() << "Calibration Slot"
             << slotNumber
             << "SLOT MISS";

    int slotIndex =
        test->calibrationSlotIndex();

    int startRow =
        slotIndex * 64;

    for (int i = 0; i < 64; ++i)
    {
        int row = startRow + i;

        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem("SLOT MISS"));
    }

    // Scroll to the latest received values
      ui->tableView_Calibration
          ->scrollTo(
              m_calibrationModel->index(
                  startRow + 63,
                  2),
              QAbstractItemView::PositionAtBottom);

    writeToNotes(
        QString("Calibration Slot %1 : SLOT MISS")
            .arg(slotNumber));
}

void MainWindow::onCalibrationCompleted()
{
    qDebug() << "Calibration Data Received";

    writeToNotes(
        "Calibration Data Received - "
        "All 30 Slots Completed");

    // Return table to first row
     ui->tableView_Calibration
         ->scrollTo(
             m_calibrationModel->index(0, 0),
             QAbstractItemView::PositionAtTop);


    QMessageBox::information(
        this,
        "Calibration Test",
                "Calibration data received successfully.");
}

float MainWindow::calibrationBytesToFloat(
    const QByteArray &data,
    int offset)
{
    quint32 bits =
        (static_cast<quint8>(data[offset + 3]) << 24) |
        (static_cast<quint8>(data[offset + 2]) << 16) |
        (static_cast<quint8>(data[offset + 1]) << 8)  |
         static_cast<quint8>(data[offset]);

    float value;

    memcpy(&value, &bits, sizeof(value));

    return value;
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

        // -----------------------------------------
        // Populate result table FIRST
        // -----------------------------------------
        populateTwoWireTestTable(harness);

        // -----------------------------------------
        // Get expected result count
        // -----------------------------------------
        expectedResults =
                m_twoWireTestModel->rowCount();

        // -----------------------------------------
        // Give count to TestController
        // -----------------------------------------
        test->setTwoWireExpectedResults(
            expectedResults);

        // -----------------------------------------
        // NOW start transmission
        // -----------------------------------------
        test->startTwoWireTransmission(
            startPacket,
            packets);

        // Reset progress bar
        ui->progressBar_twoWire->setValue(0);
        ui->progressBar_twoWire->setMaximum(
            m_twoWireTestModel->rowCount());

        // Move to two wire test page
        ui->stackedWidget->setCurrentWidget(
            ui->page_twoWireTest);

        ui->label_test->setText(
            "Two Wire Continuity Test");
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


void MainWindow::on_pushButton_backFromTwoWireTest_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_test);
}

void MainWindow::on_pushButton_selfTest_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_selfTest);
}


void MainWindow::on_pushButton_selfBack_clicked()
{
    for (int i = 1; i <= 15; ++i)
    {
        QTextEdit *textEdit =
                findChild<QTextEdit *>(
                    QString("textEdit_%1").arg(i));

        if (textEdit)
        {
            textEdit->clear();
            textEdit->setStyleSheet("");
        }
    }

    ui->stackedWidget->setCurrentWidget(
        ui->page_testPage);
}

void MainWindow::on_pushButton_selfRun_clicked()
{
    if (!test)
        return;

    if (!test->isConnected())
    {
        QMessageBox::warning(
                    this,
                    "Self Test",
                    "Serial port is not connected.");
        return;
    }

    // Reset all Self Test Relay Board displays
    for (int i = 1; i <= 15; ++i)
    {
        QTextEdit *textEdit =
                findChild<QTextEdit *>(
                    QString("textEdit_%1").arg(i));

        if (textEdit)
        {
            textEdit->clear();
            textEdit->setStyleSheet("");
        }
    }

    writeToNotes(
        "==============================");

    writeToNotes(
        "Self Test Started");

    writeToNotes(
        "==============================");


    for (int i = 0; i < 15; ++i)
    {
        m_selfTestFailedPins[i].clear();
        m_selfTestBoardMissing[i] = false;
    }

    test->startSelfTest();
}

void MainWindow::on_pushButton_selfAbort_clicked()
{
    test->abortCommand();
}

void MainWindow::on_pushButton_stopTwoWireTest_clicked()
{
    test->abortCommand();
}

void MainWindow::on_pushButton_calibrationTest_clicked()
{
    // -------------------------------------------------
    // Create Calibration table model
    // -------------------------------------------------

    m_calibrationModel =
            new QStandardItemModel(
                1920,
                3,
                this);

    // -------------------------------------------------
    // Headers
    // -------------------------------------------------

    m_calibrationModel->setHorizontalHeaderLabels(
        {
            "Connector",
            "Pin",
            "Calibration Value (Ω)"
        });

    // -------------------------------------------------
    // Fill 15 Connectors × 128 Pins
    // -------------------------------------------------

    int row = 0;

    for (int connector = 1;
         connector <= 15;
         ++connector)
    {
        for (int pin = 1;
             pin <= 128;
             ++pin)
        {
            m_calibrationModel->setItem(
                row,
                0,
                new QStandardItem(
                    QString("%1")
                    .arg(connector)));

            m_calibrationModel->setItem(
                row,
                1,
                new QStandardItem(
                    QString::number(pin)));

            m_calibrationModel->setItem(
                row,
                2,
                new QStandardItem(""));

            ++row;
        }
    }

    // -------------------------------------------------
    // Show model in table
    // -------------------------------------------------

    ui->tableView_Calibration
        ->setModel(m_calibrationModel);

    // -------------------------------------------------
    // Table settings
    // -------------------------------------------------

    ui->tableView_Calibration
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

    ui->tableView_Calibration
        ->setSelectionBehavior(
            QAbstractItemView::SelectRows);

    ui->tableView_Calibration
        ->setSelectionMode(
            QAbstractItemView::SingleSelection);

    // -------------------------------------------------
    // Table settings
    // -------------------------------------------------

    ui->tableView_Calibration
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

    ui->tableView_Calibration
        ->setSelectionBehavior(
            QAbstractItemView::SelectRows);

    ui->tableView_Calibration
        ->setSelectionMode(
            QAbstractItemView::SingleSelection);

    // -------------------------------------------------
    // Header styling
    // -------------------------------------------------

    QFont headerFont =
            ui->tableView_Calibration
            ->horizontalHeader()
            ->font();

    headerFont.setPointSize(13);
    headerFont.setBold(true);

    ui->tableView_Calibration
        ->horizontalHeader()
        ->setFont(headerFont);

    // -------------------------------------------------
    // Table font size
    // -------------------------------------------------

    QFont tableFont =
            ui->tableView_Calibration
            ->font();

    tableFont.setPointSize(13);

    ui->tableView_Calibration
            ->setFont(tableFont);


    // -------------------------------------------------
    // Equal column width
    // -------------------------------------------------

    ui->tableView_Calibration
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch);

    // -------------------------------------------------
    // Row height
    // -------------------------------------------------

    ui->tableView_Calibration
        ->verticalHeader()
        ->setDefaultSectionSize(24);

    ui->stackedWidget->setCurrentWidget(ui->page_calibrationTest);
}

void MainWindow::on_pushButton_calibrationRun_clicked()
{
    // Clear Calibration Value column
    for (int row = 0;
         row < m_calibrationModel->rowCount();
         ++row)
    {
        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem(""));
    }

    // Start calibration
    test->startCalibration();
}

void MainWindow::on_pushButton_calibrationBack_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_testPage);
}

void MainWindow::on_pushButton_calibrationAbort_clicked()
{
    test->abortCommand();
}

void MainWindow::on_pushButton_calibrationSave_clicked()
{
    // ---------------------------------------------------------
    // Check database
    // ---------------------------------------------------------

    if (!db.isOpen())
    {
        QString error =
                "Calibration Save Failed:\n"
                "Database is not open.";

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration Save Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Create calibration table if it doesn't exist
    // ---------------------------------------------------------

    QSqlQuery query(db);

    QString createTable =
            "CREATE TABLE IF NOT EXISTS calibrationData ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "connector INTEGER NOT NULL, "
            "pin INTEGER NOT NULL, "
            "calibrationValue TEXT NOT NULL)";

    if (!query.exec(createTable))
    {
        QString error =
                "Failed to create calibrationData table:\n"
                + query.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration Save Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Start transaction
    // ---------------------------------------------------------

    if (!db.transaction())
    {
        QString error =
                "Failed to start database transaction:\n"
                + db.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration Save Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Delete previous calibration data
    // ---------------------------------------------------------

    if (!query.exec("DELETE FROM calibrationData"))
    {
        QString error =
                "Failed to clear previous calibration data:\n"
                + query.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        db.rollback();

        QMessageBox::critical(
            this,
            "Calibration Save Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Prepare INSERT query
    // ---------------------------------------------------------

    QSqlQuery insertQuery(db);

    insertQuery.prepare(
        "INSERT INTO calibrationData "
        "(connector, pin, calibrationValue) "
        "VALUES (:connector, :pin, :calibrationValue)");

    // ---------------------------------------------------------
    // Insert all calibration rows
    // ---------------------------------------------------------

    for (int row = 0;
         row < m_calibrationModel->rowCount();
         ++row)
    {
        QString connectorText =
                m_calibrationModel
                ->item(row, 0)
                ->text();

        QString pinText =
                m_calibrationModel
                ->item(row, 1)
                ->text();

        QString valueText =
                m_calibrationModel
                ->item(row, 2)
                ->text();

        insertQuery.bindValue(
            ":connector",
            connectorText.toInt());

        insertQuery.bindValue(
            ":pin",
            pinText.toInt());

        // Can contain either:
        // numeric value OR "SLOT MISS"
        insertQuery.bindValue(
            ":calibrationValue",
            valueText);

        if (!insertQuery.exec())
        {
            QString error =
                    QString(
                        "Failed to save calibration data.\n"
                        "Row: %1\n"
                        "Connector: %2\n"
                        "Pin: %3\n"
                        "Value: %4\n\n"
                        "Database Error:\n%5")
                    .arg(row + 1)
                    .arg(connectorText)
                    .arg(pinText)
                    .arg(valueText)
                    .arg(insertQuery.lastError().text());

            qWarning() << error;

            writeToNotes(error);

            db.rollback();

            QMessageBox::critical(
                this,
                "Calibration Save Error",
                error);

            return;
        }
    }

    // ---------------------------------------------------------
    // Commit transaction
    // ---------------------------------------------------------

    if (!db.commit())
    {
        QString error =
                "Failed to commit calibration data:\n"
                + db.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration Save Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Success
    // ---------------------------------------------------------

    qDebug() << "Calibration data saved successfully."
             << "Rows:" << m_calibrationModel->rowCount();

    writeToNotes(
        QString(
            "Calibration data saved successfully. "
            "Total rows: %1")
        .arg(m_calibrationModel->rowCount()));

    QMessageBox::information(
        this,
        "Calibration Save",
        QString(
            "Calibration data saved successfully.\n\n"
            "Total rows saved: %1")
        .arg(m_calibrationModel->rowCount()));
}

void MainWindow::on_pushButton_calibrationView_clicked()
{
    // ---------------------------------------------------------
    // Check database
    // ---------------------------------------------------------

    if (!db.isOpen())
    {
        QString error =
                "Calibration View Failed:\n"
                "Database is not open.";

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration View Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Check calibration table
    // ---------------------------------------------------------

    QSqlQuery query(db);

    QString checkTable =
            "SELECT name FROM sqlite_master "
            "WHERE type='table' "
            "AND name='calibrationData'";

    if (!query.exec(checkTable))
    {
        QString error =
                "Failed to check calibration table:\n"
                + query.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration View Error",
            error);

        return;
    }

    if (!query.next())
    {
        QString error =
                "Calibration data table does not exist.";

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::warning(
            this,
            "Calibration Data",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Fetch calibration data
    // ---------------------------------------------------------

    if (!query.exec(
            "SELECT connector, pin, calibrationValue "
            "FROM calibrationData "
            "ORDER BY connector, pin"))
    {
        QString error =
                "Failed to fetch calibration data:\n"
                + query.lastError().text();

        qWarning() << error;

        writeToNotes(error);

        QMessageBox::critical(
            this,
            "Calibration View Error",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Create model
    // ---------------------------------------------------------

    m_calibrationModel =
            new QStandardItemModel(
                this);

    m_calibrationModel
        ->setHorizontalHeaderLabels(
            {
                "Connector",
                "Pin",
                "Calibration Value (Ω)"
            });

    // ---------------------------------------------------------
    // Fill model
    // ---------------------------------------------------------

    int row = 0;

    while (query.next())
    {
        QString connector =
                query.value("connector").toString();

        QString pin =
                query.value("pin").toString();

        QString calibrationValue =
                query.value("calibrationValue").toString();

        m_calibrationModel->setItem(
            row,
            0,
            new QStandardItem(connector));

        m_calibrationModel->setItem(
            row,
            1,
            new QStandardItem(pin));

        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem(calibrationValue));

        ++row;
    }

    // ---------------------------------------------------------
    // Check if data exists
    // ---------------------------------------------------------

    if (row == 0)
    {
        QString error =
                "No calibration data found.";

        qDebug() << error;

        writeToNotes(error);

        QMessageBox::information(
            this,
            "Calibration Data",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Configure table
    // ---------------------------------------------------------

    ui->tableView_Calibration
        ->setModel(m_calibrationModel);

    ui->tableView_Calibration
        ->setEditTriggers(
            QAbstractItemView::NoEditTriggers);

    ui->tableView_Calibration
        ->setSelectionBehavior(
            QAbstractItemView::SelectRows);

    ui->tableView_Calibration
        ->setSelectionMode(
            QAbstractItemView::SingleSelection);

    ui->tableView_Calibration
        ->horizontalHeader()
        ->setSectionResizeMode(
            QHeaderView::Stretch);

    ui->tableView_Calibration
        ->verticalHeader()
        ->setDefaultSectionSize(24);

    // ---------------------------------------------------------
    // Show first row
    // ---------------------------------------------------------

    ui->tableView_Calibration
        ->scrollTo(
            m_calibrationModel->index(0, 0),
            QAbstractItemView::PositionAtTop);

    ui->stackedWidget
        ->setCurrentWidget(
            ui->page_calibrationTest);

    // ---------------------------------------------------------
    // Log
    // ---------------------------------------------------------

    qDebug() << "Calibration data loaded:"
             << row << "rows";

    writeToNotes(
        QString(
            "Calibration data loaded successfully. "
            "Total rows: %1")
        .arg(row));

    QMessageBox::information(
        this,
        "Calibration Data",
        QString(
            "Calibration data loaded successfully.\n\n"
            "Total rows loaded: %1")
        .arg(row));}

void MainWindow::on_pushButton_setColumnCalibrate_clicked()
{
    // ---------------------------------------------------------
    // Get value from line edit
    // ---------------------------------------------------------

    QString valueText =
            ui->lineEdit_calibratedValue
            ->text()
            .trimmed();

    // ---------------------------------------------------------
    // Check empty value
    // ---------------------------------------------------------

    if (valueText.isEmpty())
    {
        QString error =
                "Please enter a calibration value.";

        writeToNotes(error);

        QMessageBox::warning(
            this,
            "Calibration Value",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Validate integer / float
    // ---------------------------------------------------------

    bool ok = false;

    double value =
            valueText.toDouble(&ok);

    if (!ok)
    {
        QString error =
                "Invalid calibration value.\n\n"
                "Please enter a valid integer or float value.\n\n"
                "Example: 12, 12.5, 0.25";

        qWarning() << error
                   << "Entered:" << valueText;

        writeToNotes(
            QString("Invalid calibration value entered: %1")
            .arg(valueText));

        QMessageBox::warning(
            this,
            "Invalid Calibration Value",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Optional: reject negative values
    // ---------------------------------------------------------

    if (value < 0)
    {
        QString error =
                "Calibration value cannot be negative.";

        writeToNotes(error);

        QMessageBox::warning(
            this,
            "Invalid Calibration Value",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Check calibration table
    // ---------------------------------------------------------

    if (!m_calibrationModel ||
        m_calibrationModel->rowCount() == 0)
    {
        QString error =
                "Calibration table is empty.";

        writeToNotes(error);

        QMessageBox::warning(
            this,
            "Calibration Value",
            error);

        return;
    }

    // ---------------------------------------------------------
    // Set entire third column
    // ---------------------------------------------------------

    for (int row = 0;
         row < m_calibrationModel->rowCount();
         ++row)
    {
        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem(valueText));
    }

    // ---------------------------------------------------------
    // Log
    // ---------------------------------------------------------

    qDebug() << "Calibration column updated:"
             << valueText;

    writeToNotes(
        QString(
            "Calibration column updated with value: %1 "
            "(%2 rows)")
        .arg(valueText)
        .arg(m_calibrationModel->rowCount()));

    QMessageBox::information(
        this,
        "Calibration Value",
        QString(
            "Calibration value updated successfully.\n\n"
            "Value: %1\n"
            "Rows updated: %2")
        .arg(valueText)
        .arg(m_calibrationModel->rowCount()));

    //continue from here ...
}

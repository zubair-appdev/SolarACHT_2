#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "testcontroller.h"
#include <QToolButton>

#include <QPainter>
#include <QAbstractTextDocumentLayout>

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;



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
     connect(ui->pushButton_refresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);

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
    ui->tableView_patch->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView_patch->setSelectionMode(QAbstractItemView::SingleSelection);

    //harness model
    harnessModel = new QSqlTableModel(this,db);
    ui->tableView_harness->setModel(harnessModel);
    ui->tableView_harness->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    harnessModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    ui->tableView_harness->verticalHeader()->setVisible(true);

    // Relay model
    relayModel = new QSqlTableModel(this, db);
    ui->tableView_Relay->setModel(relayModel);
    ui->tableView_Relay->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    ui->tableView_Relay->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView_Relay->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView_Relay->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView_Relay->verticalHeader()->setVisible(true);

    // Diode model
    diodeModel = new QSqlTableModel(this, db);
    ui->tableView_diode->setModel(diodeModel);
    ui->tableView_diode->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    ui->tableView_diode->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView_diode->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView_diode->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView_diode->verticalHeader()->setVisible(true);

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

    // Insulation/Isolation test signals
    connect(test,
            &TestController::insulationLoomPassed,
            this,
            &MainWindow::onInsulationLoomPassed);

    connect(test,
            &TestController::insulationLoomFailed,
            this,
            &MainWindow::onInsulationLoomFailed);

    connect(test,
            &TestController::insulationResultsCompleted,
            this,
            &MainWindow::onInsulationResultsCompleted);

    connect(test,
            &TestController::insulationNetMarkerReceived,
            this,
            &MainWindow::onInsulationNetMarkerReceived);

    //Diode Test Slots
    // =====================================================
    // DIODE RESULT -> UI TABLE
    // =====================================================

    connect(test,
            &TestController::diodeResultReceived,
            this,
            [this](int row, const QString &result)
    {
        if (!m_diodeTestModel)
            return;

        if (row < 0 || row >= m_diodeTestModel->rowCount())
            return;

        // Result column = 7
        QStandardItem *item =
            m_diodeTestModel->item(row, 7);

        if (item)
        {
            item->setText(result);

            // PASS = light green; all other results = light red
            if (result == "PASS")
            {
                item->setBackground(QColor(200, 255, 200));
            }
            else
            {
                item->setBackground(QColor(255, 200, 200));
            }
        }

        // Update progress
        ui->progressBar_DiodeTest->setValue(row + 1);

        // Auto-scroll to the latest result
        QModelIndex resultIndex =
            m_diodeTestModel->index(row, 7);

        ui->tableView_diodeTest->scrollTo(
            resultIndex,
            QAbstractItemView::PositionAtBottom);
    });

    // =====================================================
    // DIODE TEST COMPLETED
    // =====================================================

    connect(test,
            &TestController::diodeResultsCompleted,
            this,
            [this]()
    {
        ui->progressBar_DiodeTest->setValue(
            ui->progressBar_DiodeTest->maximum());

        ui->label_test_Diode->setText(
            "Diode Test Completed");

        writeToNotes("Diode Test Completed");

        QMessageBox::information(
            this,
            "Diode Test",
            "Diode test completed successfully.");
    });

    //Relay Test Slots
    connect(test,
            &TestController::relayResultReceived,
            this,
            &MainWindow::onRelayResultReceived);

    connect(test,
            &TestController::relayResultsCompleted,
            this,
            &MainWindow::onRelayResultsCompleted);

}
MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::refreshPorts()
{
    qDebug() << "Refreshing ports...";

    ui->comboBox_ports->clear();

    QStringList ports =
            test->availablePorts();

    ui->comboBox_ports->addItems(ports);

//    QString detectedPort =
//            serialObj->detectDevicePort();

//    if (!detectedPort.isEmpty())
//    {
//        ui->comboBox_ports ->setCurrentText(detectedPort);

//        serialObj->setPORTNAME(detectedPort);

//        qDebug() << "Auto connected to"
//                 << detectedPort;
//    }
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

      // Check whether the table exists (case-insensitive)
      QStringList tables = db.tables();

      bool tableExists = false;

      for (const QString &table : tables)
      {
          if (table.compare(tableName, Qt::CaseInsensitive) == 0)
          {
              tableExists = true;
              break;
          }
      }

      if (tableExists)
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
    QStringList tables = db.tables();

    bool tableExists = false;

    for (const QString &table : tables)
    {
        if (table.compare(tableName, Qt::CaseInsensitive) == 0)
        {
            tableExists = true;
            break;
        }
    }

    if (tableExists)
    {
        QMessageBox::warning(
            this,
            "Table Exists",
            "Cable name already exists.");

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

    // Diode temporary vectors
    QVector<QString> diodeCableTemp;
    QVector<QString> diodeAnodeConTemp, diodeAnodePinTemp;
    QVector<QString> diodeCathodeConTemp, diodeCathodePinTemp;
    QVector<QString> diodeExpVoltageTemp;

    // Relay temporary vectors
    QVector<QString> relayCableTemp;

    QVector<QString> relaySourceConTemp;
    QVector<QString> relaySourcePinTemp;

    QVector<QString> relaySideConTemp;
    QVector<QString> relaySideRSPinsTemp;

    QVector<QString> relayDestConTemp;
    QVector<QString> relayDestPinTemp;

    QVector<QString> relayExcitationVoltageTemp;
    QVector<QString> relayExpTemp;

    // ⭐ IMPORTANT LOG ENTRY
    writeToNotes("Processing CSV → Cable: " + cableNameFromUI +
                 ", Type: " + type +
                 ", File: " + fileName);

    int lineNumber = 0;

    enum class HarnessSection
    {
        Harness,
        Relay,
        Diode
    };

    HarnessSection currentSection = HarnessSection::Harness;
    bool skipSectionHeader = false;

    bool relayDataFound = false;
    bool diodeDataFound = false;


    while (!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();
        lineNumber++;

        if (line.isEmpty())
            continue;

        QStringList parts = line.split(",");

        // Remove only trailing empty columns.
        // Preserve empty fields between populated columns.
        while (!parts.isEmpty() && parts.last().trimmed().isEmpty())
        {
            parts.removeLast();
        }

        // Skip rows containing no actual values.
        if (std::all_of(parts.begin(), parts.end(),
                        [](const QString &s)
                        {
                            return s.trimmed().isEmpty();
                        }))
        {
            continue;
        }

        // -------------------------------------------------
        // Relay and Diode section detection (HARNESS upload only)
        // -------------------------------------------------
        if (type == "harness")
        {
            // A section marker must occupy the first three
            // consecutive columns of the same row.
            auto isSectionMarker = [&parts](const QString &marker)
            {
                return parts.size() >= 3 &&
                       parts[0].trimmed() == marker &&
                       parts[1].trimmed() == marker &&
                       parts[2].trimmed() == marker;
            };

            if (isSectionMarker("#12345"))
            {
                currentSection = HarnessSection::Relay;
                skipSectionHeader = true;

                writeToNotes(
                    "Relay section detected at line " +
                    QString::number(lineNumber));

                continue;
            }

            if (isSectionMarker("#54321"))
            {
                currentSection = HarnessSection::Diode;
                skipSectionHeader = true;

                writeToNotes(
                    "Diode section detected at line " +
                    QString::number(lineNumber));

                continue;
            }


            // -------------------------------------------------
            // Relay section: 9 columns
            // -------------------------------------------------
            if (currentSection == HarnessSection::Relay)
            {
                if (parts.size() != 9)
                {
                    QString error =
                        "Relay row must contain 9 columns at line " +
                        QString::number(lineNumber) +
                        ". Found: " + QString::number(parts.size());

                    writeToNotes("❌ " + error);
                    ui->textEdit_ErrorLog->append("❌ " + error);

                    file.close();
                    return false;
                }

                if (skipSectionHeader)
                {
                    skipSectionHeader = false;

                    writeToNotes(
                        "Relay header validated at line " +
                        QString::number(lineNumber));

                    continue;
                }

                // Store Relay data in temporary vectors
                relayCableTemp.append(parts[0].trimmed());

                relaySourceConTemp.append(parts[1].trimmed());
                relaySourcePinTemp.append(parts[2].trimmed());

                relaySideConTemp.append(parts[3].trimmed());
                relaySideRSPinsTemp.append(parts[4].trimmed());

                relayDestConTemp.append(parts[5].trimmed());
                relayDestPinTemp.append(parts[6].trimmed());

                relayExcitationVoltageTemp.append(parts[7].trimmed());
                relayExpTemp.append(parts[8].trimmed());

                relayDataFound = true;

                continue;
            }


            // -------------------------------------------------
            // Diode section: 6 columns
            // -------------------------------------------------
            if (currentSection == HarnessSection::Diode)
            {
                if (parts.size() != 6)
                {
                    QString error =
                        "Diode row must contain 6 columns at line " +
                        QString::number(lineNumber) +
                        ". Found: " + QString::number(parts.size());

                    writeToNotes("❌ " + error);
                    ui->textEdit_ErrorLog->append("❌ " + error);

                    file.close();
                    return false;
                }

                if (skipSectionHeader)
                {
                    skipSectionHeader = false;

                    writeToNotes(
                        "Diode header validated at line " +
                        QString::number(lineNumber));

                    continue;
                }

                // Store Diode data in separate vectors
                diodeCableTemp.append(parts[0].trimmed());
                diodeAnodeConTemp.append(parts[1].trimmed());
                diodeAnodePinTemp.append(parts[2].trimmed());
                diodeCathodeConTemp.append(parts[3].trimmed());
                diodeCathodePinTemp.append(parts[4].trimmed());
                diodeExpVoltageTemp.append(parts[5].trimmed());

                diodeDataFound = true;

                continue;
            }
        }

        // -------------------------------------------------
        // EXISTING PATCH PROCESSING — unchanged
        // -------------------------------------------------
        if (type == "patch")
        {
            if (parts.size() != 5)
            {
                writeToNotes(
                    "Malformed PATCH line " +
                    QString::number(lineNumber));

                ui->textEdit_ErrorLog->append(
                    "Malformed PATCH line " +
                    QString::number(lineNumber));

                file.close();
                return false;
            }

            QString pCon = parts[3].trimmed();
            QString pPinStr = parts[4].trimmed();

            if (!validatePatchLine(lineNumber, pCon, pPinStr))
            {
                file.close();
                return false;
            }

            int tpNo = pCon.section('-', 1).toInt();
            int pPin = pPinStr.toInt();

            if (pPin <= 64)
            {
                tpNo = (tpNo * 2) - 1;
            }
            else
            {
                if (tpNo == 15)
                    tpNo = 31;
                else
                    tpNo = tpNo * 2;

                pPin -= 64;
            }

            pCon = QString("TP-%1").arg(tpNo);
            pPinStr = QString::number(pPin);

            writeToNotes(
                "PATCH Line OK (" + QString::number(lineNumber) + "): " +
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
        else if (type == "harness" &&
                 currentSection == HarnessSection::Harness)
        {
            if (parts.size() != 6)
            {
                QString error =
                    "Malformed HARNESS line " +
                    QString::number(lineNumber) +
                    ". Expected 6 columns, found " +
                    QString::number(parts.size());

                writeToNotes("❌ " + error);
                ui->textEdit_ErrorLog->append("❌ " + error);

                file.close();
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
    //While loop end


    file.close();

    if (type == "harness")
    {
        if (!relayDataFound)
        {
            const QString message =
                "Relay data not detected in the uploaded file.";

            writeToNotes(message);
            ui->textEdit_ErrorLog->append(message);
        }

        if (!diodeDataFound)
        {
            const QString message =
                "Diode data not detected in the uploaded file.";

            writeToNotes(message);
            ui->textEdit_ErrorLog->append(message);
        }
    }

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
    // -------------------------------------------------
    // Run final Diode validations and save to database
    // -------------------------------------------------
    if (type == "harness" && diodeDataFound)
    {
        if (!validateDiodeData(
                diodeCableTemp,
                diodeAnodeConTemp, diodeAnodePinTemp,
                diodeCathodeConTemp, diodeCathodePinTemp,
                diodeExpVoltageTemp))
        {
            return false;
        }

        if (!saveDiodeToDb(
                cableNameFromUI,
                diodeCableTemp,
                diodeAnodeConTemp, diodeAnodePinTemp,
                diodeCathodeConTemp, diodeCathodePinTemp,
                diodeExpVoltageTemp))
        {
            writeToNotes("❌ Failed to save DIODE data to DB");
            ui->textEdit_ErrorLog->append(
                "❌ Failed to save DIODE data to DB");
            return false;
        }

        writeToNotes(
            "💾 Saved DIODE data to DB for cable: " +
            cableNameFromUI);
    }

    // -------------------------------------------------
    // Run final Relay validations and save to database
    // -------------------------------------------------
    if (type == "harness" && relayDataFound)
    {
        if (!validateRelayData(
                    relayCableTemp,
                    relaySourceConTemp,
                    relaySourcePinTemp,
                    relaySideConTemp,
                    relaySideRSPinsTemp,
                    relayDestConTemp,
                    relayDestPinTemp,
                    relayExcitationVoltageTemp,
                    relayExpTemp))
        {
            return false;
        }

        if (!saveRelayToDb(
                    cableNameFromUI,
                    relayCableTemp,
                    relaySourceConTemp,
                    relaySourcePinTemp,
                    relaySideConTemp,
                    relaySideRSPinsTemp,
                    relayDestConTemp,
                    relayDestPinTemp,
                    relayExcitationVoltageTemp,
                    relayExpTemp))
        {
            QString error = "Failed to save RELAY data to DB";

            writeToNotes("❌ " + error);
            ui->textEdit_ErrorLog->append("❌ " + error);
            return false;
        }

        writeToNotes(
                    "💾 Saved RELAY data to DB for cable: " +
                    cableNameFromUI);
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

bool MainWindow::validateHarnessData(
        const QVector<QString> &cableTemp,
        const QVector<QString> &sourceConTemp,
        const QVector<QString> &sourcePinTemp,
        const QVector<QString> &destConTemp,
        const QVector<QString> &destPinTemp,
        const QVector<QString> &expTemp)
{
    QString extraLog;

    // =====================================================
    // Validate Source / Destination against Patch data
    // =====================================================

    QStringList missingRows;

    for (int i = 0; i < sourceConTemp.size(); i++)
    {
        int matchSrc = 0;
        int matchDst = 0;

        for (int j = 0; j < userConList.size(); j++)
        {
            if (sourceConTemp[i] == userConList[j] &&
                sourcePinTemp[i] == userPinList[j])
            {
                matchSrc++;
            }

            if (destConTemp[i] == userConList[j] &&
                destPinTemp[i] == userPinList[j])
            {
                matchDst++;
            }
        }

        // -------------------------------------------------
        // Check missing Source / Destination
        // -------------------------------------------------

        if (matchSrc == 0 || matchDst == 0)
        {
            QString problem;

            if (matchSrc == 0 && matchDst == 0)
            {
                problem =
                    "Source and Destination not found in Patch data";
            }
            else if (matchSrc == 0)
            {
                problem =
                    "Source not found in Patch data";
            }
            else
            {
                problem =
                    "Destination not found in Patch data";
            }

            QString log =
                QString(
                    "Row %1 | Cable: %2 | "
                    "Source: %3 - %4 | "
                    "Destination: %5 - %6 | "
                    "%7")
                .arg(i + 1)
                .arg(cableTemp[i])
                .arg(sourceConTemp[i])
                .arg(sourcePinTemp[i])
                .arg(destConTemp[i])
                .arg(destPinTemp[i])
                .arg(problem);

            missingRows.append(log);
        }

        // -------------------------------------------------
        // Extra source / destination checks
        // -------------------------------------------------

        //        if (matchSrc > 1)
        //            extraLog +=
        //                QString(
        //                    "⚠ EXTRA source append at index %1: %2-%3\n")
        //                .arg(i)
        //                .arg(sourceConTemp[i])
        //                .arg(sourcePinTemp[i]);

        //        if (matchDst > 1)
        //            extraLog +=
        //                QString(
        //                    "⚠ EXTRA destination append at index %1: %2-%3\n")
        //                .arg(i)
        //                .arg(destConTemp[i])
        //                .arg(destPinTemp[i]);
    }

    // =====================================================
    // Extra validation errors
    // =====================================================

    if (!extraLog.isEmpty())
    {
        ui->textEdit_ErrorLog->append(extraLog);
        writeToNotes(extraLog);

        return false;
    }

    // =====================================================
    // Missing Source / Destination
    // =====================================================

    if (!missingRows.isEmpty())
    {
        QString missingLog =
            "❌ HARNESS VALIDATION FAILED\n\n" +
            missingRows.join("\n");

        writeToNotes(missingLog);
        ui->textEdit_ErrorLog->append(missingLog);

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
    // Range in ohms: 400-600, 100-200
    // =====================================================

    QRegularExpression expRegex(
        R"(^(?:[<>][0-9]+[KM]?|[0-9]+[KM]?\s*±\s*[0-9]+[KM]?|[0-9]+-[0-9]+)$)",
        QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < expTemp.size(); i++)
    {
        QString v = expTemp[i].trimmed();

        if (!expRegex.match(v).hasMatch())
        {
            writeToNotes(
                QString(
                    "Invalid expValue at index %1: %2")
                .arg(i)
                .arg(v));

            ui->textEdit_ErrorLog->append(
                "Invalid expValue at index " +
                QString::number(i) +
                ": " +
                v);

            return false;
        }
    }

    // =====================================================
    // PRINT ALL HARNESS DATA AFTER SUCCESSFUL VALIDATION
    // =====================================================

    writeToNotes(
        "######## HARNESS DATA (VALIDATED) ########");

    for (int i = 0;
         i < sourceConTemp.size();
         i++)
    {
        QString log =
            QString(
                "HARNESS Line OK (%1): "
                "Cable: %2 , "
                "SRC: %3-%4 → "
                "DEST: %5-%6 , "
                "exp: %7")
            .arg(i + 1)
            .arg(cableTemp[i])
            .arg(sourceConTemp[i])
            .arg(sourcePinTemp[i])
            .arg(destConTemp[i])
            .arg(destPinTemp[i])
            .arg(expTemp[i]);

        writeToNotes(log);
    }

    writeToNotes(
        "###########################################");

    writeToNotes(
        "✅ Harness data validated successfully.");

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
    list.append("Files");  // First fixed item

    QStringList tables = db.tables();

    QSet<QString> patchNames;
    QSet<QString> harnessNames;
    QSet<QString> detailsNames;

    for (const QString &t : tables)
    {
        if (t.endsWith("_patch"))
        {
            patchNames.insert(t.left(t.length() - 6));
        }
        else if (t.endsWith("_harness"))
        {
            harnessNames.insert(t.left(t.length() - 8));
        }
        else if (t.endsWith("_details"))
        {
            detailsNames.insert(t.left(t.length() - 8));
        }
    }

    // Include only names that have all three tables
    for (const QString &name : patchNames)
    {
        if (harnessNames.contains(name) &&
            detailsNames.contains(name))
        {
            list.append(name);
        }
    }

    // Sort cable names, keeping "Files" first
    std::sort(list.begin() + 1, list.end());

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
    QString cableName = ui->lineEdit_cableName->text().trimmed();

    if (!cableName.isEmpty())
    {
        cableName.replace(" ", "_");

        QString patchTable = cableName + "_patch";
        QString harnessTable = cableName + "_harness";
        QString detailsTable = cableName + "_details";

        QStringList missingTables;

        if (!db.tables().contains(patchTable))
            missingTables.append("Patch data");

        if (!db.tables().contains(harnessTable))
            missingTables.append("Harness data");

        if (!db.tables().contains(detailsTable))
            missingTables.append("Add MetaData");

        if (missingTables.isEmpty())
        {
            ui->stackedWidget->setCurrentIndex(4);
        }
        else
        {
            QMessageBox::information(
                this,
                "Incomplete Data",
                "Please fill in the following missing data for '"
                    + cableName + "':\n\n"
                    + missingTables.join("\n")
            );
            QMessageBox::StandardButton reply;
            reply=QMessageBox::question(this,"Leave page","Do you want to leave without filling?");
            if(reply==QMessageBox::Yes)
            {
                  deleteCable(cableName);
                  ui->stackedWidget->setCurrentIndex(4);
            }
        }
    }
    else
    {
         ui->stackedWidget->setCurrentIndex(4);
    }

}

void MainWindow::on_pushButton_getDetails_clicked()
{
    QString tableName = ui->comboBox_fileNames->currentText();

    ui->pushButton_patchCancel->hide();

    if (tableName.isEmpty())
    {
        QMessageBox::information(
            this, "Missing file Name",
            "Select file from dropdown");
        return;
    }

    if (tableName == "Files")
    {
        QMessageBox::information(
            this, "Invalid",
            "Select valid fileName");
        return;
    }

    ui->tabWidget->setCurrentIndex(0);
    ui->stackedWidget->setCurrentIndex(11);

    // Patch
    loadPatchTable(tableName + "_patch");
    ui->label_fileName->setText(tableName);

    // Harness
    loadHarnessTable(tableName + "_harness");
    ui->label_fileNameH->setText(tableName);

    // Relay (optional)
    loadRelayTable(tableName + "_relay");

    // Diode (optional)
    loadDiodeTable(tableName + "_diode");
}

void MainWindow::on_tabWidget_tabBarClicked(int index)
{
    //Used as back button
    if(index == 4)
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
                int tpNo = value.section('-', 1).toInt();

                // Special case:
                // TP-31 is the second half of logical TP-15
                if(tpNo == 31)
                {
                    tpNo = 15;
                }
                else if(tpNo % 2)
                {
                    tpNo = (tpNo + 1) / 2;
                }
                else
                {
                    tpNo /= 2;
                }

                value = QString("TP-%1").arg(tpNo);
            }
            else if(col == 5)  // Patch Pin
            {
                int pin = value.toInt();

                QString dbCon =
                        patchModel->index(row, 4)
                        .data()
                        .toString();

                int tpNo =
                        dbCon.section('-', 1).toInt();

                // Even physical TP = second half
                // TP-31 is also the second half of TP-15
                if(tpNo % 2 == 0 || tpNo == 31)
                {
                    pin += 64;
                }

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
        bool del = deleteCable(selectedFile);

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

    QString patchTable   = cableName + "_patch";
    QString harnessTable = cableName + "_harness";
    QString metaDetaTable= cableName + "_details";
    QString relayTable   = cableName + "_relay";
    QString diodeTable   = cableName + "_diode";

    // Clear loaded tables from models
    if (patchModel)
    {
        patchModel->clear();
        patchModel->setTable("");
    }

    if (harnessModel)
    {
        harnessModel->clear();
        harnessModel->setTable("");
    }

    if (relayModel)
    {
        relayModel->clear();
        relayModel->setTable("");
    }

    if (diodeModel)
    {
        diodeModel->clear();
        diodeModel->setTable("");
    }

    QApplication::processEvents();

    QSqlQuery q(db);

    // Drop Patch table
    if (!q.exec("DROP TABLE IF EXISTS \"" + patchTable + "\""))
    {
        writeToNotes("❌ Failed to drop Patch table: " +
                     q.lastError().text());
        return false;
    }

    // Drop Harness table
    if (!q.exec("DROP TABLE IF EXISTS \"" + harnessTable + "\""))
    {
        writeToNotes("❌ Failed to drop Harness table: " +
                     q.lastError().text());
        return false;
    }
    if (!q.exec("DROP TABLE IF EXISTS \"" + metaDetaTable + "\""))
    {
        writeToNotes("❌ Failed to drop metaDataTable table: " +
                     q.lastError().text());
        return false;
    }


    // Drop Relay table
    if (!q.exec("DROP TABLE IF EXISTS \"" + relayTable + "\""))
    {
        writeToNotes("❌ Failed to drop Relay table: " +
                     q.lastError().text());
        return false;
    }

    // Drop Diode table
    if (!q.exec("DROP TABLE IF EXISTS \"" + diodeTable + "\""))
    {
        writeToNotes("❌ Failed to drop Diode table: " +
                     q.lastError().text());
        return false;
    }

    writeToNotes("✅ Cable and associated Patch, Harness, Relay, "
                 "and Diode tables deleted.");

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
        // Get calibration values
        // =============================================

        int sourceConnector =
                test->get_k_SourceCon()[row]
                .section('-', 1)
                .toInt();

        int sourcePin =
                test->get_k_SourcePin()[row]
                .toInt();

        int destinationConnector =
                test->get_k_DestinationCon()[row]
                .section('-', 1)
                .toInt();

        int destinationPin =
                test->get_k_DestinationPin()[row]
                .toInt();

        double sourceCalibration = 0.0;
        double destinationCalibration = 0.0;

        QSqlQuery calibrationQuery(db);

        // Source
        calibrationQuery.prepare(
            "SELECT calibrationValue "
            "FROM calibrationData "
            "WHERE connector = :connector "
            "AND pin = :pin");

        calibrationQuery.bindValue(
            ":connector",
            sourceConnector);

        calibrationQuery.bindValue(
            ":pin",
            sourcePin);

        if (calibrationQuery.exec() &&
            calibrationQuery.next())
        {
            QString value =
                    calibrationQuery.value(
                        "calibrationValue")
                    .toString()
                    .trimmed();

            if (value != "SLOT MISS")
            {
                sourceCalibration =
                        value.toDouble();
            }
        }

        // Destination
        calibrationQuery.finish();

        calibrationQuery.bindValue(
            ":connector",
            destinationConnector);

        calibrationQuery.bindValue(
            ":pin",
            destinationPin);

        if (calibrationQuery.exec() &&
            calibrationQuery.next())
        {
            QString value =
                    calibrationQuery.value(
                        "calibrationValue")
                    .toString()
                    .trimmed();

            if (value != "SLOT MISS")
            {
                destinationCalibration =
                        value.toDouble();
            }
        }

        // =============================================
        // Calibration Formula
        // =============================================

        float convertedMeasuredOhms =
                measuredOhms
                - (sourceCalibration +
                   destinationCalibration);

        // =============================================
        // Log calibration calculation
        // =============================================

        writeToNotes(
            QString(
                "Two Wire Result %1 | "
                "HW Measured: %2 Ω | "
                "SRC Calibration: %3 Ω | "
                "DST Calibration: %4 Ω | "
                "Converted: %5 Ω")
            .arg(row + 1)
            .arg(measuredOhms, 0, 'f', 6)
            .arg(sourceCalibration, 0, 'f', 6)
            .arg(destinationCalibration, 0, 'f', 6)
            .arg(convertedMeasuredOhms, 0, 'f', 6));

        // =============================================
        // Convert calibrated measured value to readable format
        // =============================================

        QString measuredText;

        if (qAbs(convertedMeasuredOhms) >= 1000000000.0)
        {
            measuredText =
                    QString::number(
                        convertedMeasuredOhms / 1000000000.0,
                        'f',
                        3)
                    + "GΩ";
        }
        else if (qAbs(convertedMeasuredOhms) >= 1000000.0)
        {
            measuredText =
                    QString::number(
                        convertedMeasuredOhms / 1000000.0,
                        'f',
                        3)
                    + "MΩ";
        }
        else if (qAbs(convertedMeasuredOhms) >= 1000.0)
        {
            measuredText =
                    QString::number(
                        convertedMeasuredOhms / 1000.0,
                        'f',
                        3)
                    + "KΩ";
        }
        else
        {
            measuredText =
                    QString::number(
                        convertedMeasuredOhms,
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

    ui->tableView_twoWireTest->scrollTo(
                m_twoWireTestModel->index(0,0),
                QAbstractItemView::PositionAtTop);

    writeToNotes(
        QString("Two Wire comparison completed. "
                "Results: %1")
        .arg(resultCount));

    QMessageBox::information(
                this,
                "Two Wire Test",
                "Two Wire Test Completed Successfully.");
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


     QMessageBox::StandardButton reply =
             QMessageBox::question(
                 this,
                 "Calibration Test",
                 "Calibration Test Completed Successfully.\n\n"
                 "Do you want to save the calibration data in the database?",
                 QMessageBox::Yes |
                 QMessageBox::No);

     if (reply == QMessageBox::Yes)
     {
         on_pushButton_calibrationSave_clicked();
     }
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

void MainWindow::onInsulationLoomPassed(int loomNo)
{
    qDebug() << "UI: Insulation Loom"
             << loomNo
             << "PASS";

    QStandardItemModel *model =
        qobject_cast<QStandardItemModel *>(
            ui->tableView_InsuIso->model());

    if(!model)
        return;

    for(int row = 0;
        row < model->rowCount();
        ++row)
    {
        int tableLoomNo =
                model->item(row, 1)
                ->text()
                .toInt();

        if(tableLoomNo == loomNo)
        {
            // Result column
            model->item(row, 7)
                ->setText("Pass");

            // Fail Net With Resistance
            model->item(row, 5)
                ->setText("-");

            // Light green Cell
            model->item(row, 7)
                ->setBackground(
                    QColor(200, 255, 200));

        }
    }

    ui->tableView_InsuIso->resizeRowsToContents();

    ui->tableView_InsuIso->resizeRowsToContents();

    // Position the last row of this loom in the center
    for(int row = model->rowCount() - 1; row >= 0; --row)
    {
        int tableLoomNo =
                model->item(row, 1)->text().toInt();

        if(tableLoomNo == loomNo)
        {
            ui->tableView_InsuIso->scrollTo(
                model->index(row, 7),
                QAbstractItemView::PositionAtCenter);

            break;
        }
    }

    //For Progress Bar
    if(totalInsulationLooms > 0)
    {
        currentPercentage += 100.0 / totalInsulationLooms;
        currentLoomNetProgress = 0.0;

        ui->progressBar_InsuIsoTest->setValue(
            qRound(qMin(currentPercentage, 100.0)));
    }
}

void MainWindow::onInsulationLoomFailed(
        int loomNo,
        const QByteArray &loomData)
{
    qDebug() << "UI: Insulation Loom"
             << loomNo
             << "FAIL";

    qDebug() << "Failed Loom Data:"
             << loomData.toHex(' ').toUpper();

    QStandardItemModel *model =
        qobject_cast<QStandardItemModel *>(
            ui->tableView_InsuIso->model());

    if(!model)
        return;


    // First mark all rows of this loom as PASS
    for(int row = 0; row < model->rowCount(); ++row)
    {
        int tableLoomNo = model->item(row, 1)->text().toInt();

        if(tableLoomNo == loomNo)
        {
            model->item(row, 7)->setText("Pass");
            model->item(row, 5)->setText("-");

            // Only the Result column becomes green
            model->item(row, 7)->setBackground(
                QColor(200, 255, 200));
        }
    }

    // --------------------------------------------------
    // Find all resistance records
    //
    // Format:
    // 2E 2E + 4 byte float
    // --------------------------------------------------

    const QByteArray failedMarker =
            QByteArray::fromHex("2E2E");

    QVector<float> resistances;

    int pos = 0;

    while(pos + 6 <= loomData.size())
    {
        if(loomData.mid(pos, 2) == failedMarker)
        {
            QByteArray resistanceBytes =
                    loomData.mid(pos + 2, 4);

            quint32 bits =
                    (static_cast<quint8>(resistanceBytes[3]) << 24) |
                    (static_cast<quint8>(resistanceBytes[2]) << 16) |
                    (static_cast<quint8>(resistanceBytes[1]) << 8)  |
                     static_cast<quint8>(resistanceBytes[0]);

            float resistance = 0.0f;

            memcpy(&resistance,
                   &bits,
                   sizeof(float));

            resistances.append(resistance);

            qDebug() << "Resistance:"
                     << resistance;

            pos += 6;
        }
        else
        {
            pos++;
        }
    }


    // --------------------------------------------------
    // Find locator pairs
    //
    // Example:
    // 04 00 08 00
    // -> Net 4 <-> Net 8
    // --------------------------------------------------

    QVector<QPair<int,int>> locatorPairs;

    pos = 0;

    while(pos + 4 <= loomData.size())
    {
        // Skip 2F pass indicators
        if(static_cast<quint8>(loomData[pos]) == 0x2F)
        {
            pos++;
            continue;
        }

        // Skip resistance record
        if(pos + 2 <= loomData.size() &&
           loomData.mid(pos, 2) == failedMarker)
        {
            pos += 6;
            continue;
        }

        quint16 net1 =
                static_cast<quint8>(loomData[pos]) |
                (static_cast<quint8>(loomData[pos + 1]) << 8);

        quint16 net2 =
                static_cast<quint8>(loomData[pos + 2]) |
                (static_cast<quint8>(loomData[pos + 3]) << 8);

        locatorPairs.append(
            qMakePair(
                static_cast<int>(net1),
                static_cast<int>(net2)));

        qDebug() << "Locator Pair:"
                 << net1
                 << "<->"
                 << net2;

        pos += 4;
    }


    // --------------------------------------------------
    // Resistance <-> locator pair
    // --------------------------------------------------

    int count =
            qMin(resistances.size(),
                 locatorPairs.size());

    for(int i = 0; i < count; ++i)
    {
        float resistance =
                resistances[i];

        int net1 =
                locatorPairs[i].first;

        int net2 =
                locatorPairs[i].second;


        // Smaller net becomes the row
        int rowNet =
                qMin(net1, net2);

        // Larger net is displayed
        int failedNet =
                qMax(net1, net2);


        QString failText =
                QString("%1 -> %2")
                    .arg(failedNet)
                    .arg(resistance);


        qDebug()
            << "Failure:"
            << "Net"
            << rowNet
            << "->"
            << failedNet
            << "Resistance"
            << resistance;


        // --------------------------------------------------
        // Find corresponding row
        // --------------------------------------------------

        for(int row = 0;
            row < model->rowCount();
            ++row)
        {
            int tableLoomNo =
                    model->item(row, 1)
                        ->text()
                        .toInt();

            int tableNetNo =
                    model->item(row, 2)
                        ->text()
                        .toInt();

            if(tableLoomNo == loomNo &&
               tableNetNo == rowNet)
            {
                QStandardItem *failItem =
                        model->item(row, 5);

                QStandardItem *resultItem =
                        model->item(row, 7);


                // Multiple failures can belong
                // to the same source net.
                if(failItem->text() == "-")
                {
                    failItem->setText(failText);
                }
                else
                {
                    failItem->setText(
                        failItem->text()
                        + "\n"
                        + failText);
                }

                // Result
                resultItem->setText("Fail");


                // -----------------------------------------
                // Light red for failed row cell
                // -----------------------------------------

                model->item(row, 7)
                    ->setBackground(
                        QColor(255, 200, 200));

                break;
            }
        }
    }


    ui->tableView_InsuIso->resizeRowsToContents();

    // Position the last row of this failed loom in the center
    for(int row = model->rowCount() - 1; row >= 0; --row)
    {
        int tableLoomNo =
                model->item(row, 1)->text().toInt();

        if(tableLoomNo == loomNo)
        {
            ui->tableView_InsuIso->scrollTo(
                model->index(row, 7),
                QAbstractItemView::PositionAtCenter);

            break;
        }
    }

    //For Progress Bar
    if(totalInsulationLooms > 0)
    {
        currentPercentage += 100.0 / totalInsulationLooms;
        currentLoomNetProgress = 0.0;

        ui->progressBar_InsuIsoTest->setValue(
            qRound(qMin(currentPercentage, 100.0)));
    }
}

void MainWindow::onInsulationResultsCompleted()
{
    qDebug()
            << "UI: Test Completed";

    // Move table to the top
    ui->tableView_InsuIso->scrollToTop();

    QMessageBox::information(
                this,
                ui->label_test_InsuIso->text(),
                QString("%1 Completed Successfully.").arg(ui->label_test_InsuIso->text()));
}

void MainWindow::onInsulationNetMarkerReceived(
    int loomNo, int totalNets)
{
    Q_UNUSED(loomNo);

    if(totalInsulationLooms <= 0 || totalNets <= 0)
        return;

    // Progress allocated to one loom
    double loomAllowance =
        100.0 / totalInsulationLooms;

    // Progress allocated to one net
    double progressPerNet =
        loomAllowance / totalNets;

    // Advance progress for this net
    currentLoomNetProgress += progressPerNet;

    // Never exceed this loom's allocated progress
    currentLoomNetProgress =
        qMin(currentLoomNetProgress, loomAllowance);

    double progress =
        currentPercentage + currentLoomNetProgress;

    ui->progressBar_InsuIsoTest->setValue(
        qRound(qMin(progress, 100.0)));

    qDebug() << "Current percentage:" << currentPercentage
             << "Current loom net progress:" << currentLoomNetProgress
             << "Progress bar:"
             << ui->progressBar_InsuIsoTest->value();
}

void MainWindow::onRelayResultReceived(
    int row, float measuredValue)
{
    if (!m_relayTestModel ||
        row < 0 ||
        row >= m_relayTestModel->rowCount())
    {
        return;
    }

    // =====================================================
    // 1. Store raw measured value
    // =====================================================

    if (row >= m_relayMeasuredValues.size())
        return;

    m_relayMeasuredValues[row] = measuredValue;

    // =====================================================
    // 2. Update Measured Value column
    // Column 11 = Measured Value
    // =====================================================

    m_relayTestModel->setData(
        m_relayTestModel->index(row, 11),
        QString::number(measuredValue, 'f', 3));

    // =====================================================
    // 3. Select current row
    // =====================================================

    ui->tableView_relay->selectRow(row);

    // =====================================================
    // 4. Scroll to current row
    // =====================================================

    ui->tableView_relay->scrollTo(
        m_relayTestModel->index(row, 0),
        QAbstractItemView::PositionAtBottom);

    // =====================================================
    // 5. Update progress bar
    // =====================================================

    ui->progressBar_RelayTest->setValue(row + 1);

    qDebug() << "Relay Live Result:"
             << "Row:" << row + 1
             << "Value:" << measuredValue;
}

void MainWindow::onRelayResultsCompleted()
{
    if (!m_relayTestModel)
        return;

    const int rowCount = m_relayTestModel->rowCount();

    const int resultCount =
        qMin(m_relayMeasuredValues.size(), rowCount);

    // =====================================================
    // Process all Relay results
    // =====================================================

    for (int row = 0; row < resultCount; ++row)
    {
        const float measuredOhms =
            m_relayMeasuredValues[row];

        // =================================================
        // 1. Convert measured value to readable format
        // =================================================

        QString measuredText;

        if (qAbs(measuredOhms) >= 1000000000.0)
        {
            measuredText =
                QString::number(
                    measuredOhms / 1000000000.0,
                    'f', 3) + "GΩ";
        }
        else if (qAbs(measuredOhms) >= 1000000.0)
        {
            measuredText =
                QString::number(
                    measuredOhms / 1000000.0,
                    'f', 3) + "MΩ";
        }
        else if (qAbs(measuredOhms) >= 1000.0)
        {
            measuredText =
                QString::number(
                    measuredOhms / 1000.0,
                    'f', 3) + "KΩ";
        }
        else
        {
            measuredText =
                QString::number(
                    measuredOhms,
                    'f', 3) + "Ω";
        }

        // =================================================
        // 2. Update Measured Value
        // Column 11
        // =================================================

        m_relayTestModel->setData(
            m_relayTestModel->index(row, 11),
            measuredText);

        // =================================================
        // 3. Get Expected Value
        // Column 9
        // =================================================

        const QString expected =
            m_relayTestModel->index(row, 9)
                .data().toString().trimmed();

        // =================================================
        // 4. Compare measured against expected
        // =================================================

        const bool pass =
            checkRelayExpected(measuredOhms, expected);

        // =================================================
        // 5. Update PASS / FAIL
        // Column 12
        // =================================================

        const QModelIndex resultIndex =
            m_relayTestModel->index(row, 12);

        m_relayTestModel->setData(
            resultIndex,
            pass ? "PASS" : "FAIL");

        // =================================================
        // 6. Light green / light red background
        // =================================================

        const QColor resultColor =
            pass ? QColor(200, 245, 200)
                 : QColor(255, 205, 205);

        m_relayTestModel->setData(
            resultIndex,
            QBrush(resultColor),
            Qt::BackgroundRole);

        // =================================================
        // 7. Remarks
        // Column 13
        // =================================================

        m_relayTestModel->setData(
            m_relayTestModel->index(row, 13),
            pass ? "Within expected range"
                 : "Measured value out of range");

        writeToNotes(
            QString("Relay Result %1 | Measured: %2 | "
                    "Expected: %3 | %4")
                .arg(row + 1)
                .arg(measuredText)
                .arg(expected)
                .arg(pass ? "PASS" : "FAIL"));
    }

    // =====================================================
    // 8. Return table to top
    // =====================================================

    ui->tableView_relay->clearSelection();

    ui->tableView_relay->scrollTo(
        m_relayTestModel->index(0, 0),
        QAbstractItemView::PositionAtTop);

    // =====================================================
    // 9. Completion popup
    // =====================================================

    writeToNotes(
        QString("Relay comparison completed. Results: %1")
            .arg(resultCount));

    QMessageBox::information(
        this,
        "Relay Test",
                "Relay Test Completed Successfully.");
}

bool MainWindow::checkRelayExpected(
    float measuredOhms,
    const QString &expected)
{
    const QString value = expected.trimmed();

    // Convert a resistance number with optional K/M/G
    // and optional Ω into ohms.
    auto parseOhms = [](const QString &text,
                         double &ohms) -> bool
    {
        static const QRegularExpression numberRegex(
            R"(^\s*([0-9]+(?:\.[0-9]+)?)\s*([KMG]?)\s*Ω?\s*$)",
            QRegularExpression::CaseInsensitiveOption);

        const QRegularExpressionMatch match =
            numberRegex.match(text);

        if (!match.hasMatch())
            return false;

        ohms = match.captured(1).toDouble();

        const QString unit =
            match.captured(2).toUpper();

        if (unit == "K")
            ohms *= 1000.0;
        else if (unit == "M")
            ohms *= 1000000.0;
        else if (unit == "G")
            ohms *= 1000000000.0;

        return true;
    };

    // =====================================================
    // 1. Less than / Greater than
    // Examples: <2, >10K, <2Ω, >10MΩ
    // =====================================================

    if (value.startsWith('<') ||
        value.startsWith('>'))
    {
        double limit = 0.0;

        if (!parseOhms(value.mid(1), limit))
            return false;

        if (value.startsWith('<'))
            return measuredOhms < limit;

        return measuredOhms > limit;
    }

    // =====================================================
    // 2. Tolerance format
    // Examples: 20K ± 3, 20KΩ ± 3KΩ
    // =====================================================

    if (value.contains(QChar(0x00B1)))
    {
        const QStringList parts =
            value.split(QChar(0x00B1));

        if (parts.size() != 2)
            return false;

        double nominal = 0.0;
        double tolerance = 0.0;

        if (!parseOhms(parts[0], nominal) ||
            !parseOhms(parts[1], tolerance))
        {
            return false;
        }

        return measuredOhms >= nominal - tolerance &&
               measuredOhms <= nominal + tolerance;
    }

    // =====================================================
    // 3. Min-Max range format
    // Examples: 400-600, 1K-2K, 400Ω-600Ω
    // =====================================================

    if (value.contains('-'))
    {
        const QStringList parts = value.split('-');

        if (parts.size() != 2)
            return false;

        double minimum = 0.0;
        double maximum = 0.0;

        if (!parseOhms(parts[0], minimum) ||
            !parseOhms(parts[1], maximum))
        {
            return false;
        }

        if (minimum > maximum)
            return false;

        return measuredOhms >= minimum &&
               measuredOhms <= maximum;
    }

    // =====================================================
    // 4. Invalid / unsupported expected format
    // =====================================================

    return false;
}

void MainWindow::populateInsulationTable(
    const QVector<InsulationTableNet> &nets,
    const QVariantList &patchData)
{
  model =
        new QStandardItemModel(this);

    model->setColumnCount(9);

    model->setHorizontalHeaderLabels({
        "S.No",
        "Loom No",
        "Net No",
        "Source Net",
        "Destination Net",
        "Failed Net (Ω)",
        "Exp Value",
        "Result",
        "Remarks"
    });

    int serialNo = 1;

    for(const InsulationTableNet &net : nets)
    {
        int netNo = net.netNo + 1;

        /*
         * Last net of a loom has no destination net,
         * so there is nothing to test against after it.
         */

        QString destinationNets;

        for(const InsulationTableNet &other : nets)
        {
            if(other.loomNo != net.loomNo)
                continue;

            int otherNetNo = other.netNo + 1;

            if(otherNetNo > netNo)
            {
                if(!destinationNets.isEmpty())
                    destinationNets += ", ";

                destinationNets +=
                    QString::number(otherNetNo);
            }
        }

        if(destinationNets.isEmpty())
            continue;

        QList<QStandardItem *> row;

        row.append(
            new QStandardItem(
                QString::number(serialNo)));

        row.append(
            new QStandardItem(
                QString::number(net.loomNo)));

        row.append(
            new QStandardItem(
                QString::number(netNo)));

        QString sourceNet;

        for(const auto &endpoint : net.endpoints)
        {
            if(!sourceNet.isEmpty())
                sourceNet += "\n";

            sourceNet += getActualConnectorName(
                patchData,
                endpoint.first,
                endpoint.second);
        }

        row.append(
            new QStandardItem(sourceNet));

        // Destination Net
        row.append(
            new QStandardItem(destinationNets));

        // Fail Net With Resistance
        row.append(
            new QStandardItem("-"));

        // Expected value
        QString expValue =
            ">" +
            ui->doubleSpinBox_ResistanceThreshold
                ->text();

        row.append(
            new QStandardItem(expValue));

        // Result
        row.append(
            new QStandardItem("Waiting..."));

        // Remarks
        row.append(
            new QStandardItem(""));

        model->appendRow(row);

        serialNo++;
    }

    ui->tableView_InsuIso->setModel(model);

    ui->tableView_InsuIso->setWordWrap(true);

    ui->tableView_InsuIso->setTextElideMode(
        Qt::ElideNone);

    QHeaderView *header =
        ui->tableView_InsuIso->horizontalHeader();

    // Compact columns
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // S.No
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Loom No
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Net No

    // Remaining columns stretch to fill available space
    header->setSectionResizeMode(3, QHeaderView::Stretch); // Source Net
    header->setSectionResizeMode(4, QHeaderView::Stretch); // Destination Net
    header->setSectionResizeMode(5, QHeaderView::Stretch); // Failed Net (Ω)
    header->setSectionResizeMode(6, QHeaderView::Stretch); // Exp Value
    header->setSectionResizeMode(7, QHeaderView::Stretch); // Result
    header->setSectionResizeMode(8, QHeaderView::Stretch); // Remarks

    // Wrapping
    ui->tableView_InsuIso->setWordWrap(true);
    ui->tableView_InsuIso->setTextElideMode(Qt::ElideNone);

    // No horizontal scrolling
    ui->tableView_InsuIso->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);

    ui->tableView_InsuIso->resizeRowsToContents();

    // Force the table to the top
    ui->tableView_InsuIso->scrollToTop();

    // Disable editing
    ui->tableView_InsuIso->setEditTriggers(
                QAbstractItemView::NoEditTriggers);

    // Allow only one row selection
    ui->tableView_InsuIso->setSelectionBehavior(
                QAbstractItemView::SelectRows);

    ui->tableView_InsuIso->setSelectionMode(
                QAbstractItemView::SingleSelection);

    ui->tableView_InsuIso->setAlternatingRowColors(true);
}

QString MainWindow::getActualConnectorName(
    const QVariantList &patchData,
    const QString &hardwareCon,
    int hardwarePin)
{
    for(const QVariant &v : patchData)
    {
        QVariantMap m = v.toMap();

        QString patchCon =
            m["patchCon"].toString();

        int patchPin =
            m["patchPin"].toInt();

        if(patchCon == hardwareCon &&
           patchPin == hardwarePin)
        {
            QString userCon =
                m["userCon"].toString();

            QString userPin =
                m["userPin"].toString();

            return QString("(\"%1\", \"%2\")")
                .arg(userCon)
                .arg(userPin);
        }
    }

    // If mapping is not found
    return QString("(\"%1\", \"%2\")")
        .arg(hardwareCon)
            .arg(hardwarePin);
}

bool MainWindow::validateDiodeData(
    const QVector<QString> &cableTemp,
    const QVector<QString> &anodeConTemp,
    const QVector<QString> &anodePinTemp,
    const QVector<QString> &cathodeConTemp,
    const QVector<QString> &cathodePinTemp,
    const QVector<QString> &expVoltageTemp)
{
    int count = cableTemp.size();

    if (count == 0)
    {
        writeToNotes("❌ No Diode data available to validate.");
        ui->textEdit_ErrorLog->append(
            "❌ No Diode data available to validate.");
        return false;
    }

    if (anodeConTemp.size() != count ||
        anodePinTemp.size() != count ||
        cathodeConTemp.size() != count ||
        cathodePinTemp.size() != count ||
        expVoltageTemp.size() != count)
    {
        QString error =
            "Diode validation failed: vector sizes do not match.";

        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
        return false;
    }

    // =====================================================
    // Validate Anode / Cathode against Patch data
    // =====================================================

    QStringList missingRows;

    for (int i = 0; i < count; ++i)
    {
        int matchAnode = 0;
        int matchCathode = 0;

        for (int j = 0; j < userConList.size(); ++j)
        {
            if (anodeConTemp[i].simplified() ==
                    userConList[j].simplified() &&
                anodePinTemp[i].simplified() ==
                    userPinList[j].simplified())
            {
                matchAnode++;
            }

            if (cathodeConTemp[i].simplified() ==
                    userConList[j].simplified() &&
                cathodePinTemp[i].simplified() ==
                    userPinList[j].simplified())
            {
                matchCathode++;
            }
        }

        if (matchAnode == 0 || matchCathode == 0)
        {
            QString problem;

            if (matchAnode == 0 && matchCathode == 0)
            {
                problem =
                    "Anode and Cathode not found in Patch data";
            }
            else if (matchAnode == 0)
            {
                problem = "Anode not found in Patch data";
            }
            else
            {
                problem = "Cathode not found in Patch data";
            }

            QString log =
                QString(
                    "Row %1 | Cable: %2 | "
                    "Anode: %3 - %4 | "
                    "Cathode: %5 - %6 | %7")
                .arg(i + 1)
                .arg(cableTemp[i])
                .arg(anodeConTemp[i])
                .arg(anodePinTemp[i])
                .arg(cathodeConTemp[i])
                .arg(cathodePinTemp[i])
                .arg(problem);

            missingRows.append(log);
        }
    }

    if (!missingRows.isEmpty())
    {
        QString missingLog =
            "❌ DIODE PATCH VALIDATION FAILED\n\n" +
            missingRows.join("\n");

        writeToNotes(missingLog);
        ui->textEdit_ErrorLog->append(missingLog);

        return false;
    }

    // =====================================================
    // Validate fields and expected voltage
    // =====================================================

    static const QRegularExpression voltageRegex(
        R"(^\s*\d+(?:\.\d+)?-\d+(?:\.\d+)?V\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < count; ++i)
    {
        QStringList fields = {
            cableTemp[i],
            anodeConTemp[i],
            anodePinTemp[i],
            cathodeConTemp[i],
            cathodePinTemp[i],
            expVoltageTemp[i]
        };

        for (int col = 0; col < fields.size(); ++col)
        {
            if (fields[col].trimmed().isEmpty())
            {
                QString error =
                    QString("Empty field in Diode row %1, column %2.")
                    .arg(i + 1)
                    .arg(col + 1);

                writeToNotes("❌ " + error);
                ui->textEdit_ErrorLog->append("❌ " + error);
                return false;
            }
        }

        if (!voltageRegex.match(expVoltageTemp[i]).hasMatch())
        {
            QString error =
                QString("Invalid Diode voltage in row %1: %2. "
                        "Expected format: float-floatV.")
                .arg(i + 1)
                .arg(expVoltageTemp[i]);

            writeToNotes("❌ " + error);
            ui->textEdit_ErrorLog->append("❌ " + error);
            return false;
        }
    }

    writeToNotes("✅ Diode data validation successful.");
    return true;
}

bool MainWindow::saveDiodeToDb(
    const QString &cableName,
    const QVector<QString> &cableTemp,
    const QVector<QString> &anodeConTemp,
    const QVector<QString> &anodePinTemp,
    const QVector<QString> &cathodeConTemp,
    const QVector<QString> &cathodePinTemp,
    const QVector<QString> &expVoltageTemp)
{
    QString tableName = cableName + "_diode";
    tableName.replace(" ", "_");

    QSqlQuery q(db);

    QString create =
        "CREATE TABLE IF NOT EXISTS " + tableName + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "cable TEXT,"
        "anodeCon TEXT,"
        "anodePin TEXT,"
        "cathodeCon TEXT,"
        "cathodePin TEXT,"
        "expVoltage TEXT"
        ")";

    if (!q.exec(create))
    {
        QString error =
            "Failed to create table " + tableName + ": " +
            q.lastError().text();

        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
        return false;
    }

    if (!db.transaction())
    {
        QString error =
            "Failed to start Diode transaction: " +
            db.lastError().text();

        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
        return false;
    }

    if (!q.exec("DELETE FROM " + tableName))
    {
        QString error =
            "Failed to clear table " + tableName + ": " +
            q.lastError().text();

        db.rollback();

        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
        return false;
    }

    q.prepare(
        "INSERT INTO " + tableName +
        " (cable, anodeCon, anodePin, "
        "cathodeCon, cathodePin, expVoltage) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    for (int i = 0; i < cableTemp.size(); ++i)
    {
        q.addBindValue(cableTemp[i]);
        q.addBindValue(anodeConTemp[i]);
        q.addBindValue(anodePinTemp[i]);
        q.addBindValue(cathodeConTemp[i]);
        q.addBindValue(cathodePinTemp[i]);
        q.addBindValue(expVoltageTemp[i]);

        if (!q.exec())
        {
            QString error =
                "Diode insert failed in " + tableName + ": " +
                q.lastError().text();

            db.rollback();

            writeToNotes("❌ " + error);
            ui->textEdit_ErrorLog->append("❌ " + error);
            return false;
        }
    }

    if (!db.commit())
    {
        QString error =
            "Failed to commit Diode transaction: " +
            db.lastError().text();

        db.rollback();

        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
        return false;
    }

    writeToNotes(
        "✅ Diode data stored in table: " + tableName);

    return true;
}


bool MainWindow::validateRelayData(
    const QVector<QString> &cableTemp,
    const QVector<QString> &sourceConTemp,
    const QVector<QString> &sourcePinTemp,
    const QVector<QString> &relaySideConTemp,
    const QVector<QString> &relaySideRSPinsTemp,
    const QVector<QString> &destConTemp,
    const QVector<QString> &destPinTemp,
    const QVector<QString> &excitationVoltageTemp,
    const QVector<QString> &expTemp)
{
    const int count = cableTemp.size();

    auto reportError = [this](const QString &error)
    {
        writeToNotes("❌ " + error);
        ui->textEdit_ErrorLog->append("❌ " + error);
    };

    if (count == 0)
    {
        reportError("No Relay data available to validate.");
        return false;
    }

    if (sourceConTemp.size() != count ||
        sourcePinTemp.size() != count ||
        relaySideConTemp.size() != count ||
        relaySideRSPinsTemp.size() != count ||
        destConTemp.size() != count ||
        destPinTemp.size() != count ||
        excitationVoltageTemp.size() != count ||
        expTemp.size() != count)
    {
        reportError(
            "Relay validation failed: vector sizes do not match.");
        return false;
    }

    // Expected resistance formats, same as Harness
    const QRegularExpression expRegex(
        R"(^(?:[<>][0-9]+[KM]?|[0-9]+[KM]?\s*±\s*[0-9]+[KM]?|[0-9]+-[0-9]+)$)",
        QRegularExpression::CaseInsensitiveOption);

    // Excitation voltage must be an integer from 1V to 34V
    const QRegularExpression voltageRegex(
        R"(^([0-9]+)V$)",
        QRegularExpression::CaseInsensitiveOption);

    // Relay side connector must be Relay_X
    const QRegularExpression relayRegex(
                R"(^Relay_([1-9][0-9]*)(?:\s+.*)?$)",
                QRegularExpression::CaseInsensitiveOption);

    QSet<QString> completedRelays;
    QSet<QString> coilPairsSeen;

    QString currentRelay;
    bool currentRelayHasCoil = false;

    for (int i = 0; i < count; ++i)
    {
        const QString relay =
            relaySideConTemp[i].simplified();

        const QString rsPins =
            relaySideRSPinsTemp[i].simplified();

        const QString voltage =
            excitationVoltageTemp[i].simplified();

        const QString expected =
            expTemp[i].simplified();

        // ---------------------------------------------
        // Required fields
        // ---------------------------------------------

        QStringList requiredFields = {
            cableTemp[i],
            sourceConTemp[i],
            sourcePinTemp[i],
            relay,
            rsPins,
            destConTemp[i],
            destPinTemp[i],
            expected
        };

        for (int col = 0; col < requiredFields.size(); ++col)
        {
            if (requiredFields[col].trimmed().isEmpty())
            {
                reportError(
                    QString("Empty required field in Relay row %1, column %2.")
                        .arg(i + 1)
                        .arg(col + 1));
                return false;
            }
        }

        // ---------------------------------------------
        // Validate Relay_X naming
        // ---------------------------------------------

        const auto relayMatch = relayRegex.match(relay);

        if (!relayMatch.hasMatch())
        {
            reportError(
                QString("Invalid Relay-side connector at row %1: %2. "
                        "Expected Relay_X.")
                    .arg(i + 1)
                    .arg(relay));
            return false;
        }

        // ---------------------------------------------
        // Relay groups must be contiguous.
        // Each new group must start with X1&X2.
        // ---------------------------------------------

        if (relay != currentRelay)
        {
            if (!currentRelay.isEmpty())
            {
                if (!currentRelayHasCoil)
                {
                    reportError(
                        QString("%1 has no mandatory X1&X2 pair.")
                            .arg(currentRelay));
                    return false;
                }

                completedRelays.insert(currentRelay);
            }

            if (completedRelays.contains(relay))
            {
                reportError(
                    QString("%1 appears in multiple separate groups.")
                        .arg(relay));
                return false;
            }

            currentRelay = relay;
            currentRelayHasCoil = false;

            if (rsPins != "X1&X2")
            {
                reportError(
                    QString("First row of %1 must use X1&X2. "
                            "Found %2 at row %3.")
                        .arg(relay)
                        .arg(rsPins)
                        .arg(i + 1));
                return false;
            }
        }

        // ---------------------------------------------
        // Validate supported relay-side pin pairs
        // ---------------------------------------------

        const bool isXPair = (rsPins == "X1&X2");
        const bool isYPair = (rsPins == "Y1&Y2");

        const bool isContactPair =
            rsPins.contains('&') &&
            !isXPair &&
            !isYPair;

        if (!isXPair && !isYPair && !isContactPair)
        {
            reportError(
                QString("Invalid RSPins at Relay row %1: %2")
                    .arg(i + 1)
                    .arg(rsPins));
            return false;
        }

        // ---------------------------------------------
        // X1&X2 must occur exactly once per relay
        // ---------------------------------------------

        if (isXPair)
        {
            if (currentRelayHasCoil)
            {
                reportError(
                    QString("%1 contains duplicate X1&X2 pairs.")
                        .arg(relay));
                return false;
            }

            currentRelayHasCoil = true;
            coilPairsSeen.insert(relay);
        }

        // ---------------------------------------------
        // Validate excitation voltage
        // Required for X1&X2 and Y1&Y2 only.
        // ---------------------------------------------

        if (isXPair || isYPair)
        {
            const auto voltageMatch =
                voltageRegex.match(voltage);

            if (!voltageMatch.hasMatch())
            {
                reportError(
                    QString("Relay row %1: %2 requires an excitation "
                            "voltage in the format 1V to 34V.")
                        .arg(i + 1)
                        .arg(rsPins));
                return false;
            }

            bool ok = false;
            const int voltageValue =
                voltageMatch.captured(1).toInt(&ok);

            if (!ok || voltageValue < 1 || voltageValue > 34)
            {
                reportError(
                    QString("Invalid excitation voltage at Relay row %1: %2. "
                            "Allowed range is 1V to 34V.")
                        .arg(i + 1)
                        .arg(voltage));
                return false;
            }
        }
        else if (isContactPair && !voltage.isEmpty())
        {
            reportError(
                QString("Relay row %1: excitation voltage must be empty "
                        "for contact pair %2.")
                    .arg(i + 1)
                    .arg(rsPins));
            return false;
        }

        // ---------------------------------------------
        // Validate expected resistance
        // ---------------------------------------------

        if (!expRegex.match(expected).hasMatch())
        {
            reportError(
                QString("Invalid expected resistance at Relay row %1: %2")
                    .arg(i + 1)
                    .arg(expected));
            return false;
        }

        // ---------------------------------------------
        // Source connector + pin must exist in Patch
        // ---------------------------------------------

        bool sourceFound = false;

        for (int j = 0; j < userConList.size(); ++j)
        {
            if (sourceConTemp[i].simplified() ==
                    userConList[j].simplified() &&
                sourcePinTemp[i].simplified() ==
                    userPinList[j].simplified())
            {
                sourceFound = true;
                break;
            }
        }

        if (!sourceFound)
        {
            reportError(
                QString("Relay row %1: Source connector/pin %2 / %3 "
                        "does not exist in Patch data.")
                    .arg(i + 1)
                    .arg(sourceConTemp[i])
                    .arg(sourcePinTemp[i]));
            return false;
        }

        // ---------------------------------------------
        // Destination connector + pin must exist in Patch
        // ---------------------------------------------

        bool destFound = false;

        for (int j = 0; j < userConList.size(); ++j)
        {
            if (destConTemp[i].simplified() ==
                    userConList[j].simplified() &&
                destPinTemp[i].simplified() ==
                    userPinList[j].simplified())
            {
                destFound = true;
                break;
            }
        }

        if (!destFound)
        {
            reportError(
                QString("Relay row %1: Destination connector/pin %2 / %3 "
                        "does not exist in Patch data.")
                    .arg(i + 1)
                    .arg(destConTemp[i])
                    .arg(destPinTemp[i]));
            return false;
        }
    }

    // Validate the final relay group
    if (!currentRelay.isEmpty() && !currentRelayHasCoil)
    {
        reportError(
            QString("%1 has no mandatory X1&X2 pair.")
                .arg(currentRelay));
        return false;
    }

    writeToNotes("✅ Relay data validation successful.");
    return true;
}


bool MainWindow::saveRelayToDb(
    const QString &cableName,
    const QVector<QString> &cableTemp,
    const QVector<QString> &sourceConTemp,
    const QVector<QString> &sourcePinTemp,
    const QVector<QString> &relaySideConTemp,
    const QVector<QString> &relaySideRSPinsTemp,
    const QVector<QString> &destConTemp,
    const QVector<QString> &destPinTemp,
    const QVector<QString> &excitationVoltageTemp,
    const QVector<QString> &expTemp)
{
    const int count = cableTemp.size();

    // Validate vector sizes.
    if (count == 0 ||
        sourceConTemp.size() != count ||
        sourcePinTemp.size() != count ||
        relaySideConTemp.size() != count ||
        relaySideRSPinsTemp.size() != count ||
        destConTemp.size() != count ||
        destPinTemp.size() != count ||
        excitationVoltageTemp.size() != count ||
        expTemp.size() != count)
    {
        writeToNotes("❌ Relay data vectors have inconsistent sizes.");
        return false;
    }

    if (!db.isOpen())
    {
        writeToNotes("❌ Database is not open.");
        return false;
    }

    const QString tableName = cableName + "_relay";

    static const QRegularExpression validIdentifier(
        R"(^[A-Za-z0-9_]+$)");

    if (!validIdentifier.match(tableName).hasMatch())
    {
        writeToNotes("❌ Invalid Relay table name.");
        return false;
    }

    QSqlQuery query(db);

    // Create the table if it does not exist.
    const QString createSql =
        QString(R"(
            CREATE TABLE IF NOT EXISTS "%1" (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                cable TEXT,
                sourceCon TEXT,
                sourcePin TEXT,
                relaySideCon TEXT,
                relaySideRSPins TEXT,
                destCon TEXT,
                destPin TEXT,
                excitationVoltage TEXT,
                exp TEXT
            )
        )").arg(tableName);

    if (!query.exec(createSql))
    {
        writeToNotes(
            "❌ Failed to create Relay table: " +
            query.lastError().text());
        return false;
    }

    // Start transaction.
    if (!db.transaction())
    {
        writeToNotes(
            "❌ Failed to start Relay transaction: " +
            db.lastError().text());
        return false;
    }

    // Insert Relay data without deleting existing records.
    const QString insertSql =
        QString(R"(
            INSERT INTO "%1" (
                cable,
                sourceCon,
                sourcePin,
                relaySideCon,
                relaySideRSPins,
                destCon,
                destPin,
                excitationVoltage,
                exp
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        )").arg(tableName);

    if (!query.prepare(insertSql))
    {
        writeToNotes(
            "❌ Relay prepare failed: " +
            query.lastError().text());

        db.rollback();
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        query.bindValue(0, cableTemp[i]);
        query.bindValue(1, sourceConTemp[i]);
        query.bindValue(2, sourcePinTemp[i]);
        query.bindValue(3, relaySideConTemp[i]);
        query.bindValue(4, relaySideRSPinsTemp[i]);
        query.bindValue(5, destConTemp[i]);
        query.bindValue(6, destPinTemp[i]);
        query.bindValue(7, excitationVoltageTemp[i]);
        query.bindValue(8, expTemp[i]);

        if (!query.exec())
        {
            writeToNotes(
                QString("❌ Relay insert failed at row %1: %2")
                    .arg(i + 1)
                    .arg(query.lastError().text()));

            db.rollback();
            return false;
        }
    }

    if (!db.commit())
    {
        writeToNotes(
            "❌ Relay commit failed: " +
            db.lastError().text());

        db.rollback();
        return false;
    }

    writeToNotes(
        QString("✅ Saved %1 Relay rows to table: %2")
            .arg(count)
            .arg(tableName));

    return true;
}


void MainWindow::loadRelayTable(const QString &tableName)
{
    relayModel->setTable(tableName);

    if (!relayModel->select())
    {
        writeToNotes("Relay table unavailable: " + tableName);

        relayModel->clear();

        return;
    }

    relayModel->setHeaderData(1, Qt::Horizontal, "Cable");
    relayModel->setHeaderData(2, Qt::Horizontal, "Source Con");
    relayModel->setHeaderData(3, Qt::Horizontal, "Source Pin");
    relayModel->setHeaderData(4, Qt::Horizontal, "Relay Side Con");
    relayModel->setHeaderData(5, Qt::Horizontal, "Relay Side Pins");
    relayModel->setHeaderData(6, Qt::Horizontal, "Dest Con");
    relayModel->setHeaderData(7, Qt::Horizontal, "Dest Pin");
    relayModel->setHeaderData(8, Qt::Horizontal, "Excitation Voltage");
    relayModel->setHeaderData(9, Qt::Horizontal, "Expected");

    // Hide the internal database ID.
    relayModel->setHeaderData(0, Qt::Horizontal, "ID");
    ui->tableView_Relay->hideColumn(0);
}


void MainWindow::loadDiodeTable(const QString &tableName)
{
    diodeModel->setTable(tableName);

    if (!diodeModel->select())
    {
        writeToNotes("Diode table unavailable: " + tableName);
        diodeModel->clear();
        return;
    }

    diodeModel->setHeaderData(1, Qt::Horizontal, "Cable");
    diodeModel->setHeaderData(2, Qt::Horizontal, "Anode Con");
    diodeModel->setHeaderData(3, Qt::Horizontal, "Anode Pin");
    diodeModel->setHeaderData(4, Qt::Horizontal, "Cathode Con");
    diodeModel->setHeaderData(5, Qt::Horizontal, "Cathode Pin");
    diodeModel->setHeaderData(6, Qt::Horizontal, "Expected Voltage");

    qDebug() << "Loaded table:" << diodeModel->tableName();
    qDebug() << "Columns:" << diodeModel->columnCount();
    qDebug() << "Rows:" << diodeModel->rowCount();


    ui->tableView_diode->hideColumn(0);
}

QVector<QByteArray> MainWindow::constructUARTPacketsForDiode(
    const QVector<QString> &sourceCon,
    const QVector<QString> &sourcePin,
    const QVector<QString> &destCon,
    const QVector<QString> &destPin,
    const QVector<QString> &expectedVoltage)
{
    QVector<QByteArray> packets;

    constexpr int MAX_PACKET_SIZE = 1024;
    constexpr int HEADER_SIZE = 6;
    constexpr int ENTRY_SIZE = 12;

    const int count = sourceCon.size();

    if (sourcePin.size() != count ||
        destCon.size() != count ||
        destPin.size() != count ||
        expectedVoltage.size() != count)
    {
        writeToNotes("Diode packet creation failed: vector sizes do not match.");
        return {};
    }

    const int maxEntriesPerPacket =
        (MAX_PACKET_SIZE - HEADER_SIZE) / ENTRY_SIZE;

    quint16 packetNo = 1;

    for (int start = 0; start < count; start += maxEntriesPerPacket)
    {
        QByteArray packet;

        // Header: 2F 2F
        packet.append(char(0x2F));
        packet.append(char(0x2F));

        // Packet number: MSB first
        packet.append(char((packetNo >> 8) & 0xFF));
        packet.append(char(packetNo & 0xFF));

        // Reserve payload length
        packet.append(char(0x00));
        packet.append(char(0x00));

        const int end = qMin(start + maxEntriesPerPacket, count);

        for (int i = start; i < end; ++i)
        {
            // Validate connector format
            if (!sourceCon[i].startsWith("TP-") ||
                !destCon[i].startsWith("TP-"))
            {
                writeToNotes(
                    QString("Invalid Diode connector at row %1: %2 -> %3")
                    .arg(i + 1)
                    .arg(sourceCon[i])
                    .arg(destCon[i]));

                return {};
            }

            // Parse expected voltage: 0.16-0.7V
            static const QRegularExpression voltageRegex(
                R"(^\s*(\d+(?:\.\d+)?)-(\d+(?:\.\d+)?)V\s*$)",
                QRegularExpression::CaseInsensitiveOption);

            QRegularExpressionMatch match =
                voltageRegex.match(expectedVoltage[i]);

            if (!match.hasMatch())
            {
                writeToNotes(
                    QString("Invalid Diode voltage at row %1: %2")
                    .arg(i + 1)
                    .arg(expectedVoltage[i]));

                return {};
            }

            bool minOk = false;
            bool maxOk = false;

            float vMin = match.captured(1).toFloat(&minOk);
            float vMax = match.captured(2).toFloat(&maxOk);

            if (!minOk || !maxOk || vMin > vMax)
            {
                writeToNotes(
                    QString("Invalid voltage range at Diode row %1")
                    .arg(i + 1));

                return {};
            }

            quint8 srcCon = static_cast<quint8>(
                sourceCon[i].section('-', 1, 1).toUInt());

            quint8 srcPin =
                static_cast<quint8>(sourcePin[i].toUInt());

            quint8 dstCon = static_cast<quint8>(
                destCon[i].section('-', 1, 1).toUInt());

            quint8 dstPin =
                static_cast<quint8>(destPin[i].toUInt());

            // Connector/pin bytes
            packet.append(char(srcCon));
            packet.append(char(srcPin));
            packet.append(char(dstCon));
            packet.append(char(dstPin));

            // Voltage floats: MSB first (big-endian)
            // Explicit byte ordering avoids host-endian assumptions.
            quint32 minBits;
            quint32 maxBits;

            static_assert(sizeof(float) == sizeof(quint32),
                          "Expected 32-bit float");

            memcpy(&minBits, &vMin, sizeof(float));
            memcpy(&maxBits, &vMax, sizeof(float));

            packet.append(char((minBits >> 24) & 0xFF));
            packet.append(char((minBits >> 16) & 0xFF));
            packet.append(char((minBits >> 8) & 0xFF));
            packet.append(char(minBits & 0xFF));

            packet.append(char((maxBits >> 24) & 0xFF));
            packet.append(char((maxBits >> 16) & 0xFF));
            packet.append(char((maxBits >> 8) & 0xFF));
            packet.append(char(maxBits & 0xFF));
        }

        // Payload length excludes the six-byte header
        const quint16 packetLength =
            static_cast<quint16>(packet.size() - HEADER_SIZE);

        packet[4] = char((packetLength >> 8) & 0xFF);
        packet[5] = char(packetLength & 0xFF);

        packets.append(packet);
        ++packetNo;
    }

    writeToNotes(
        QString("Diode packets constructed: %1")
        .arg(packets.size()));

    return packets;
}

QByteArray MainWindow::constructDiodeStartPacket(quint16 totalPackets)
{
    QByteArray packet;

    packet.append(char(0x53));
    packet.append(char(0x4B));
    packet.append(char(0x3B));

    // Total packet count (MSB first)
    packet.append(char((totalPackets >> 8) & 0xFF));
    packet.append(char(totalPackets & 0xFF));

    // XOR checksum
    quint8 checksum = 0;

    for (char c : packet)
        checksum ^= static_cast<quint8>(c);

    packet.append(char(checksum));

    writeToNotes(
        QString("Diode start packet: %1")
            .arg(QString(packet.toHex(' ').toUpper())));

    return packet;
}

void MainWindow::populateDiodeTestTable(
    const QVariantList &diode)
{
    // =====================================================
    // Create model if it doesn't exist
    // =====================================================

    if (!m_diodeTestModel)
    {
        m_diodeTestModel = new QStandardItemModel(this);

        ui->tableView_diodeTest->setModel(
            m_diodeTestModel);
    }

    // =====================================================
    // Clear previous data
    // =====================================================

    m_diodeTestModel->clear();

    // =====================================================
    // Set columns
    // =====================================================

    m_diodeTestModel->setColumnCount(9);

    m_diodeTestModel->setHorizontalHeaderLabels({
        "S.No",
        "Loom",
        "Source Conn (Anode)",
        "Source Pin",
        "Destination Conn (Cathode)",
        "Destination Pin",
        "Voltage Drop Across Diode (Min-Max)",
        "Result",
        "Remarks"
    });

    // =====================================================
    // Make header text bold
    // =====================================================

    QFont headerFont =
        ui->tableView_diodeTest->horizontalHeader()->font();

    headerFont.setBold(true);

    ui->tableView_diodeTest->horizontalHeader()->setFont(
        headerFont);

    // =====================================================
    // Fill Diode data
    // =====================================================

    for (int row = 0; row < diode.size(); ++row)
    {
        QVariantMap data = diode[row].toMap();

        // S.No
        m_diodeTestModel->setItem(
            row, 0,
            new QStandardItem(QString::number(row + 1)));

        // Loom (Cable)
        m_diodeTestModel->setItem(
            row, 1,
            new QStandardItem(data["cable"].toString()));

        // Source Connector (Anode)
        m_diodeTestModel->setItem(
            row, 2,
            new QStandardItem(data["anodeCon"].toString()));

        // Source Pin (Anode)
        m_diodeTestModel->setItem(
            row, 3,
            new QStandardItem(data["anodePin"].toString()));

        // Destination Connector (Cathode)
        m_diodeTestModel->setItem(
            row, 4,
            new QStandardItem(data["cathodeCon"].toString()));

        // Destination Pin (Cathode)
        m_diodeTestModel->setItem(
            row, 5,
            new QStandardItem(data["cathodePin"].toString()));

        // Expected Voltage Drop
        m_diodeTestModel->setItem(
            row, 6,
            new QStandardItem(data["expVoltage"].toString()));

        // Result - initially empty
        m_diodeTestModel->setItem(
            row, 7,
            new QStandardItem(""));

        // Remarks - initially empty
        m_diodeTestModel->setItem(
            row, 8,
            new QStandardItem(""));
    }

    // =====================================================
    // Table behavior
    // =====================================================

    ui->tableView_diodeTest->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    ui->tableView_diodeTest->setSelectionMode(
        QAbstractItemView::SingleSelection);

    ui->tableView_diodeTest->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    ui->tableView_diodeTest->setAlternatingRowColors(true);

    // =====================================================
    // Column sizing
    // =====================================================

    ui->tableView_diodeTest->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);

    // =====================================================
    // Row height
    // =====================================================

    ui->tableView_diodeTest->verticalHeader()
            ->setDefaultSectionSize(28);
}

QVector<QByteArray> MainWindow::constructUARTPacketsForRelay(
    const QVector<QString>& sourceCon,
    const QVector<QString>& sourcePin,
    const QVector<QString>& relaySideCon,
    const QVector<QString>& relaySideRSPins,
    const QVector<QString>& destCon,
    const QVector<QString>& destPin,
    const QVector<QString>& excitationVoltage,
    const QVector<QString>& expected)
{
    QVector<QByteArray> packets;

    constexpr int MAX_PACKET_SIZE = 1024;
    constexpr int HEADER_SIZE = 6;
    constexpr int MAX_INNER_LENGTH = 255;

    const int rowCount = sourceCon.size();

    // ---------------------------------------------------------
    // 1. Validate vector sizes
    // ---------------------------------------------------------
    if (rowCount == 0 ||
        sourcePin.size() != rowCount ||
        relaySideCon.size() != rowCount ||
        relaySideRSPins.size() != rowCount ||
        destCon.size() != rowCount ||
        destPin.size() != rowCount ||
        excitationVoltage.size() != rowCount ||
        expected.size() != rowCount)
    {
        writeToNotes("Relay packet error: Invalid or empty vectors.");
        return {};
    }

    // ---------------------------------------------------------
    // 2. Helper: encode a float as IEEE-754, MSB first
    // ---------------------------------------------------------
    auto appendFloatBE = [](QByteArray& data, float value)
    {
        quint32 bits = 0;
        static_assert(sizeof(bits) == sizeof(value),
                      "Float must be 32 bits");

        memcpy(&bits, &value, sizeof(bits));

        data.append(static_cast<char>((bits >> 24) & 0xFF));
        data.append(static_cast<char>((bits >> 16) & 0xFF));
        data.append(static_cast<char>((bits >> 8) & 0xFF));
        data.append(static_cast<char>(bits & 0xFF));
    };

    // ---------------------------------------------------------
    // 3. Helper: convert TP connector and pin to uint8
    // ---------------------------------------------------------
    auto parseEndpoint = [](const QString& connector,
                            const QString& pin,
                            quint8& connectorNumber,
                            quint8& pinNumber) -> bool
    {
        const QRegularExpression connectorRegex(
            R"(^TP-([0-9]+)(?:-|$))",
            QRegularExpression::CaseInsensitiveOption);

        const QRegularExpressionMatch match =
            connectorRegex.match(connector.trimmed());

        if (!match.hasMatch())
            return false;

        bool connectorOk = false;
        bool pinOk = false;

        const uint con =
            match.captured(1).toUInt(&connectorOk);

        const uint pinValue =
            pin.trimmed().toUInt(&pinOk);

        if (!connectorOk || !pinOk ||
            con > 255 || pinValue > 255)
        {
            return false;
        }

        connectorNumber = static_cast<quint8>(con);
        pinNumber = static_cast<quint8>(pinValue);

        return true;
    };

    // ---------------------------------------------------------
    // 4. Helper: parse coil Expected value as min-max resistance
    //    Example: "400-600"
    // ---------------------------------------------------------
    auto parseResistanceRange = [](const QString& value,
                                   quint16& minimum,
                                   quint16& maximum) -> bool
    {
        static const QRegularExpression rangeRegex(
            R"(^\s*(\d+)\s*-\s*(\d+)\s*$)");

        const QRegularExpressionMatch match =
            rangeRegex.match(value);

        if (!match.hasMatch())
            return false;

        bool minOk = false;
        bool maxOk = false;

        const uint minValue = match.captured(1).toUInt(&minOk);
        const uint maxValue = match.captured(2).toUInt(&maxOk);

        if (!minOk || !maxOk ||
            minValue > 65535 || maxValue > 65535 ||
            minValue > maxValue)
        {
            return false;
        }

        minimum = static_cast<quint16>(minValue);
        maximum = static_cast<quint16>(maxValue);

        return true;
    };

    // ---------------------------------------------------------
    // 5. Build complete Relay groups
    // ---------------------------------------------------------
    struct RelayGroup
    {
        quint8 relayId = 0;
        QByteArray bytes;
    };

    QVector<RelayGroup> groups;

    int row = 0;

    while (row < rowCount)
    {
        const QRegularExpression relayRegex(
            R"(^Relay_([1-9][0-9]*)(?:\s+.*)?$)",
            QRegularExpression::CaseInsensitiveOption);

        const QRegularExpressionMatch relayMatch =
            relayRegex.match(relaySideCon[row].trimmed());

        if (!relayMatch.hasMatch())
        {
            writeToNotes(QString("Relay packet error: Invalid relay name at row %1").arg(row));
            return {};
        }

        bool relayIdOk = false;

        const uint relayIdValue =
            relayMatch.captured(1).toUInt(&relayIdOk);

        if (!relayIdOk || relayIdValue > 255)
        {
            writeToNotes("Relay packet error: Relay ID exceeds uint8.");
            return {};
        }

        const quint8 relayId =
            static_cast<quint8>(relayIdValue);

        const int groupStart = row;

        // Gather consecutive rows belonging to this Relay_X.
        while (row < rowCount)
        {
            const QRegularExpressionMatch currentMatch =
                relayRegex.match(relaySideCon[row].trimmed());

            if (!currentMatch.hasMatch())
                return {};

            bool currentIdOk = false;

            const uint currentId =
                currentMatch.captured(1).toUInt(&currentIdOk);

            if (!currentIdOk)
                return {};

            if (currentId != relayIdValue)
                break;

            ++row;
        }

        const int groupEnd = row;

        int xCoilRow = -1;
        int yCoilRow = -1;

        // -----------------------------------------------------
        // Identify the coil rows and validate group contents
        // -----------------------------------------------------
        for (int i = groupStart; i < groupEnd; ++i)
        {
            const QString rsPins =
                relaySideRSPins[i].trimmed().toUpper();

            if (rsPins == "X1&X2")
            {
                if (xCoilRow != -1)
                {
                    writeToNotes(QString("Duplicate X1&X2 in Relay %1").arg(relayIdValue));
                    return {};
                }

                xCoilRow = i;
            }
            else if (rsPins == "Y1&Y2")
            {
                if (yCoilRow != -1)
                {
                    writeToNotes(QString("Duplicate Y1&Y2 in Relay %1").arg(relayIdValue));
                    return {};
                }

                yCoilRow = i;
            }
        }

        // Every group must have its X1&X2 coil.
        if (xCoilRow == -1)
        {
            writeToNotes(QString("Relay packet error: Missing X1&X2 in Relay %1").arg(relayIdValue));
            return {};
        }

        const bool isLatch = (yCoilRow != -1);

        // -----------------------------------------------------
        // 6. Parse excitation voltage from the X1&X2 row
        // -----------------------------------------------------
        QString voltageText =
            excitationVoltage[xCoilRow].trimmed();

        if (voltageText.endsWith('V', Qt::CaseInsensitive))
            voltageText.chop(1);

        bool voltageOk = false;
        const float voltage = voltageText.trimmed().toFloat(
            &voltageOk);

        if (!voltageOk || voltage <= 0.0f)
        {
            writeToNotes(QString("Relay packet error: Invalid excitation voltage %1")
                         .arg(excitationVoltage[xCoilRow]));
            return {};
        }

        // -----------------------------------------------------
        // 7. Build the inner Relay data
        // -----------------------------------------------------
        QByteArray innerData;

        auto appendCoil = [&](int coilRow) -> bool
        {
            quint8 srcConByte = 0;
            quint8 srcPinByte = 0;
            quint8 dstConByte = 0;
            quint8 dstPinByte = 0;

            if (!parseEndpoint(sourceCon[coilRow],
                               sourcePin[coilRow],
                               srcConByte,
                               srcPinByte) ||
                !parseEndpoint(destCon[coilRow],
                               destPin[coilRow],
                               dstConByte,
                               dstPinByte))
            {
                writeToNotes(QString("Relay packet error: Invalid coil endpoints Relay: %1 row: %2")
                             .arg(relayIdValue).arg(coilRow));
                return false;
            }

            quint16 minResistance = 0;
            quint16 maxResistance = 0;

            if (!parseResistanceRange(expected[coilRow],
                                      minResistance,
                                      maxResistance))
            {
                writeToNotes(QString("Relay packet error: Invalid coil resistance range %1")
                             .arg(expected[coilRow]));
                return false;
            }

            innerData.append(static_cast<char>(srcConByte));
            innerData.append(static_cast<char>(srcPinByte));
            innerData.append(static_cast<char>(dstConByte));
            innerData.append(static_cast<char>(dstPinByte));

            // Minimum resistance: uint16, MSB first
            innerData.append(static_cast<char>((minResistance >> 8) & 0xFF));
            innerData.append(static_cast<char>(minResistance & 0xFF));

            // Maximum resistance: uint16, MSB first
            innerData.append(static_cast<char>((maxResistance >> 8) & 0xFF));
            innerData.append(static_cast<char>(maxResistance & 0xFF));

            return true;
        };

        // X1&X2 always comes first.
        if (!appendCoil(xCoilRow))
            return {};

        // Y1&Y2 is present only for latch relays.
        if (isLatch && !appendCoil(yCoilRow))
            return {};

        // Append all NO/NC contact rows without resistance floats.
        for (int i = groupStart; i < groupEnd; ++i)
        {
            const QString rsPins =
                relaySideRSPins[i].trimmed().toUpper();

            if (rsPins == "X1&X2" || rsPins == "Y1&Y2")
                continue;

            quint8 srcConByte = 0;
            quint8 srcPinByte = 0;
            quint8 dstConByte = 0;
            quint8 dstPinByte = 0;

            if (!parseEndpoint(sourceCon[i],
                               sourcePin[i],
                               srcConByte,
                               srcPinByte) ||
                !parseEndpoint(destCon[i],
                               destPin[i],
                               dstConByte,
                               dstPinByte))
            {
                writeToNotes(QString("Relay packet error: Invalid contact endpoints Relay: %1 row: %2")
                             .arg(relayIdValue).arg(i));
                return {};
            }

            innerData.append(static_cast<char>(srcConByte));
            innerData.append(static_cast<char>(srcPinByte));
            innerData.append(static_cast<char>(dstConByte));
            innerData.append(static_cast<char>(dstPinByte));
        }

        // The inner data length is uint8. Never split a group.
        if (innerData.size() > MAX_INNER_LENGTH)
        {
            writeToNotes(QString("Relay packet error: Inner data exceeds 255 bytes. Relay: %1 Length: %2")
                         .arg(relayIdValue).arg(innerData.size()));
            return {};
        }

        // -----------------------------------------------------
        // 8. Build the Relay group record
        // -----------------------------------------------------
        RelayGroup group;
        group.relayId = relayId;

        group.bytes.append(static_cast<char>(relayId));

        group.bytes.append(
            static_cast<char>(isLatch ? 0xAE : 0xEA));

        appendFloatBE(group.bytes, voltage);

        group.bytes.append(
            static_cast<char>(innerData.size()));

        group.bytes.append(innerData);

        groups.append(group);

        writeToNotes(
            QString("Relay group built: ID = %1, Type = %2, Inner length = %3")
                .arg(relayIdValue)
                .arg(isLatch ? "Latch" : "Non-latch")
                .arg(innerData.size()));

        writeToNotes(
            QString("Relay %1 Inner Data (HEX): %2")
                .arg(relayIdValue)
                .arg(QString::fromLatin1(innerData.toHex(' ').toUpper())));

        writeToNotes(
            QString("Relay %1 Complete Group Bytes (HEX): %2")
                .arg(relayIdValue)
                .arg(QString::fromLatin1(group.bytes.toHex(' ').toUpper())));
    }

    // ---------------------------------------------------------
    // 9. Pack complete Relay groups into outer packets
    // ---------------------------------------------------------
    QByteArray payload;
    quint16 packetNo = 1;
    int groupsInPacket = 0;

    auto finalizePacket = [&]()
    {
        if (groupsInPacket == 0)
            return;

        QByteArray packet;

        // Header: 2F 2F
        packet.append(static_cast<char>(0x2F));
        packet.append(static_cast<char>(0x2F));

        // Packet number, uint16 MSB first.
        packet.append(static_cast<char>((packetNo >> 8) & 0xFF));
        packet.append(static_cast<char>(packetNo & 0xFF));

        // Payload length, uint16 MSB first; excludes 6-byte header.
        const quint16 dataLength =
            static_cast<quint16>(payload.size());

        packet.append(static_cast<char>((dataLength >> 8) & 0xFF));
        packet.append(static_cast<char>(dataLength & 0xFF));

        packet.append(payload);

        if (packet.size() > MAX_PACKET_SIZE)
        {
            writeToNotes("Relay packet error: Outer packet exceeds 1024 bytes.");
            packets.clear();
            return;
        }

        packets.append(packet);

        writeToNotes(QString("Relay packet: Packet No = %1 Groups = %2 Data length = %3 Total bytes = %4")
                     .arg(packetNo)
                     .arg(groupsInPacket)
                     .arg(dataLength)
                     .arg(packet.size()));

        ++packetNo;
        payload.clear();
        groupsInPacket = 0;
    };

    for (const RelayGroup& group : groups)
    {
        const int projectedSize =
            HEADER_SIZE + payload.size() + group.bytes.size();

        if (groupsInPacket > 0 &&
            projectedSize > MAX_PACKET_SIZE)
        {
            finalizePacket();

            // Abort if packet finalization detected an error.
            if (packets.isEmpty())
                return {};
        }

        // A single complete group must fit in one outer packet.
        if (HEADER_SIZE + group.bytes.size() > MAX_PACKET_SIZE)
        {
            writeToNotes("Relay packet error: Single group cannot fit.");
            return {};
        }

        payload.append(group.bytes);
        ++groupsInPacket;
    }

    finalizePacket();

    if (packets.isEmpty())
        return {};

    writeToNotes(QString("Total Relay packets generated: %1").arg(packets.size()));

    return packets;
}


QByteArray MainWindow::constructRelayStartPacket(
    quint8 totalRelays,
    quint16 totalPackets)
{
    QByteArray packet;

    packet.append(char(0x53));
    packet.append(char(0x4B));
    packet.append(char(0x3A));

    // Number of relays (uint8)
    packet.append(char(totalRelays));

    // Total packet count (uint16, MSB first)
    packet.append(char((totalPackets >> 8) & 0xFF));
    packet.append(char(totalPackets & 0xFF));

    // XOR checksum of all preceding bytes
    quint8 checksum = 0;

    for (char c : packet)
        checksum ^= static_cast<quint8>(c);

    packet.append(char(checksum));

    writeToNotes(
        QString("Relay start packet: %1")
            .arg(QString(packet.toHex(' ').toUpper())));

    return packet;
}


void MainWindow::populateRelayTestTable(
    const QVariantList &relayData)
{
    if (!m_relayTestModel)
    {
        m_relayTestModel = new QStandardItemModel(this);
    }

    m_relayTestModel->clear();

    m_relayTestModel->setColumnCount(14);

    m_relayTestModel->setHorizontalHeaderLabels({
        "S.No",
        "Cable",
        "Source Connector",
        "Source Pin",
        "Relay Connector",
        "Relay Pins",
        "Destination Connector",
        "Destination Pin",
        "Excitation Voltage",
        "Expected",
        "Condition",
        "Measured Value",
        "Results",
        "Remarks"
    });

    // Attach the model to the Relay table.
    ui->tableView_relay->setModel(m_relayTestModel);

    // Keep columns readable; allow horizontal scrolling.
    ui->tableView_relay->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Interactive);

    ui->tableView_relay->setHorizontalScrollMode(
        QAbstractItemView::ScrollPerPixel);

    // Set practical initial column widths.
    const int columnWidths[] = {
        55, 100, 240, 85, 260, 100, 240,
        100, 130, 100, 100, 130, 100, 180
    };

    for (int col = 0; col < 14; ++col)
    {
        ui->tableView_relay->setColumnWidth(
            col, columnWidths[col]);
    }

    int serialNumber = 1;
    int start = 0;

    // Database rows are already ordered by Relay group.
    while (start < relayData.size())
    {
        const QVariantMap firstMap =
            relayData[start].toMap();

        const QString relayName =
            firstMap["relaySideCon"].toString().simplified();

        // Find the end of this contiguous Relay group.
        int end = start + 1;

        while (end < relayData.size())
        {
            const QString nextRelay =
                relayData[end].toMap()["relaySideCon"]
                    .toString().simplified();

            if (nextRelay != relayName)
                break;

            ++end;
        }

        // Add every database row for OFF, then every row for ON.
        const QString conditions[] = {"OFF", "ON"};

        for (const QString &condition : conditions)
        {
            for (int i = start; i < end; ++i)
            {
                const QVariantMap m =
                    relayData[i].toMap();

                // Determine whether this is a coil row
                const QString relayPins =
                    m["relaySideRSPins"].toString().simplified();

                const bool isCoil =
                    relayPins.compare("X1&X2", Qt::CaseInsensitive) == 0 ||
                    relayPins.compare("Y1&Y2", Qt::CaseInsensitive) == 0;

                // Set expected value according to Relay condition
                QString expectedValue = m["exp"].toString();

                if (!isCoil)
                {
                    if (condition == "OFF")
                        expectedValue = "<2";
                    else if (condition == "ON")
                        expectedValue = ">2";
                }

                QStringList values = {
                    QString::number(serialNumber++),
                    m["cable"].toString(),
                    m["sourceCon"].toString(),
                    m["sourcePin"].toString(),
                    m["relaySideCon"].toString(),
                    m["relaySideRSPins"].toString(),
                    m["destCon"].toString(),
                    m["destPin"].toString(),
                    m["excitationVoltage"].toString(),
                    expectedValue,
                    condition,
                    "",     // Measured Value
                    "",     // Results
                    ""      // Remarks
                };

                QList<QStandardItem *> rowItems;

                for (const QString &value : values)
                {
                    QStandardItem *item =
                        new QStandardItem(value);

                    item->setEditable(false);
                    rowItems.append(item);
                }

                m_relayTestModel->appendRow(rowItems);
            }
        }

        start = end;
    }

    // Row selection and readable display.
    ui->tableView_relay->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    // Single-row selection
    ui->tableView_relay->setSelectionMode(
        QAbstractItemView::SingleSelection);

    // Disable editing
    ui->tableView_relay->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    ui->tableView_relay->setAlternatingRowColors(true);
    ui->tableView_relay->verticalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);

    writeToNotes(
        QString("Relay table populated: %1 database rows, "
                "%2 display rows.")
            .arg(relayData.size())
            .arg(m_relayTestModel->rowCount()));
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
    testCalibration=false;

    if(ui->comboBox_files->currentIndex()==0||ui->lineEdit_inspectedBy->text()==""
            ||ui->lineEdit_setNo->text()==""||ui->lineEdit_performedBy->text()==""
            ||ui->lineEdit_projectName->text()==""||ui->textEdit_notesOutput->toPlainText().isEmpty()
            ||ui->comboBox_test->currentIndex()==0)
    {
        QMessageBox::information(this,"Empty Fields","Please fill all details,select file and choose test");
        return;
    }

    QString cableName = ui->comboBox_files->currentText();

    QVariantList patch = getPatchData(cableName);
    QVariantList harness = getHarnessData(cableName);
    QVariantList diode   = getDiodeData(cableName);
    QVariantList relay   = getRelayData(cableName);

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


    // -------------------------------------------------
    // Diode Test BLOCK Start
    // -------------------------------------------------
    if (ui->comboBox_test->currentText() == "Diode")
    {
        if (patch.isEmpty() || diode.isEmpty())
        {
            QMessageBox::information(
                this,
                "Data Missing",
                "No Patch/Diode data found for this cable!");
            return;
        }

        if (!test->mapDiodeToHardware(patch, diode))
        {
            QMessageBox::warning(
                this, "Error", "Patch to Diode mapping failed.");
            return;
        }

        QVector<QByteArray> packets =
            constructUARTPacketsForDiode(
                test->m_diodeAnodeCon,
                test->m_diodeAnodePin,
                test->m_diodeCathodeCon,
                test->m_diodeCathodePin,
                test->m_diodeExpVoltage);

        if (packets.isEmpty())
        {
            QMessageBox::warning(
                this, "Error",
                "Failed to construct Diode packets.");
            return;
        }

        // ------------------------------------
        // Print Diode Data Packets
        // ------------------------------------
        writeToNotes("========== DIODE DATA PACKETS ==========");

        for (int i = 0; i < packets.size(); ++i)
        {
            writeToNotes(
                QString("Diode Packet %1 | Size: %2 bytes | HEX: %3")
                    .arg(i + 1)
                    .arg(packets[i].size())
                    .arg(QString(packets[i].toHex(' ').toUpper())));
        }

        QByteArray startPacket =
            constructDiodeStartPacket(
                static_cast<quint16>(packets.size()));

        //Table Filling and Reset State
        // -----------------------------------------
        // Populate result table FIRST
        // -----------------------------------------

        populateDiodeTestTable(diode);

        // -----------------------------------------
        // Get expected result count
        // -----------------------------------------

        int expectedDiodeResults =
            m_diodeTestModel->rowCount();

        // -----------------------------------------
        // Reset progress bar
        // -----------------------------------------

        ui->progressBar_DiodeTest->setValue(0);

        ui->progressBar_DiodeTest->setMaximum(
            expectedDiodeResults);

        // -----------------------------------------
        // Move to Diode test page
        // -----------------------------------------

        ui->stackedWidget->setCurrentWidget(
            ui->page_diodeTest);

        ui->label_test_Diode->setText("Diode Test");

        // ------------------------------------
        // Send Start Packet and begin ACK flow
        // ------------------------------------
        test->startDiodeTransmission(startPacket, packets);

    }
    // -------------------------------------------------
    // Diode Test BLOCK End
    // -------------------------------------------------

    // -------------------------------------------------
    // Relay Test BLOCK Start
    // -------------------------------------------------
    if (ui->comboBox_test->currentText() == "Relay")
    {
        if (patch.isEmpty() || relay.isEmpty())
        {
            QMessageBox::information(
                this,
                "Data Missing",
                "No Patch/Relay data found for this cable!");
            return;
        }


        if (!test->mapRelayToHardware(patch, relay))
        {
            QMessageBox::warning(
                this, "Error", "Patch to Relay mapping failed.");
            return;
        }

        QVector<QByteArray> relayPackets =
            constructUARTPacketsForRelay(
                test->m_relaySourceCon,
                test->m_relaySourcePin,
                test->m_relaySideCon,
                test->m_relaySideRSPins,
                test->m_relayDestCon,
                test->m_relayDestPin,
                test->m_relayExcitationVoltage,
                test->m_relayExp);

        if (relayPackets.isEmpty())
        {
            QMessageBox::warning(
                this,
                "Relay Packet Error",
                "Failed to construct Relay UART packets. Check the debug output.");
            return;
        }

        // ---------------------------------------------------------
        // Print our precious Relay packets
        // ---------------------------------------------------------
        writeToNotes(
            QString("========== RELAY PACKETS GENERATED: %1 ==========")
                .arg(relayPackets.size()));

        for (int i = 0; i < relayPackets.size(); ++i)
        {
            const QByteArray &packet = relayPackets[i];

            const QString hexData =
                QString::fromLatin1(packet.toHex(' ').toUpper());

            writeToNotes(
                QString("Relay Packet %1 | Size: %2 bytes")
                    .arg(i + 1)
                    .arg(packet.size()));

            writeToNotes(hexData);

            qDebug().noquote()
                << QString("Relay Packet %1 (%2 bytes): %3")
                       .arg(i + 1)
                       .arg(packet.size())
                       .arg(hexData);
        }

        writeToNotes("========== END OF RELAY PACKETS ==========");

        // Count distinct Relay IDs (Relay_1, Relay_2, Relay_3, etc.)
        QSet<QString> relayIds;

        for (const QString &relayName : test->m_relaySideCon)
        {
            QRegularExpression re(
                R"(^Relay_([1-9][0-9]*)(?:\s+.*)?$)",
                QRegularExpression::CaseInsensitiveOption);

            const QRegularExpressionMatch match =
                re.match(relayName.trimmed());

            if (match.hasMatch())
                relayIds.insert(match.captured(1));
        }

        const quint8 totalRelays =
            static_cast<quint8>(relayIds.size());

        QByteArray startPacket =
            constructRelayStartPacket(
                totalRelays,
                static_cast<quint16>(relayPackets.size()));


        // -----------------------------------------
        // Table Filling and Reset State
        // -----------------------------------------

        // Populate Relay result table FIRST.
        populateRelayTestTable(relay);

        m_relayMeasuredValues.clear();
        m_relayMeasuredValues.resize(m_relayTestModel->rowCount());

        // -----------------------------------------
        // Get expected result count
        // -----------------------------------------

        int expectedRelayResults =
            m_relayTestModel->rowCount();

        // -----------------------------------------
        // Reset progress bar
        // -----------------------------------------

        ui->progressBar_RelayTest->setValue(0);

        ui->progressBar_RelayTest->setMaximum(
            expectedRelayResults);

        // -----------------------------------------
        // Move to Relay test page
        // -----------------------------------------

        ui->stackedWidget->setCurrentWidget(
            ui->page_relayTest);

        ui->label_test_Relay->setText("Relay Test");

        test->setRelayExpectedResults(m_relayTestModel->rowCount());
        // ------------------------------------
        // Send Start Packet and begin ACK flow
        // ------------------------------------
        test->startRelayTransmission(startPacket, relayPackets);

    }
    // -------------------------------------------------
    // Relay Test BLOCK End
    // -------------------------------------------------

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


    // Insulation/Isolation Packet logic start ------------------------------------------------

    if(ui->comboBox_test->currentText() == "Insulation" ||
       ui->comboBox_test->currentText() == "Isolation")
    {
        writeToNotes("======================================");
        writeToNotes("STEP-1 : DIVIDING HARNESS INTO LOOMS");
        writeToNotes("======================================");

        QVector<int> insulationNetsPerLoom;

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

        // Final storer
        QVector<QByteArray> loomPackets;
        QVector<InsulationTableNet> tableNets;

        //Loom wise net creation

        quint16 loomNumber = 1;

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


            for(int net = 0; net < fixedGroups.size(); ++net)
            {
                InsulationTableNet tableNet;

                tableNet.loomNo = loomNumber;
                tableNet.netNo = net;
                tableNet.endpoints = fixedGroups[net];

                tableNets.append(tableNet);
            }

            insulationNetsPerLoom.append(fixedGroups.size());

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

        test->setInsulationLoomNetCounts(insulationNetsPerLoom);

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


        //Reset Progress
        currentPercentage = 0.0;
        ui->progressBar_InsuIsoTest->setValue(0);
        currentLoomNetProgress = 0.0;

        //Sending Packet Creation To testcontroller
        quint16 numberOfPackets =
                static_cast<quint16>(uartPackets.size());

        quint16 numberOfLooms =
                static_cast<quint16>(loomPackets.size());

        totalInsulationLooms = numberOfLooms;

        float insulationVoltage =
                static_cast<float>(
                    ui->doubleSpinBox_insuIsoVoltage->value());

        float resistanceThreshold =
                static_cast<float>(
                    ui->doubleSpinBox_ResistanceThreshold->value());

        quint8 testBetweenConnectors =
                ui->checkBox_TestBwConnectors->isChecked()
                ? 0x01
                : 0x00;

        quint8 testType =
            (ui->comboBox_test->currentText() == "Isolation")
            ? 0x4F
            : 0x3F;

        test->sendInsulationStartPacket(
            numberOfPackets,
            numberOfLooms,
            insulationVoltage,
            resistanceThreshold,
            testBetweenConnectors,
            testType,
            uartPackets);

        //Page Movement
        // Move to two wire test page
        ui->stackedWidget->setCurrentWidget(
            ui->page_InsuIsoTEst);

        ui->label_test_InsuIso->setText(
            ui->comboBox_test->currentText()+" Test");

        populateInsulationTable(tableNets, patch);

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

QVariantList MainWindow::getDiodeData(const QString &cableName)
{
    QVariantList rows;

    if (!db.isOpen()) {
        writeToNotes("getDiodeData: DB not open");
        return rows;
    }

    QString tableName = cableName + "_diode";

    if (!db.tables().contains(tableName)) {
        writeToNotes("getDiodeData: Table not found → " + tableName);
        return rows;
    }

    QString sql =
        "SELECT cable, anodeCon, anodePin, "
        "cathodeCon, cathodePin, expVoltage "
        "FROM " + tableName;

    QSqlQuery q(db);

    if (!q.exec(sql)) {
        writeToNotes("getDiodeData: query failed → " +
                     q.lastError().text());
        return rows;
    }

    while (q.next()) {
        QVariantMap m;

        m["cable"]       = q.value(0).toString();
        m["anodeCon"]    = q.value(1).toString();
        m["anodePin"]    = q.value(2).toString();
        m["cathodeCon"]  = q.value(3).toString();
        m["cathodePin"]  = q.value(4).toString();
        m["expVoltage"]  = q.value(5).toString();

        rows.append(m);
    }

    writeToNotes(QString("getDiodeData: %1 rows for %2")
                 .arg(rows.size())
                 .arg(cableName));

    return rows;
}


QVariantList MainWindow::getRelayData(const QString &cableName)
{
    QVariantList rows;

    if (!db.isOpen())
    {
        writeToNotes("getRelayData: DB not open");
        return rows;
    }

    const QString tableName = cableName + "_relay";

    if (!db.tables().contains(tableName))
    {
        writeToNotes("getRelayData: Table not found -> " + tableName);
        return rows;
    }

    const QString sql =
        QString(
            "SELECT cable, sourceCon, sourcePin, "
            "relaySideCon, relaySideRSPins, "
            "destCon, destPin, excitationVoltage, exp "
            "FROM \"%1\"")
            .arg(tableName);

    QSqlQuery q(db);

    if (!q.exec(sql))
    {
        writeToNotes(
            "getRelayData: query failed -> " +
            q.lastError().text());

        return rows;
    }

    while (q.next())
    {
        QVariantMap m;

        m["cable"]             = q.value(0).toString();
        m["sourceCon"]         = q.value(1).toString();
        m["sourcePin"]         = q.value(2).toString();
        m["relaySideCon"]      = q.value(3).toString();
        m["relaySideRSPins"]   = q.value(4).toString();
        m["destCon"]           = q.value(5).toString();
        m["destPin"]           = q.value(6).toString();
        m["excitationVoltage"] = q.value(7).toString();
        m["exp"]               = q.value(8).toString();

        rows.append(m);
    }

    writeToNotes(
        QString("getRelayData: %1 rows for %2")
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
    if(test->isTwoWireTestRunning())
    {
        QMessageBox::warning(this,"Two Wire Test","Please wait two wire test is running");
        return;
    }

    ui->stackedWidget->setCurrentWidget(ui->page_test);
}

void MainWindow::on_pushButton_selfTest_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_selfTest);
}


void MainWindow::on_pushButton_selfBack_clicked()
{
    if(test->isSelfTestRunning())
    {
        QMessageBox::warning(
            this,
            "Self Test Running",
            "Please wait until the self test is completed.");
        return;
    }

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

    if(test->isSelfTestRunning())
    {
        QMessageBox::warning(
            this,
            "Self Test Running",
            "Self test is already running.");
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
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Self Test",
                "Are you sure you want to abort the Self test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_stopTwoWireTest_clicked()
{
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Two Wire Test",
                "Are you sure you want to abort the Two Wire test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_calibrationTest_clicked()
{
    // -------------------------------------------------
    // Create Calibration table model
    // -------------------------------------------------
    testCalibration=true;
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
    if (test->isCalibrationRunning())
       {
           QMessageBox::warning(
               this,
               "Calibration Test Running",
               "Calibration test is already running.");
           return;
       }
    testCalibration=true;

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
    if (test->isCalibrationRunning())
       {
           QMessageBox::warning(
               this,
               "Calibration Test Running",
               "Please wait until the calibration test is completed.");
           return;
       }

    ui->lineEdit_calibratedValue->clear();
    ui->stackedWidget->setCurrentWidget(ui->page_testPage);
}

void MainWindow::on_pushButton_calibrationAbort_clicked()
{
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Calibration",
                "Are you sure you want to abort the calibration test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_calibrationSave_clicked()
{
    if (test->isCalibrationRunning())
       {
           QMessageBox::warning(
               this,
               "Calibration Test Running",
               "Please wait until the calibration test is completed.");
           return;
       }

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

        // ---------------------------------------------------------
        // User representation -> Hardware representation
        // ---------------------------------------------------------

        int logicalConnector = connectorText.toInt();
        int logicalPin = pinText.toInt();

        int dbConnector;
        int dbPin;

        if (logicalPin <= 64)
        {
            // First half
            //
            // TP1  -> TP1
            // TP2  -> TP3
            // ...
            // TP15 -> TP29

            dbConnector =
                    (logicalConnector * 2) - 1;

            dbPin = logicalPin;
        }
        else
        {
            // Second half
            //
            // TP1  -> TP2
            // TP2  -> TP4
            // ...
            // TP14 -> TP28
            //
            // Special:
            // TP15 second half -> TP31
            // because SLOT 30 is missing.

            if (logicalConnector == 15)
            {
                dbConnector = 31;
            }
            else
            {
                dbConnector =
                        logicalConnector * 2;
            }

            dbPin = logicalPin - 64;
        }

        // ---------------------------------------------------------
        // Insert hardware representation into DB
        // ---------------------------------------------------------

        insertQuery.bindValue(
            ":connector",
            dbConnector);

        insertQuery.bindValue(
            ":pin",
            dbPin);

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
                        "User Connector: %2\n"
                        "User Pin: %3\n"
                        "DB Connector: %4\n"
                        "DB Pin: %5\n"
                        "Value: %6\n\n"
                        "Database Error:\n%7")
                    .arg(row + 1)
                    .arg(logicalConnector)
                    .arg(logicalPin)
                    .arg(dbConnector)
                    .arg(dbPin)
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

    // Save successful
    ui->tableView_Calibration->setClipboardEnabled(false);

    ui->tableView_Calibration->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    ui->tableView_Calibration->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    ui->tableView_Calibration->setSelectionMode(
        QAbstractItemView::SingleSelection);

    QMessageBox::information(
        this,
        "Calibration Save",
        QString(
            "Calibration data saved successfully.\n\n"
            "Total rows saved: %1")
        .arg(m_calibrationModel->rowCount()));
    qDebug()<<"database saving for calibration complted";


    //pdf saving
    failedPinsCalibrate=0;
    generateCalibratePDF();

}
void MainWindow::generateCalibratePDF()
{
    if(!m_calibrationModel)
    {
        return;
    }
qDebug()<<"pdf func for calibration";
 //    Create PDF folder on Desktop
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/Calibration_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Generate filename with timestamp
    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    QString fileName = folderPath +
            "/calibration_" + timestamp + ".pdf";

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    // Printer setup
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOrientation(QPrinter::Portrait);

    printer.setPageMargins(
                15, 15, 15, 20,
                QPrinter::Millimeter);

    printer.setResolution(96);

    // Start painting
    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
                    this,
                    "PDF",
                    "Unable to create PDF.");
        return;
    }

    // Calculate page dimensions
    QRect deviceRect =
            printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = deviceRect.width();
    const qreal pageHeight = deviceRect.height();

    const qreal bottom = pageHeight - 25;


    int pageNumber = 1;

    // 1. Draw report header
    qreal y = drawTwoWireTestHeader(
                painter,
                pageWidth,
                failedPinsCalibrate);
    qDebug()<<"header part completed";

    // 2. Draw table
    drawCalibrateTable(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);
    qDebug()<<"table part completed";

    // 3. Draw notes, signatures and final page number
    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // Finish PDF generation
    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "PDF generated successfully.");
}
void MainWindow::drawCalibrateTable(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{
    const qreal left = 0;

    // Table column widths
    const qreal proportions[] = {
        0.335,0.333,0.333
    };

    qreal colWidths[3];

    for (int c = 0; c < 3; ++c)
        colWidths[c] = pageWidth * proportions[c];

    // Table headers
    const QStringList headers = {
        "Connector", "Pin","Calibration Value(Ω)"
    };

    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.75;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.50);
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };

    // Fonts
    QFont normalFont("Times New Roman", 11);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    // Draw a table cell
    auto drawCell = [&](
            const QRectF &rect,
            const QString &value,
            const QFont &font,
            const QColor &background)
    {
        if (background != Qt::transparent)
            painter.fillRect(rect, background);

        painter.setPen(Qt::black);
        painter.drawRect(rect);

        painter.setFont(font);

        QRectF textRect = rect.adjusted(3, 2, -3, -2);

        painter.drawText(
                    textRect,
                    Qt::AlignCenter |
                    Qt::TextWordWrap |
                    Qt::TextWrapAnywhere,
                    value);
    };

    // Draw the table header
    auto drawTableHeader = [&](qreal &currentY)
    {
        const qreal headerHeight = 45;
        qreal x = left;

        for (int c = 0; c < 3; ++c)
        {
            QRectF cell(
                        x,
                        currentY,
                        colWidths[c],
                        headerHeight);

            drawCell(
                        cell,
                        headers[c],
                        boldFont,
                        QColor(204, 204, 204));

            x += colWidths[c];
        }

        currentY += headerHeight;
    };

    int totalRows = m_calibrationModel->rowCount();
    int currentRow = 0;

    // Draw table header on the first page
    drawWatermark();
    drawTableHeader(y);

    // Draw the table rows
    while (currentRow < totalRows)
    {
        // Calculate dynamic row height
        qreal rowHeight =30;

        for (int c = 0; c < 3; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_calibrationModel->item(currentRow, c);

            if (item)
                value = item->text();

            QFont cellFont = normalFont;

//            if (c == 7 &&
//                (value.compare(
//                     "Pass", Qt::CaseInsensitive) == 0 ||
//                 value.compare(
//                     "Fail", Qt::CaseInsensitive) == 0))
//            {
//                cellFont = boldFont;
//            }

            QFontMetricsF metrics(cellFont);

            QRectF textRect = metrics.boundingRect(
                        QRectF(
                            0, 0,
                            qMax(1, static_cast<int>(
                                 colWidths[c] - 8)),
                            10000),
                        Qt::AlignCenter |
                        Qt::TextWordWrap |
                        Qt::TextWrapAnywhere,
                        value);

            rowHeight = qMax<qreal>(
                        rowHeight,
                        textRect.height() +20);
        }

        // Start a new page if the row will not fit
        if (y + rowHeight > bottom)
        {
            // Current page footer
            painter.setFont(boldFont);
            painter.setPen(Qt::black);

            painter.drawText(
                        QRectF(
                            0,
                            pageHeight - 25,
                            pageWidth,
                            15),
                        Qt::AlignCenter,
                        "Page " + QString::number(pageNumber));

            printer.newPage();

            ++pageNumber;

            y = 0;

            // Repeat table header on the new page
            drawWatermark();
            drawTableHeader(y);
        }

        // Draw current row
        qreal x = left;

        for (int c = 0; c < 3; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_calibrationModel->item(currentRow, c);

            if (item)
                value = item->text();

            QColor background = Qt::transparent;
            QFont cellFont = normalFont;

//            // Result column: Pass / Fail
//            if (c == 7)
//            {
//                if (value.compare(
//                            "Pass", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(144, 238, 144);
//                    cellFont = boldFont;
//                }
//                else if (value.compare(
//                                 "Fail", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(255, 127, 127);
//                    cellFont = boldFont;
//                }
//            }

            QRectF cell(
                        x,
                        y,
                        colWidths[c],
                        rowHeight);

            drawCell(
                        cell,
                        value,
                        cellFont,
                        background);

            x += colWidths[c];
        }

        y += rowHeight;
        ++currentRow;
    }
    y+=20;
}


void MainWindow::on_pushButton_calibrationView_clicked()
{
    if (test->isCalibrationRunning())
       {
           QMessageBox::warning(
               this,
               "Calibration Test Running",
               "Please wait until the calibration test is completed.");
           return;
       }

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
        int dbConnector =
                query.value("connector").toInt();

        int dbPin =
                query.value("pin").toInt();

        QString calibrationValue =
                query.value("calibrationValue").toString();

        // ---------------------------------------------------------
        // Hardware representation -> User representation
        // ---------------------------------------------------------

        int userConnector;
        int userPin;

        if (dbConnector == 31)
        {
            // SLOT 31 is the second half of TP15
            userConnector = 15;
            userPin = dbPin + 64;
        }
        else if (dbConnector % 2 == 1)
        {
            // First half
            //
            // TP1  -> User TP1
            // TP3  -> User TP2
            // ...
            // TP29 -> User TP15

            userConnector =
                    (dbConnector + 1) / 2;

            userPin = dbPin;
        }
        else
        {
            // Second half
            //
            // TP2  -> User TP1
            // TP4  -> User TP2
            // ...
            // TP28 -> User TP14

            userConnector =
                    dbConnector / 2;

            userPin = dbPin + 64;
        }

        // ---------------------------------------------------------
        // Fill user-visible model
        // ---------------------------------------------------------

        m_calibrationModel->setItem(
            row,
            0,
            new QStandardItem(
                QString::number(userConnector)));

        m_calibrationModel->setItem(
            row,
            1,
            new QStandardItem(
                QString::number(userPin)));

        m_calibrationModel->setItem(
            row,
            2,
            new QStandardItem(
                calibrationValue));

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
        .arg(row));
}

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

}

void MainWindow::on_pushButton_editCalibration_clicked()
{
    if (test->isCalibrationRunning())
    {
        QMessageBox::warning(
            this,
            "Calibration Test Running",
            "Please wait until the calibration test is completed.");

        return;
    }

    ui->tableView_Calibration->setClipboardEnabled(true);

    ui->tableView_Calibration->setEditTriggers(
        QAbstractItemView::DoubleClicked);

    ui->tableView_Calibration->setSelectionBehavior(
        QAbstractItemView::SelectItems);

    ui->tableView_Calibration->setSelectionMode(
        QAbstractItemView::ExtendedSelection);

    QMessageBox::information(
        this,
        "Calibration Value",
        "Double click to edit.\n\n"
        "You can select multiple calibration values "
        "and use Ctrl+C / Ctrl+V.");
}


void MainWindow::on_pushButton_backFromInsuIso_clicked()
{
    if (test->isInsuIsoTestRunning())
       {
           QMessageBox::warning(
               this,
               "Test Running",
               "Please wait until the test is completed.");
           return;
       }

     ui->stackedWidget->setCurrentWidget(ui->page_test);
}

void MainWindow::on_pushButton_stopInsuIso_clicked()
{
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Test",
                "Are you sure you want to abort the test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_saveInsuIso_clicked()
{
    if (test->isInsuIsoTestRunning())
       {
           QMessageBox::warning(
               this,
               "Test Running",
               "Please wait until the test is completed.");
           return;
       }
    failedPinsInso=0;
    generateInsoIsoPDF();

}
void MainWindow::generateInsoIsoPDF()
{
    if(!model)
    {
        return;
    }

 //    Create PDF folder on Desktop
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString testType=ui->comboBox_test->currentIndex()==2?"Isolation":"Insulation";

    QString folderPath = desktopPath + "/InsoIso_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Generate filename with timestamp
    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    QString fileName = folderPath +
            "/"+testType+"_" + timestamp + ".pdf";

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    // Printer setup
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOrientation(QPrinter::Portrait);

    printer.setPageMargins(
                15, 15, 15, 20,
                QPrinter::Millimeter);

    printer.setResolution(96);

    // Start painting
    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
                    this,
                    "PDF",
                    "Unable to create PDF.");
        return;
    }

    // Calculate page dimensions
    QRect deviceRect =
            printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = deviceRect.width();
    const qreal pageHeight = deviceRect.height();

    const qreal bottom = pageHeight - 25;

    // Count failed pins from Result column (column 8)


    for (int row = 0;
         row < model->rowCount();
         ++row)
    {
        QStandardItem *item =
                model->item(row, 7);

        if (item &&
            item->text().compare(
                "Fail", Qt::CaseInsensitive) == 0)
        {
            ++failedPinsInso;
        }
    }

    int pageNumber = 1;

    // 1. Draw report header
    qreal y = drawTwoWireTestHeader(
                painter,
                pageWidth,
                failedPinsInso);

    // 2. Draw table
    drawInsoIsoTable(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // 3. Draw notes, signatures and final page number
    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // Finish PDF generation
    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "PDF generated successfully.");
}
void MainWindow::drawInsoIsoTable(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{
    const qreal left = 0;

    // Table column widths
    const qreal proportions[] = {
        0.055, 0.065, 0.050, 0.175, 0.175,
        0.140, 0.155, 0.100, 0.085
    }; // Sum = 1.000

    qreal colWidths[9];

    for (int c = 0; c < 9; ++c)
        colWidths[c] = pageWidth * proportions[c];

    // Table headers
    const QStringList headers = {
        "S.No", "Loom\nNo","Net No","Src Net",
        "Dstn Net","Failed Net",
        "Exp\nValue", "Result", "Remarks"
    };

    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.50;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.30);  // 12% opacity
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };

    // Fonts
    QFont normalFont("Times New Roman", 11);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    // Draw a table cell
    auto drawCell = [&](
            const QRectF &rect,
            const QString &value,
            const QFont &font,
            const QColor &background)
    {
        if (background != Qt::transparent)
            painter.fillRect(rect, background);

        painter.setPen(Qt::black);
        painter.drawRect(rect);

        painter.setFont(font);

        QRectF textRect = rect.adjusted(3, 2, -3, -2);

        painter.drawText(
                    textRect,
                    Qt::AlignCenter |
                    Qt::TextWordWrap |
                    Qt::TextWrapAnywhere,
                    value);
    };

    // Draw the table header
    auto drawTableHeader = [&](qreal &currentY)
    {
        const qreal headerHeight = 45;
        qreal x = left;

        for (int c = 0; c < 9; ++c)
        {
            QRectF cell(
                        x,
                        currentY,
                        colWidths[c],
                        headerHeight);

            drawCell(
                        cell,
                        headers[c],
                        boldFont,
                        QColor(204, 204, 204));

            x += colWidths[c];
        }

        currentY += headerHeight;
    };

    int totalRows = model->rowCount();
    int currentRow = 0;

    // Draw table header on the first page
    drawWatermark();
    drawTableHeader(y);

    // Draw the table rows
    while (currentRow < totalRows)
    {
        // Calculate dynamic row height
        qreal rowHeight = 30;

        for (int c = 0; c < 9; ++c)
        {
            QString value;

            QStandardItem *item =
                    model->item(currentRow, c);

            if (item)
                value = item->text();


            QFont cellFont = normalFont;

            if (c == 7 &&
                (value.compare(
                     "Pass", Qt::CaseInsensitive) == 0 ||
                 value.compare(
                     "Fail", Qt::CaseInsensitive) == 0))
            {
                cellFont = boldFont;
            }

            QFontMetricsF metrics(cellFont);

            QRectF textRect = metrics.boundingRect(
                        QRectF(
                            0, 0,
                            qMax(1, static_cast<int>(
                                 colWidths[c] - 8)),
                            10000),
                        Qt::AlignCenter |
                        Qt::TextWordWrap |
                        Qt::TextWrapAnywhere,
                        value);

            rowHeight = qMax<qreal>(
                        rowHeight,
                        textRect.height() +20);
        }

        // Start a new page if the row will not fit
        if (y + rowHeight > bottom)
        {
            // Current page footer
            painter.setFont(boldFont);
            painter.setPen(Qt::black);

            painter.drawText(
                        QRectF(
                            0,
                            pageHeight - 25,
                            pageWidth,
                            15),
                        Qt::AlignCenter,
                        "Page " + QString::number(pageNumber));

            printer.newPage();

            ++pageNumber;

            y = 0;

            // Repeat table header on the new page
            drawWatermark();
            drawTableHeader(y);
        }

        // Draw current row
        qreal x = left;

        for (int c = 0; c < 9; ++c)
        {
            QString value;

            QStandardItem *item =
                    model->item(currentRow, c);

            if (item)
                value = item->text();

            QColor background = Qt::transparent;
            QFont cellFont = normalFont;

            // Result column: Pass / Fail
//            if (c == 7)
//            {
//                if (value.compare(
//                            "Pass", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(144, 238, 144);
//                    cellFont = boldFont;
//                }
//                else if (value.compare(
//                                 "Fail", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(255, 127, 127);
//                    cellFont = boldFont;
//                }
//            }

            QRectF cell(
                        x,
                        y,
                        colWidths[c],
                        rowHeight);

            drawCell(
                        cell,
                        value,
                        cellFont,
                        background);

            x += colWidths[c];
        }

        y += rowHeight;
        ++currentRow;
    }
    y+=20;
}

void MainWindow::on_pushButton_openInsuIsoFiles_clicked()
{
    if (test->isInsuIsoTestRunning())
       {
           QMessageBox::warning(
               this,
               "Test Running",
               "Please wait until the test is completed.");
           return;
       }

    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString testType=ui->comboBox_test->currentIndex()==2?"Isolation":"Insulation";

    QString folderPath = desktopPath + "/InsoIso_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Open the folder in File Explorer
    QDesktopServices::openUrl(
                QUrl::fromLocalFile(folderPath));

}

void MainWindow::on_pushButton_saveTwoWirePdf_clicked()
{
    if (test->isTwoWireTestRunning())
       {
           QMessageBox::warning(
               this,
               "Test Running",
               "Please wait until the test is completed.");
           return;
       }
     failedPinsCount = 0;
     generateTwoWireTestPDF();
}


qreal MainWindow::drawTwoWireTestHeader(
        QPainter &painter,
        qreal pageWidth,
        int failedPinsCount)
{
    QImage solarLogo(":/new/prefix1/images/solar1.png");
   // QImage drdoLogo(":/new/prefix1/images/drdo.png");
    QImage databaseImage;


    const qreal logoWidth = 105;
    const qreal logoHeight = 75;

    if (!d_imageData.isEmpty() &&
            databaseImage.loadFromData(d_imageData))
    {
        painter.drawImage(
            QRectF(pageWidth - logoWidth, 0,
                   logoWidth, logoHeight),
            databaseImage
        );
    }


    QFont normalFont("Times New Roman", 11);
    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    QFont titleFont("Times New Roman", 14);
    titleFont.setBold(true);

    QFont companyFont("Times New Roman", 16);
    companyFont.setBold(true);

    if (!solarLogo.isNull())
        painter.drawImage(QRectF(0, 0, logoWidth, logoHeight),
                          solarLogo);

    if (!d_imageData.isEmpty() &&
            databaseImage.loadFromData(d_imageData))
    {
        painter.drawImage(
            QRectF(pageWidth - logoWidth, 0,
                   logoWidth, logoHeight),
            databaseImage
        );
    }
    painter.setPen(Qt::black);
    painter.setFont(companyFont);

    painter.drawText(
        QRectF(logoWidth,45, pageWidth - 2 * logoWidth, 25),
      Qt::AlignCenter,
        "SOLAR DEFENCE AND AEROSPACE LIMITED");

    painter.setFont(titleFont);

    painter.drawText(
        QRectF(0, 85, pageWidth, 30),
        Qt::AlignCenter,
        d_projectName + " CABLE HARNESS TEST REPORT"
    );

    qreal y = 155;

    painter.setFont(normalFont);


    QString testType=testCalibration?"Calibration":ui->comboBox_test->currentText();

    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Date : " +
                     QDate::currentDate().toString("dd-MM-yyyy"));
    y += 22;

    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Time : " +
                     QTime::currentTime().toString("hh:mm:ss AP"));
    y += 22;
    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Test Type : " +
                    testType);
    y += 22;
    if(!testCalibration)
    {

    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Set No : " + ui->lineEdit_setNo->text());
    y += 22;



    QString result = (failedPinsCount == 0) ? "Pass" : "Fail";

    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Final Result : " + result);
    y += 22;

    painter.setFont(boldFont);


    painter.drawText(QRectF(0, y, pageWidth, 14),
                     Qt::AlignLeft,
                     "Total Failed Pins : " +
                     QString::number(failedPinsCount));
    }

    return y + 50;
    qDebug()<<"header part completed successfully";
}

void MainWindow::generateTwoWireTestPDF()
{
    if (!m_twoWireTestModel)
        return;

 //    Create PDF folder on Desktop
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/TwoWire_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Generate filename with timestamp
    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    QString fileName = folderPath +
            "/TwoWireTest_" + timestamp + ".pdf";

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    // Printer setup
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOrientation(QPrinter::Portrait);

    printer.setPageMargins(
                15, 15, 15, 20,
                QPrinter::Millimeter);

    printer.setResolution(96);

    // Start painting
    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
                    this,
                    "PDF",
                    "Unable to create PDF.");
        return;
    }

    // Calculate page dimensions
    QRect deviceRect =
            printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = deviceRect.width();
    const qreal pageHeight = deviceRect.height();

    const qreal bottom = pageHeight - 25;

    // Count failed pins from Result column (column 8)
    int failedPinsCount = 0;

    for (int row = 0;
         row < m_twoWireTestModel->rowCount();
         ++row)
    {
        QStandardItem *item =
                m_twoWireTestModel->item(row, 8);

        if (item &&
            item->text().compare(
                "Fail", Qt::CaseInsensitive) == 0)
        {
            ++failedPinsCount;
        }
    }

    int pageNumber = 1;

    // 1. Draw report header
    qreal y = drawTwoWireTestHeader(
                painter,
                pageWidth,
                failedPinsCount);

    // 2. Draw table
    drawTwoWireTestTable(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // 3. Draw notes, signatures and final page number
    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // Finish PDF generation
    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "PDF generated successfully.");
}
void MainWindow::drawDiodeTestTable(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{
    const qreal left = 0;

    // Table column widths
    const qreal proportions[] = {
        0.055, 0.085, 0.175, 0.072, 0.175,
        0.075, 0.160, 0.105, 0.098
    };

    qreal colWidths[9];
   // qDebug() << "Column widths count:" << colWidths.size();
    qDebug() << "Diode model columns:" << diodeModel->columnCount();

    for (int c = 0; c < 9; ++c)
        colWidths[c] = pageWidth * proportions[c];

    // Table headers
    const QStringList headers = {
        "S.No", "Loom", "Src Conn\n(Anode)", "Src\nPin",
        "Dstn Conn\n(Cathode)", "Dstn\nPin",
        "Voltage\nDrop Across\nDiode", "Result", "Remarks"
    };

    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.75;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.50);  // 12% opacity
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };


    // Fonts
    QFont normalFont("Times New Roman", 11);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    // Draw a table cell
    auto drawCell = [&](
            const QRectF &rect,
            const QString &value,
            const QFont &font,
            const QColor &background)
    {
        if (background != Qt::transparent)
            painter.fillRect(rect, background);

        painter.setPen(Qt::black);
        painter.drawRect(rect);

        painter.setFont(font);

        QRectF textRect = rect.adjusted(3, 2, -3, -2);

        painter.drawText(
                    textRect,
                    Qt::AlignCenter |
                    Qt::TextWordWrap |
                    Qt::TextWrapAnywhere,
                    value);
    };

    // Draw the table header
    auto drawTableHeader = [&](qreal &currentY)
    {
        const qreal headerHeight = 55;
        qreal x = left;

        for (int c = 0; c < 9; ++c)
        {
            QRectF cell(
                        x,
                        currentY,
                        colWidths[c],
                        headerHeight);

            drawCell(
                        cell,
                        headers[c],
                        boldFont,
                        QColor(204, 204, 204));

            x += colWidths[c];
        }

        currentY += headerHeight;
    };
    qDebug()<<"header part completed";

    int totalRows = m_diodeTestModel->rowCount();
    qDebug()<<"totalRows"<<totalRows;
    int currentRow = 0;

    // Draw table header on the first page
    drawWatermark();
    drawTableHeader(y);

    // Draw the table rows
    while (currentRow < totalRows)
    {
        // Calculate dynamic row height

        qreal rowHeight =30;

        for (int c = 0; c < 9; ++c)
        {
            QString value;


            QStandardItem *item =
                    m_diodeTestModel->item(currentRow, c);

            if (item)
                value = item->text();


            QFont cellFont = normalFont;

            if (c == 7&&
                (value.compare(
                     "Pass", Qt::CaseInsensitive) == 0 ||
                 value.compare(
                     "Pass", Qt::CaseInsensitive) != 0))
            {
                cellFont = boldFont;
            }

            QFontMetricsF metrics(cellFont);

            QRectF textRect = metrics.boundingRect(
                        QRectF(
                            0, 0,
                            qMax(1, static_cast<int>(
                                 colWidths[c] - 8)),
                            10000),
                        Qt::AlignCenter |
                        Qt::TextWordWrap |
                        Qt::TextWrapAnywhere,
                        value);

            rowHeight = qMax<qreal>(
                        rowHeight,
                        textRect.height() + 20.0);
        }

        // Start a new page if the row will not fit
        if (y + rowHeight > bottom)
        {
            // Current page footer
            painter.setFont(boldFont);
            painter.setPen(Qt::black);

            painter.drawText(
                        QRectF(
                            0,
                            pageHeight - 25,
                            pageWidth,
                            15),
                        Qt::AlignCenter,
                        "Page " + QString::number(pageNumber));

            printer.newPage();

            ++pageNumber;

            y = 0;

            // Repeat table header on the new page
            drawWatermark();
            drawTableHeader(y);
        }

        // Draw current row
        qreal x = left;

        for (int c = 0; c < 9; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_diodeTestModel->item(currentRow, c);

            if (item)
                value = item->text();
            QColor background = Qt::transparent;
            QFont cellFont = normalFont;

            // Result column: Pass / Fail
//            if (c == 7)
//            {
//                if (value.compare(
//                            "Pass", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(144, 238, 144);
//                    cellFont = boldFont;
//                }
//                else if (value.compare(
//                                 "Pass", Qt::CaseInsensitive) != 0)
//                {
//                    background = QColor(255, 127, 127);
//                    cellFont = boldFont;
//                }
//            }

            QRectF cell(
                        x,
                        y,
                        colWidths[c],
                        rowHeight);

            drawCell(
                        cell,
                        value,
                        cellFont,
                        background);

            x += colWidths[c];
        }

        y += rowHeight;
        ++currentRow;
    }
    y+=20;
}
void MainWindow::drawTwoWireTestTable(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{

    const qreal left = 0;

    // Table column widths
    const qreal proportions[] = {
        0.055, 0.085, 0.175, 0.062, 0.175,
        0.075, 0.085, 0.108, 0.080, 0.100
    };

    qreal colWidths[10];

    for (int c = 0; c < 10; ++c)
        colWidths[c] = pageWidth * proportions[c];

    // Table headers
    const QStringList headers = {
        "S.No", "Cable", "Src Conn", "Src\nPin",
        "Dstn Conn", "Dstn\nPin",
        "Exp\nValue", "Measured", "Result", "Remarks"
    };
    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.75;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.50);  // 12% opacity
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };

    // Fonts
    QFont normalFont("Times New Roman", 11);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    // Draw a table cell
    auto drawCell = [&](
            const QRectF &rect,
            const QString &value,
            const QFont &font,
            const QColor &background)
    {
        if (background != Qt::transparent)
            painter.fillRect(rect, background);

        painter.setPen(Qt::black);
        painter.drawRect(rect);

        painter.setFont(font);

        QRectF textRect = rect.adjusted(3, 2, -3, -2);

        painter.drawText(
                    textRect,
                    Qt::AlignCenter |
                    Qt::TextWordWrap |
                    Qt::TextWrapAnywhere,
                    value);
    };

    // Draw the table header
    auto drawTableHeader = [&](qreal &currentY)
    {
        const qreal headerHeight = 45;
        qreal x = left;

        for (int c = 0; c < 10; ++c)
        {
            QRectF cell(
                        x,
                        currentY,
                        colWidths[c],
                        headerHeight);

            drawCell(
                        cell,
                        headers[c],
                        boldFont,
                        QColor(204, 204, 204));

            x += colWidths[c];
        }

        currentY += headerHeight;
    };

    int totalRows = m_twoWireTestModel->rowCount();
    int currentRow = 0;

    // Draw table header on the first page
    drawWatermark();
    drawTableHeader(y);


    // Draw the table rows
    while (currentRow < totalRows)
    {
        // Calculate dynamic row height
        qreal rowHeight = 30;

        for (int c = 0; c < 10; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_twoWireTestModel->item(currentRow, c);

            if (item)
                value = item->text();

            QFont cellFont = normalFont;

            if (c == 8 &&
                (value.compare(
                     "Pass", Qt::CaseInsensitive) == 0 ||
                 value.compare(
                     "Fail", Qt::CaseInsensitive) == 0))
            {
                cellFont = boldFont;
            }

            QFontMetricsF metrics(cellFont);

            QRectF textRect = metrics.boundingRect(
                        QRectF(
                            0, 0,
                            qMax(1, static_cast<int>(
                                 colWidths[c] - 8)),
                            10000),
                        Qt::AlignCenter |
                        Qt::TextWordWrap |
                        Qt::TextWrapAnywhere,
                        value);

            rowHeight = qMax<qreal>(
                        rowHeight,
                        textRect.height() + 20);
        }

        // Start a new page if the row will not fit
        if (y + rowHeight > bottom)
        {
            // Current page footer
            painter.setFont(boldFont);
            painter.setPen(Qt::black);

            painter.drawText(
                        QRectF(
                            0,
                            pageHeight - 25,
                            pageWidth,
                            15),
                        Qt::AlignCenter,
                        "Page " + QString::number(pageNumber));

            printer.newPage();

            ++pageNumber;

            y = 0;

            // Repeat table header on the new page
            drawWatermark();
            drawTableHeader(y);

        }

        // Draw current row
        qreal x = left;

        for (int c = 0; c < 10; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_twoWireTestModel->item(currentRow, c);

            if (item)
                value = item->text();

            QColor background = Qt::transparent;
            QFont cellFont = normalFont;

            // Result column: Pass / Fail
//            if (c == 8)
//            {
//                if (value.compare(
//                            "Pass", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(144, 238, 144);
//                    cellFont = boldFont;
//                }
//                else if (value.compare(
//                                 "Fail", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(255, 127, 127);
//                    cellFont = boldFont;
//                }
//            }

            QRectF cell(
                        x,
                        y,
                        colWidths[c],
                        rowHeight);

            drawCell(
                        cell,
                        value,
                        cellFont,
                        background);

            x += colWidths[c];
        }

        y += rowHeight;
        ++currentRow;
    }
    y+=20;
}

void MainWindow::drawTwoWireTestFooter(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{
    Q_UNUSED(printer);

    QString notes = ui->textEdit_notesOutput->toPlainText();

    QFont normalFont("Times New Roman", 11);
    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    qreal left = 20;
    qreal right = 20;
    qreal notesWidth = pageWidth - left - right;

    // Prepare notes with automatic wrapping
    QTextDocument notesDocument;
    notesDocument.setDefaultFont(normalFont);
    notesDocument.setPlainText("Notes: " + notes);

    QTextOption textOption;
    textOption.setWrapMode(
        QTextOption::WrapAtWordBoundaryOrAnywhere);
    notesDocument.setDefaultTextOption(textOption);
    notesDocument.setTextWidth(notesWidth);

    qreal notesHeight = qMax<qreal>(
        25.0, notesDocument.size().height() + 10.0);

    qreal signatureHeight = 60.0;
    qreal requiredHeight = notesHeight + signatureHeight + 35.0;

    if (y + requiredHeight > bottom)
    {
        painter.setFont(boldFont);
        painter.setPen(Qt::black);

        painter.drawText(
            QRectF(0, pageHeight - 25, pageWidth, 15),
            Qt::AlignCenter,
            "Page " + QString::number(pageNumber));

        printer.newPage();
        ++pageNumber;

        y = 40;
    }

    // Draw complete notes text
    painter.save();
    painter.translate(left, y);

    notesDocument.drawContents(
        &painter,
        QRectF(0, 0, notesWidth,
               notesDocument.size().height()));

    painter.restore();

    y += notesHeight + 70;

    painter.drawText(
        QRectF(20, y, (pageWidth-40) / 3, 20),
        Qt::AlignLeft,
        ui->lineEdit_performedBy->text());

    painter.drawText(
        QRectF((pageWidth-40) / 3, y, pageWidth / 3, 20),
        Qt::AlignHCenter,
        "");

    painter.drawText(
        QRectF(2 * (pageWidth-40) / 3, y, pageWidth / 3, 20),
        Qt::AlignRight,
        ui->lineEdit_inspectedBy->text());
    y+=20;


    // Signature headings
    painter.setFont(boldFont);
    painter.setPen(Qt::black);


    painter.drawText(
        QRectF(20, y, (pageWidth-40) / 3, 20),
        Qt::AlignLeft,
        "Test Performed By:");

    painter.drawText(
        QRectF((pageWidth-40) / 3, y, pageWidth / 3, 20),
        Qt::AlignHCenter,
        "Prod. In-Charge");

    painter.drawText(
        QRectF(2 * (pageWidth-40) / 3, y, pageWidth / 3, 20),
        Qt::AlignRight,
        "Inspected By:");

    // Page number on the final page
    painter.drawText(
        QRectF(0, pageHeight - 25, pageWidth, 15),
        Qt::AlignCenter,
        "Page "+ QString::number(pageNumber));
}
void MainWindow::on_pushButton_selfTestSave_clicked()
{
    QString folderPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation)
            + "/SelfTest_Pdf";

    QDir dir;
    if (!dir.exists(folderPath) && !dir.mkpath(folderPath))
    {
        QMessageBox::warning(this, "PDF",
                             "Unable to create PDF folder.");
        return;
    }

    QString fileName = folderPath + "/SelfTest_" +
            QDateTime::currentDateTime().toString(
                "yyyyMMdd_HHmmss") + ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);

    printer.setPageMargins(
                12, 12, 12, 20,
                QPrinter::Millimeter);
    printer.setResolution(96);

    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(this, "PDF",
                             "Unable to create PDF.");
        return;
    }

    QRect pageRect = printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = pageRect.width();
    const qreal pageHeight = pageRect.height();

    const qreal left = 50;
    const qreal right = 50;
    const qreal top = 50;
    const qreal bottomMargin = 55;

    QFont titleFont("Times New Roman", 14, QFont::Bold);
    QFont normalFont("Times New Roman", 11);
    QFont companyFont("Times New Roman", 16, QFont::Bold);
   // QFont pageFont("Times New Roman", 9);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    QStringList values = {
        ui->textEdit_1->toPlainText(),
        ui->textEdit_2->toPlainText(),
        ui->textEdit_3->toPlainText(),
        ui->textEdit_4->toPlainText(),
        ui->textEdit_5->toPlainText(),
        ui->textEdit_6->toPlainText(),
        ui->textEdit_7->toPlainText(),
        ui->textEdit_8->toPlainText(),
        ui->textEdit_9->toPlainText(),
        ui->textEdit_10->toPlainText(),
        ui->textEdit_11->toPlainText(),
        ui->textEdit_12->toPlainText(),
        ui->textEdit_13->toPlainText(),
        ui->textEdit_14->toPlainText(),
        ui->textEdit_15->toPlainText()
    };

    painter.setPen(Qt::black);

    // Header: first page only
    QImage solarLogo(":/new/prefix1/images/solar1.png");
    QImage drdoLogo(":/new/prefix1/images/drdo.png");

    const qreal logoWidth = 105;
    const qreal logoHeight = 75;

    if (!solarLogo.isNull())
    {
        painter.drawImage(
                    QRectF(10, 0, logoWidth, logoHeight),
                    solarLogo);
    }

    if (!drdoLogo.isNull())
    {
        painter.drawImage(
                    QRectF(pageWidth - logoWidth - 10, 0,
                           logoWidth, logoHeight),
                    drdoLogo);
    }

    painter.setFont(companyFont);
    painter.drawText(
                QRectF(logoWidth+5, 50,
                       pageWidth - 2 * logoWidth, 25),
                Qt::AlignCenter,
                "SOLAR DEFENCE AND AEROSPACE LIMITED");

    painter.setFont(titleFont);
    painter.drawText(
                QRectF(0, 85, pageWidth, 30),
                Qt::AlignCenter,
                "GNC KIT CABLE HARNESS TEST REPORT");

    painter.drawText(
                QRectF(left,115,
                       pageWidth - left - right, 30),
                Qt::AlignCenter,
                "SELF TEST REPORT");

    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.75;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.30);  // 12% opacity
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };

    qreal y = 180;

    int pageNumber = 1;

    // Draw page number
    auto drawPageNumber = [&]()
    {
        painter.save();

        painter.setFont(boldFont);
        painter.setPen(Qt::black);

        painter.drawText(
                    QRectF(0, pageHeight - 30,
                           pageWidth, 20),
                    Qt::AlignCenter,
                    "Page " + QString::number(pageNumber));

        painter.restore();
    };

    // Draw four-sided page border
    auto drawPageBorder = [&](bool firstPage)
    {
        painter.save();
        painter.setPen(QPen(Qt::black, 1));

        qreal borderLeft = 12;
        qreal borderRight = pageWidth - 12;
        qreal borderTop = firstPage ? 160- 10 : 12;
        qreal borderBottom = pageHeight - 40;

        painter.drawRect(
                    QRectF(borderLeft, borderTop,
                           borderRight - borderLeft,
                           borderBottom - borderTop));

        painter.restore();
    };

    // Two-column layout
    const qreal columnGap = 25;

    const qreal columnWidth =
            (pageWidth - left - right - columnGap) / 2;

    // 0 = left column, 1 = right column
    int column = 0;

    // Start below the header on the first page
    qreal columnY[2] = {y, y};

    // Available bottom position for text
    const qreal contentBottom = pageHeight - bottomMargin;


     drawWatermark();

    for (const QString &text : values)
    {
        QString displayText = text.trimmed();

        // Remove lines containing "Testing..."
        QStringList lines = displayText.split('\n');
        QStringList filteredLines;

        for (const QString &line : lines)
        {
            if (!line.contains("Testing...", Qt::CaseInsensitive))
                filteredLines.append(line);
        }

        displayText = filteredLines.join('\n').trimmed();

        if (displayText.isEmpty())
            continue;

        QTextDocument document;
        document.setDefaultFont(normalFont);
        document.setPlainText(displayText);
        document.setTextWidth(columnWidth);

        QTextOption textOption;
        textOption.setWrapMode(
            QTextOption::WrapAtWordBoundaryOrAnywhere);
        document.setDefaultTextOption(textOption);

        // Make the first line bold and slightly larger
        QTextCursor cursor(&document);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::EndOfBlock,
                            QTextCursor::KeepAnchor);

        QTextCharFormat firstLineFormat;
        QFont firstLineFont = normalFont;
        firstLineFont.setPointSize(normalFont.pointSize() + 2);
        firstLineFont.setBold(true);
        firstLineFormat.setFont(firstLineFont);

        cursor.mergeCharFormat(firstLineFormat);

        // Calculate height after formatting
        qreal textHeight = document.size().height();
        qreal rowHeight = qMax(30.0, textHeight + 12);

        // Move to the next column/page if required
        if (columnY[column] + rowHeight > contentBottom)
        {
            if (column == 0)
            {
                column = 1;
            }
            else
            {
                drawPageBorder(pageNumber == 1);

                printer.newPage();

                pageNumber++;
                drawPageNumber();
                 drawWatermark();


                column = 0;
                columnY[0] = top;
                columnY[1] = top;
            }
        }

        qreal x = (column == 0)
                  ? left
                  : left + columnWidth + columnGap;

        // Draw the formatted text
        painter.save();
        painter.translate(x, columnY[column]);

        document.drawContents(
            &painter,
            QRectF(0, 0, columnWidth, textHeight));

        painter.restore();

        // Separate each value visually
        painter.setPen(QPen(Qt::lightGray, 0.5));
        painter.drawLine(
            QPointF(x, columnY[column] + rowHeight),
            QPointF(x + columnWidth, columnY[column] + rowHeight));

        columnY[column] += rowHeight + 5;
    }


    // Draw border and page number on the final page
    drawPageBorder(pageNumber == 1);
    drawWatermark();
    //drawPageNumber();
    qDebug()<<"lastly y value is"<<columnY[0];
    const qreal bottom = pageHeight - 25;

    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                columnY[0],
                pageNumber);

    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "Self-test PDF saved successfully:\n" + fileName);
}

void MainWindow::on_pushButton_stopDiodeTest_clicked()
{
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Diode Test",
                "Are you sure you want to abort the Diode test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_backFromDiodeTest_clicked()
{
    if (test->isDiodeTestRunning())
    {
        QMessageBox::warning(
            this,
            "Diode Test",
            "Please wait, Diode test is running.");
        return;
    }

    ui->stackedWidget->setCurrentWidget(ui->page_test);
}

void MainWindow::on_pushButton_saveDiodeTest_clicked()
{
    if (test->isDiodeTestRunning())
    {
        QMessageBox::warning(
            this,
            "Diode Test",
            "Please wait, Diode test is running.");
        return;
    }

    failedDiodeCount = 0;
    generateDiodeTestPDF();
    // Add Save functionality here
}
void MainWindow::generateDiodeTestPDF()
{
    if (!m_diodeTestModel)
        return;

 //    Create PDF folder on Desktop
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/DiodeTest_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Generate filename with timestamp
    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    QString fileName = folderPath +
            "/DiodeTest_" + timestamp + ".pdf";

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    // Printer setup
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOrientation(QPrinter::Portrait);

    printer.setPageMargins(
                15, 15, 15, 20,
                QPrinter::Millimeter);

    printer.setResolution(96);

    // Start painting
    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
                    this,
                    "PDF",
                    "Unable to create PDF.");
        return;
    }

    // Calculate page dimensions
    QRect deviceRect =
            printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = deviceRect.width();
    const qreal pageHeight = deviceRect.height();

    const qreal bottom = pageHeight - 25;

    // Count failed pins from Result column (column 8)
     failedDiodeCount = 0;
    for (int row = 0;
         row < m_diodeTestModel->rowCount();
         ++row)
    {
        QStandardItem *item =
                m_diodeTestModel->item(row, 7);

        if (item &&
            item->text().compare(
                "PASS", Qt::CaseInsensitive) != 0)
        {
            ++failedDiodeCount;
        }
    }

    int pageNumber = 1;

    // 1. Draw report header
    qreal y = drawTwoWireTestHeader(
                painter,
                pageWidth,
                failedDiodeCount);

    // 2. Draw table
    drawDiodeTestTable(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // 3. Draw notes, signatures and final page number
    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // Finish PDF generation
    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "PDF generated successfully.");
}


void MainWindow::on_pushButton_openDiodeTest_clicked()
{
    if (test->isDiodeTestRunning())
    {
        QMessageBox::warning(
            this,
            "Diode Test",
            "Please wait, Diode test is running.");
        return;
    }

    // Add Open Files functionality here
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/DiodeTest_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);
    QDesktopServices::openUrl(
                QUrl::fromLocalFile(folderPath));

}


void MainWindow::on_pushButton_stopRelayTest_clicked()
{
    QMessageBox::StandardButton reply =
            QMessageBox::question(
                this,
                "Abort Relay Test",
                "Are you sure you want to abort the Relay test?",
                QMessageBox::Yes |
                QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        test->abortCommand();
    }
}

void MainWindow::on_pushButton_saveRelayTest_clicked()
{
    if (test->isRelayTestRunning())
    {
        QMessageBox::warning(
            this,
            "Relay Test",
            "Please wait, Relay test is running.");
        return;
    }

    // Add Save functionality here
    failedRelayCount = 0;
    generateRelayTestPDF();

}
void MainWindow::generateRelayTestPDF()
{
//    if (!m_relayTestModel)
//        return;

 //    Create PDF folder on Desktop
    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/RelayTest_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);

    // Generate filename with timestamp
    QString timestamp =
            QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss");

    QString fileName = folderPath +
            "/RelayTest_" + timestamp + ".pdf";

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    // Printer setup
    QPrinter printer(QPrinter::HighResolution);

    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOrientation(QPrinter::Portrait);

    printer.setPageMargins(
                15, 15, 15, 20,
                QPrinter::Millimeter);

    printer.setResolution(96);

    // Start painting
    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
                    this,
                    "PDF",
                    "Unable to create PDF.");
        return;
    }

    // Calculate page dimensions
    QRect deviceRect =
            printer.pageLayout().paintRectPixels(
                printer.resolution());

    const qreal pageWidth = deviceRect.width();
    const qreal pageHeight = deviceRect.height();

    const qreal bottom = pageHeight - 25;

    // Count failed pins from Result column (column 8)
     failedRelayCount = 0;
    for (int row = 0;
         row < m_relayTestModel->rowCount();
         ++row)
    {
        QStandardItem *item =
                m_relayTestModel->item(row, 12);

        if (item &&
            item->text().compare(
                "PASS", Qt::CaseInsensitive) != 0)
        {
            ++failedRelayCount;
        }
    }

    int pageNumber = 1;

    // 1. Draw report header
    qreal y = drawTwoWireTestHeader(
                painter,
                pageWidth,
                failedRelayCount);

    // 2. Draw table
    drawRelayTestTable(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // 3. Draw notes, signatures and final page number
    drawTwoWireTestFooter(
                painter,
                printer,
                pageWidth,
                pageHeight,
                bottom,
                y,
                pageNumber);

    // Finish PDF generation
    painter.end();

    QMessageBox::information(
                this,
                "PDF",
                "PDF generated successfully.");
}
void MainWindow::drawRelayTestTable(
        QPainter &painter,
        QPrinter &printer,
        qreal pageWidth,
        qreal pageHeight,
        qreal bottom,
        qreal &y,
        int &pageNumber)
{
    const qreal left = 0;

    // Table column widths
    const qreal proportions[] = {
        0.055, 0.085,0.0,0.0, 0.185, 0.102, 0.0,0.0,0.075,
        0.092, 0.120, 0.108, 0.098, 0.080
    };

    qreal colWidths[14];

    for (int c = 0; c < 14; ++c)
        colWidths[c] = pageWidth * proportions[c];
    // Table headers

    const QStringList headers = {
        "S.No", "Cable", " "," ","Relay Conn", "Relay\nPin"," ","",
        "Excitation\nVolt", "Exp\nValue",
        "Condition", "Measured", "Results","Remarks"
    };


    QImage watermark(":/new/prefix1/images/charka.png");
    auto drawWatermark = [&]()
    {
        if (watermark.isNull())
            return;

        qreal watermarkWidth = pageWidth * 0.75;
        qreal watermarkHeight =
                watermarkWidth * watermark.height()
                / watermark.width();

        QRectF watermarkRect(
            (pageWidth - watermarkWidth) / 2,
            (pageHeight - watermarkHeight) / 2,
            watermarkWidth,
            watermarkHeight
        );

        painter.save();
        painter.setOpacity(0.50);  // 12% opacity
        painter.drawImage(watermarkRect, watermark);
        painter.restore();
    };

    qDebug()<<"headers set successfully";
    // Fonts
    QFont normalFont("Times New Roman", 11);

    QFont boldFont("Times New Roman", 11);
    boldFont.setBold(true);

    // Draw a table cell
    auto drawCell = [&](
            const QRectF &rect,
            const QString &value,
            const QFont &font,
            const QColor &background)
    {
        if (background != Qt::transparent)
            painter.fillRect(rect, background);

        painter.setPen(Qt::black);
        painter.drawRect(rect);

        painter.setFont(font);

        QRectF textRect = rect.adjusted(3, 2, -3, -2);

        painter.drawText(
                    textRect,
                    Qt::AlignCenter |
                    Qt::TextWordWrap |
                    Qt::TextWrapAnywhere,
                    value);
    };

    // Draw the table header
    auto drawTableHeader = [&](qreal &currentY)
    {
        const qreal headerHeight = 50;
        qreal x = left;

        for (int c = 0; c < 14; ++c)
        {
            if(c==2 ||c==3||c==6||c==7)
            {
                continue;
            }
            QRectF cell(
                        x,
                        currentY,
                        colWidths[c],
                        headerHeight);

            drawCell(
                        cell,
                        headers[c],
                        boldFont,
                        QColor(204, 204, 204));

            x += colWidths[c];
        }

        currentY += headerHeight;
    };


    int totalRows = m_relayTestModel->rowCount();

    int currentRow = 0;

    // Draw table header on the first page
    drawWatermark();
    drawTableHeader(y);

    qDebug()<<"header part completed";
    qDebug()<<"totalRows"<<totalRows;

    // Draw the table rows
    while (currentRow < totalRows)
    {
        // Calculate dynamic row height

        qreal rowHeight = 30;

        for (int c = 0; c < 14; ++c)
        {
            QString value;
            if(c==2 ||c==3||c==6||c==7)
            {
                qDebug()<<"here problem occurs";
                continue;
            }


            QStandardItem *item =
                    m_relayTestModel->item(currentRow, c);

            if (item)
                value = item->text();


            QFont cellFont = normalFont;

            if (c == 12&&
                (value.compare(
                     "Pass", Qt::CaseInsensitive) == 0 ||
                 value.compare(
                     "Pass", Qt::CaseInsensitive) != 0))
            {
                cellFont = boldFont;
            }

            QFontMetricsF metrics(cellFont);

            QRectF textRect = metrics.boundingRect(
                        QRectF(
                            0, 0,
                            qMax(1, static_cast<int>(
                                 colWidths[c] - 8)),
                            10000),
                        Qt::AlignCenter |
                        Qt::TextWordWrap |
                        Qt::TextWrapAnywhere,
                        value);

            rowHeight = qMax<qreal>(
                        rowHeight,
                        textRect.height() + 20);
        }

        // Start a new page if the row will not fit
        if (y + rowHeight > bottom)
        {
            // Current page footer
            painter.setFont(boldFont);
            painter.setPen(Qt::black);

            painter.drawText(
                        QRectF(
                            0,
                            pageHeight - 25,
                            pageWidth,
                            15),
                        Qt::AlignCenter,
                        "Page " + QString::number(pageNumber));

            printer.newPage();

            ++pageNumber;

            y = 0;

            drawWatermark();
            // Repeat table header on the new page
            drawTableHeader(y);
        }

        // Draw current row
        qreal x = left;

        for (int c = 0; c < 14; ++c)
        {
            QString value;

            QStandardItem *item =
                    m_relayTestModel->item(currentRow, c);

            if (item)
                value = item->text();


            QColor background = Qt::transparent;
            QFont cellFont = normalFont;

            // Result column: Pass / Fail
//            if (c == 12)
//            {
//                if (value.compare(
//                            "Pass", Qt::CaseInsensitive) == 0)
//                {
//                    background = QColor(144, 238, 144);
//                    cellFont = boldFont;
//                }
//                else if (value.compare(
//                                 "Pass", Qt::CaseInsensitive) != 0)
//                {
//                    background = QColor(255, 127, 127);
//                    cellFont = boldFont;
//                }
//            }

            QRectF cell(
                        x,
                        y,
                        colWidths[c],
                        rowHeight);

            drawCell(
                        cell,
                        value,
                        cellFont,
                        background);

            x += colWidths[c];
        }

        y += rowHeight;
        ++currentRow;
    }
    y+=20;
}

void MainWindow::on_pushButton_openRelayTest_clicked()
{
    if (test->isRelayTestRunning())
    {
        QMessageBox::warning(
            this,
            "Relay Test",
            "Please wait, Relay test is running.");
        return;
    }


    // Add Open Files functionality here

    QString desktopPath =
            QStandardPaths::writableLocation(
                QStandardPaths::DesktopLocation);

    QString folderPath = desktopPath + "/RelayTest_Pdf";

    QDir dir;

    if (!dir.exists(folderPath))
        dir.mkpath(folderPath);
    QDesktopServices::openUrl(
                QUrl::fromLocalFile(folderPath));
}

void MainWindow::on_pushButton_backFromRelayTest_clicked()
{
    if (test->isRelayTestRunning())
    {
        QMessageBox::warning(
            this,
            "Relay Test",
            "Please wait, Relay test is running.");
        return;
    }

    ui->stackedWidget->setCurrentWidget(ui->page_test);
}

void MainWindow::on_pushButton_openTwoWireFiles_clicked()
{
    QString desktopPath =
               QStandardPaths::writableLocation(
                   QStandardPaths::DesktopLocation);

       QString folderPath = desktopPath + "/TwoWire_Pdf";

       // Create the folder if it doesn't exist
       QDir().mkpath(folderPath);

       // Open the folder in File Explorer
       QDesktopServices::openUrl(
                   QUrl::fromLocalFile(folderPath));
}


void MainWindow::on_pushButton_browseImage_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
            this,
            "Select Image",
            QString(),
            "Images (*.png *.jpg *.jpeg *.bmp)"
        );

        if (!filePath.isEmpty())
        {
            ui->lineEdit_imagePath->setText(filePath);
        }
}


bool MainWindow::saveImageDetailsToDb(
        const QString &cableName,
        const QString &imagePath,
        const QString &notes,
        const QString &testPerformedBy,
        const QString &inspectedBy,
        const QString &projectName)
{
    QString tableName = cableName.trimmed() + "_details";
    tableName.replace(" ", "_");

    // Sanitize the dynamic table name
    for (int i = 0; i < tableName.size(); ++i)
    {
        QChar ch = tableName[i];

        if (!ch.isLetterOrNumber() && ch != '_')
            tableName[i] = '_';
    }

    if (cableName.trimmed().isEmpty())
    {
        QString error = "Failed: Cable name is empty.";
        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    // Read the selected image
    QFile file(imagePath);

    if (imagePath.trimmed().isEmpty() ||
            !file.open(QIODevice::ReadOnly))
    {
        QString error = "Failed to open image: " + imagePath;
        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    QByteArray imageData = file.readAll();
    file.close();

    if (imageData.isEmpty())
    {
        QString error = "Selected image is empty.";
        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    // Create table
    QSqlQuery createQuery(db);

    QString create =
            "CREATE TABLE IF NOT EXISTS \"" + tableName + "\" ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "cable_name TEXT, "
            "image_data BLOB, "
            "notes TEXT, "
            "test_performed_by TEXT, "
            "inspected_by TEXT, "
            "project_name TEXT"
            ")";

    if (!createQuery.exec(create))
    {
        QString error = "Failed to create table " + tableName
                + ": " + createQuery.lastError().text();

        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    // Start transaction
    if (!db.transaction())
    {
        QString error = "Failed to start transaction: "
                + db.lastError().text();

        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    // Clear previous data
    QSqlQuery deleteQuery(db);

    if (!deleteQuery.exec(
                "DELETE FROM \"" + tableName + "\""))
    {
        db.rollback();

        QString error = "Failed to clear table " + tableName
                + ": " + deleteQuery.lastError().text();

        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }
    QSqlQuery checkQuery(db);

    if (checkQuery.exec("PRAGMA table_info(\"" + tableName + "\")"))
    {
        while (checkQuery.next())
        {
            qDebug() << "Column:"
                     << checkQuery.value("name").toString();
        }
    }
    // Insert data
    QSqlQuery insertQuery(db);

    insertQuery.prepare(
                "INSERT INTO \"" + tableName + "\" "
                "(cable_name, image_data, notes, "
                "test_performed_by, inspected_by, project_name) "
                "VALUES (?, ?, ?, ?, ?, ?)");

    insertQuery.addBindValue(cableName);
    insertQuery.addBindValue(imageData);
    insertQuery.addBindValue(notes);
    insertQuery.addBindValue(testPerformedBy);
    insertQuery.addBindValue(inspectedBy);
    insertQuery.addBindValue(projectName);

    if (!insertQuery.exec())
    {
        db.rollback();

        QString error = "Insert failed in " + tableName
                + ": " + insertQuery.lastError().text();

        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    // Commit transaction
    if (!db.commit())
    {
        QString error = "Failed to commit transaction: "
                + db.lastError().text();

        db.rollback();

        writeToNotes(error);
        ui->textEdit_ErrorLog->append(error);
        return false;
    }

    QString message =
            "Image details stored in table: " + tableName;

    writeToNotes(message);
    ui->textEdit_ErrorLog->append(message);

    return true;
}

void MainWindow::on_pushButton_addMetaData_clicked()
{

    if(ui->lineEdit_cableName->text()=="")
    {
        QMessageBox::information(this,"Empty Cable Name",
                                 "Please Enter the cable name");
        return;
    }
    if(ui->textEdit_notes->toPlainText()==""|| ui->lineEdit_imagePath->text()==""
            ||ui->lineEdit_testPerformedInput->text()==""||ui->lineEdit_inspectedByInput->text()==""||
            ui->lineEdit_projectNameInput->text()=="")
    {
        QMessageBox::information(this,"Empty Fields","Fill all the fields");
        return;
    }
    if(ui->lineEdit_cableName->text().trimmed().isEmpty())
    {
        QMessageBox::information(this,"Field Empty","Enter cable name");
        return;
    }
    ui->textEdit_ErrorLog->clear();
    QString tableName =
            ui->lineEdit_cableName->text().trimmed() + "_details";

    tableName.replace(" ", "_");
    QStringList tables = db.tables();

    bool tableExists = false;

    for (const QString &table : tables)
    {
        if (table.compare(tableName, Qt::CaseInsensitive) == 0)
        {
            tableExists = true;
            break;
        }
    }

    if (tableExists)
    {
        QMessageBox::warning(
            this,
            "Table Exists",
            "Cable name already exists.");

        return;
    }


    bool success = saveImageDetailsToDb(
            ui->lineEdit_cableName->text(),
            ui->lineEdit_imagePath->text(),
            ui->textEdit_notes->toPlainText(),
            ui->lineEdit_testPerformedInput->text(),
            ui->lineEdit_inspectedByInput->text(),
            ui->lineEdit_projectNameInput->text()
        );

        if (success)
        {
            ui->comboBox_fileNames->clear();
            ui->comboBox_fileNames->addItems(getCableNames());
            QMessageBox::information(
                this,
                "Success",
                "Data saved successfully."
            );
        }
}


void MainWindow::on_comboBox_files_currentTextChanged(const QString &arg1)
{



    QString table = arg1.trimmed();

    if (table.isEmpty())
        return;

    // Generate the table name
    QString tableName = table + "_details";

    tableName.replace(" ", "_");

//    // Sanitize table name, matching your save function
//    for (int i = 0; i < tableName.size(); ++i)
//    {
//        QChar ch = tableName[i];

//        if (!ch.isLetterOrNumber() && ch != '_')
//            tableName[i] = '_';
//    }

    QSqlQuery q(db);

    // Check whether the table exists
    q.prepare(
                "SELECT name FROM sqlite_master "
                "WHERE type = 'table' AND name = ?"
                );

    q.addBindValue(tableName);

    if (!q.exec())
    {
        writeToNotes("Failed to check table: "
                     + q.lastError().text());
        return;
    }

    if (!q.next())
    {
        writeToNotes("Table does not exist: " + tableName);
        return;
    }

    // Extract the stored information
    QString sql =
            "SELECT project_name, image_data, notes, "
            "test_performed_by, inspected_by "
            "FROM \"" + tableName + "\" "
                                    "ORDER BY id DESC LIMIT 1";

    if (!q.exec(sql))
    {
        writeToNotes("Failed to retrieve data from "
                     + tableName + ": "
                     + q.lastError().text());
        return;
    }

    if (q.next())
    {
        d_projectName = q.value("project_name").toString();
        d_imageData = q.value("image_data").toByteArray();
        d_notes = q.value("notes").toString();
        d_testPerformedBy =
                q.value("test_performed_by").toString();
        d_inspectedBy =
                q.value("inspected_by").toString();
        ui->lineEdit_projectName->setText(d_projectName);
        ui->lineEdit_inspectedBy->setText(d_inspectedBy);
        ui->lineEdit_performedBy->setText(d_testPerformedBy);
        ui->textEdit_notesOutput->setPlainText(d_notes);
        QPixmap pixmap;

        if (pixmap.loadFromData(d_imageData))
        {
            ui->label_image->setPixmap(
                pixmap.scaled(
                    ui->label_image->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                )
            );
        }
        else
        {
            ui->label_image->clear();
            writeToNotes("Failed to load image from database.");
        }

        writeToNotes("Data loaded from table: " + tableName);
    }
    else
    {
        writeToNotes("Table exists, but contains no records: "
                     + tableName);
    }
}

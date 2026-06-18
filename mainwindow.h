#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLineEdit>
#include <QTableWidgetItem>
#include <QFileDialog>

#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
     bool initializeDatabase();
     void writeToNotes(const QString &message);
     void resetLogFile();
     void initializeLogFile();
     void closeLogFile();

     bool validateHarnessData(const QVector<QString> &cableTemp,
                                          const QVector<QString> &sourceConTemp,
                                          const QVector<QString> &sourcePinTemp,
                                          const QVector<QString> &destConTemp,
                                          const QVector<QString> &destPinTemp,
                                          const QVector<QString> &expTemp,
                                          const QVector<QString> &voltTemp);
     
     bool saveHarnessToDb(const QString &cableName,
                                      const QVector<QString> &cableTemp,
                                      const QVector<QString> &sourceConTemp,
                                      const QVector<QString> &sourcePinTemp,
                                      const QVector<QString> &destConTemp,
                                      const QVector<QString> &destPinTemp,
                                      const QVector<QString> &expTemp,
                                      const QVector<QString> &voltTemp);
     bool savePatchToDb(const QString &cableName);
     QStringList getCableNames();

     bool validatePatchLine(int lineNumber,const QString &pCon,const QString &pPinStr);
private slots:
     void on_pushButton_Save_clicked();

     void on_pushButton_add_clicked();

     void on_pushButton_login_clicked();

     //void on_pushButton_forgetPassword_clicked();



     void on_pushButton_forward_clicked();

     void on_pushButton_forward_2_clicked();

     void on_pushButton_forgetPassword_clicked();

     void on_pushButton_userForgPass_clicked();

     void on_pushButton_adminForgPass_clicked();

     //void on_pushButton_valAdminForUserPass_clicked();

     void on_pushButton_saveUser_clicked();

     void on_pushButton_addUser_clicked();

     void on_pushButton_addAdmin_clicked();

     void on_pushButton_validateMasterKey_clicked();

     void on_pushButton_backToLogin_clicked();

     void on_pushButton_valAdminForUserPass_clicked();

     void on_pushButton_update_clicked();

     void on_pushButton_home_clicked();

     void on_pushButton_recAdminPass_clicked();

     void on_pushButton_3_clicked();

     void on_pushButton_gotoRoleSelect_clicked();

     void on_pushButton_userPasswordPageBack_clicked();

     void on_pushButton_backToLogin_2_clicked();

     void on_pushButton_logOut_clicked();

     void on_pushButton_deleteUsers_clicked();

     void loadUsers();

     void on_pushButton_deleteSelected_clicked();

     void on_pushButton_delTomain_clicked();

     void on_pushButton_dataEntry_clicked();

     void on_pushButton_uploadPatch_clicked();

     void on_pushButton_uploadCable_clicked();

     void openCsvFile(const QString &fileType);

     bool processCsvFile(const QString &fileName,const QString &cableNameFromUI,const QString &type);

     QString convertExcelToCsv(const QString &xlsxFile);


     void on_pushButton_backToModes_clicked();

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    static QFile logFile;
    static QTextStream logStream;
    QString role;
    QString currentAdmin;

    QStringList cableList;
    QStringList userConList;
    QStringList userPinList;
    QStringList patchConList;
    QList<int> patchPinList;

    QString m_setNo;
    QString m_performedBy;
    QString m_inspectedBy;
    QString m_projectName;
    QString m_testType;

};

#endif // MAINWINDOW_H

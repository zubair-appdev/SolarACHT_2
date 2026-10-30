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
#include <QSqlTableModel>
#include <QTextCodec>
#include <QStandardItemModel>

#include <QCryptographicHash>

#include <QStandardPaths>

#include <QPrinter>
#include <QTextDocument>
#include <QFileDialog>
#include <QDate>
#include <QTime>
#include <QDir>
#include <QUrl>
#include <QPageSize>

#include <QImage>
#include <QBuffer>

#include <QDesktopServices>
#include <QEventLoop>
#include <QTimer>
#include <QApplication>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TestController;
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
                                          const QVector<QString> &expTemp
                                          );
     
     bool saveHarnessToDb(const QString &cableName,
                                      const QVector<QString> &cableTemp,
                                      const QVector<QString> &sourceConTemp,
                                      const QVector<QString> &sourcePinTemp,
                                      const QVector<QString> &destConTemp,
                                      const QVector<QString> &destPinTemp,
                                      const QVector<QString> &expTemp
                                      );
     bool savePatchToDb(const QString &cableName);
     QStringList getCableNames();

     bool validatePatchLine(int lineNumber,const QString &pCon,const QString &pPinStr);

     bool deleteCable(const QString &cableName);

     QVector<QByteArray> constructUARTPacketsForTwoWire(
         const QVector<QString> &sourceCon,
         const QVector<QString> &sourcePin,
         const QVector<QString> &destCon,
         const QVector<QString> &destPin);

     QByteArray constructTwoWireStartPacket(quint16 totalPackets);

     struct HarnessConnection
     {
         QString cable;
         QString sourceCon;
         QString sourcePin;
         QString destCon;
         QString destPin;
         QString exp;
     };

     void populateTwoWireTestTable(const QVariantList &harness);

     //Insu/Iso test Table Things
     struct InsulationTableNet
     {
         int loomNo;
         int netNo;

         QVector<QPair<QString, int>> endpoints;
     };

     void populateInsulationTable(
         const QVector<InsulationTableNet> &nets,
         const QVariantList &patchData);


     QString getActualConnectorName(
         const QVariantList &patchData,
         const QString &hardwareCon,
         int hardwarePin);

     //Diode Test
     bool validateDiodeData(
         const QVector<QString> &cableTemp,
         const QVector<QString> &anodeConTemp,
         const QVector<QString> &anodePinTemp,
         const QVector<QString> &cathodeConTemp,
         const QVector<QString> &cathodePinTemp,
         const QVector<QString> &expVoltageTemp);

     bool saveDiodeToDb(
         const QString &cableName,
         const QVector<QString> &cableTemp,
         const QVector<QString> &anodeConTemp,
         const QVector<QString> &anodePinTemp,
         const QVector<QString> &cathodeConTemp,
         const QVector<QString> &cathodePinTemp,
         const QVector<QString> &expVoltageTemp);

     bool validateRelayData(
         const QVector<QString> &cableTemp,
         const QVector<QString> &sourceConTemp,
         const QVector<QString> &sourcePinTemp,
         const QVector<QString> &relaySideConTemp,
         const QVector<QString> &relaySideRSPinsTemp,
         const QVector<QString> &destConTemp,
         const QVector<QString> &destPinTemp,
         const QVector<QString> &excitationVoltageTemp,
         const QVector<QString> &expTemp);


     bool saveRelayToDb(
         const QString &cableName,
         const QVector<QString> &cableTemp,
         const QVector<QString> &sourceConTemp,
         const QVector<QString> &sourcePinTemp,
         const QVector<QString> &relaySideConTemp,
         const QVector<QString> &relaySideRSPinsTemp,
         const QVector<QString> &destConTemp,
         const QVector<QString> &destPinTemp,
         const QVector<QString> &excitationVoltageTemp,
         const QVector<QString> &expTemp);


     void loadRelayTable(const QString &tableName);

     void loadDiodeTable(const QString &tableName);

     QVector<QByteArray> constructUARTPacketsForDiode(
         const QVector<QString> &sourceCon,
         const QVector<QString> &sourcePin,
         const QVector<QString> &destCon,
         const QVector<QString> &destPin,
         const QVector<QString> &expectedVoltage);

     QByteArray constructDiodeStartPacket(quint16 totalPackets);

     void populateDiodeTestTable(const QVariantList &diode);

     //Relay Test
     QVector<QByteArray> constructUARTPacketsForRelay(
         const QVector<QString>& sourceCon,
         const QVector<QString>& sourcePin,
         const QVector<QString>& relaySideCon,
         const QVector<QString>& relaySideRSPins,
         const QVector<QString>& destCon,
         const QVector<QString>& destPin,
         const QVector<QString>& excitationVoltage,
         const QVector<QString>& expected);

     QByteArray constructRelayStartPacket(
         quint8 totalRelays,
         quint16 totalPackets);

     void populateRelayTestTable(const QVariantList &relayData);

     // For Flexible Get Details TableViews
     void setupSqlTableView(QTableView *tableView,
                            QSqlTableModel *model);

     bool refreshPatchValidationLists();

     bool savePatchDisplayChanges();

     bool convertLogicalPatchToDatabase(
         const QString &logicalCon,
         const QString &logicalPin,
         QString &dbCon,
         int &dbPin);

     bool exportTableToExcel(QTableView *tableView,
                             const QString &fileSuffix);

     bool importTableFromExcel(
         QTableView *tableView,
         const QString &tableSuffix,
         const QStringList &columns,
         bool isPatchTable = false);

     inline void pauseFor(int milliseconds) {
         QEventLoop loop;
         QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);  // After delay, quit the event loop
         loop.exec();  // Start the event loop and wait for it to quit
         QApplication::processEvents();  // Keep UI healthy
     }


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

     void on_pushButton_getDetails_clicked();

     void loadPatchTable(const QString &tableName);

     void loadHarnessTable(const QString &tableName);

     void on_pushButton_editTableView_clicked();

     void on_pushButton_saveChanges_clicked();

     void on_pushButton_revertChanges_clicked();

     void on_pushButton_patchEdit_clicked();

     void on_pushButton_patchSave_clicked();

     void on_pushButton_patchCancel_clicked();

     void on_pushButton_delCable_clicked();

     void on_pushButton_test_clicked();

      void onPortSelected(const QString &portName);

      void on_pushButton_run_clicked();

      QVariantList getPatchData(const QString &cableName);

      QVariantList getHarnessData(const QString &cableName);

      QVariantList getDiodeData(const QString &cableName);

      QVariantList getRelayData(const QString &cableName);

      void on_pushButton_back_clicked();

      void on_pushButton_addRow_clicked();

      void on_pushButton_delRow_clicked();

      void on_pushButton_addHarnessRow_clicked();

      void on_pushButton_delHarnessRow_clicked();

      void on_pushButton_backFromTwoWireTest_clicked();

      void on_pushButton_selfTest_clicked();

      void on_pushButton_selfBack_clicked();

      void on_pushButton_selfRun_clicked();

      void on_pushButton_selfAbort_clicked();

      void on_pushButton_stopTwoWireTest_clicked();

      void on_pushButton_calibrationTest_clicked();

      void on_pushButton_calibrationRun_clicked();

      void on_pushButton_calibrationBack_clicked();

      void on_pushButton_calibrationAbort_clicked();

      void on_pushButton_calibrationSave_clicked();

      void on_pushButton_calibrationView_clicked();

      void on_pushButton_setColumnCalibrate_clicked();

      void on_pushButton_saveTwoWirePdf_clicked();

      void generateTwoWireTestPDF();

      //QString imageToBase64(const QString &filePath);

      void refreshPorts();


      qreal drawTwoWireTestHeader(QPainter &painter,
                                  qreal pageWidth,
                                  int failedPinsCount);

      void drawTwoWireTestTable(QPainter &painter,
                                QPrinter &printer,
                                qreal pageWidth,
                                qreal pageHeight,
                                qreal bottom,
                                qreal &y,
                                int &pageNumber);

      void drawTwoWireTestFooter(QPainter &painter,
                                 QPrinter &printer,
                                 qreal pageWidth,
                                 qreal pageHeight,
                                 qreal bottom,
                                 qreal &y,
                                 int &pageNumber);


      void on_pushButton_editCalibration_clicked();

//      void on_pushButton_saveTwoWirePdf_clicked();

      void on_pushButton_backFromInsuIso_clicked();

      void on_pushButton_stopInsuIso_clicked();

      void on_pushButton_saveInsuIso_clicked();

      void on_pushButton_openInsuIsoFiles_clicked();

      void drawInsoIsoTable( QPainter &painter,
                             QPrinter &printer,
                             qreal pageWidth,
                             qreal pageHeight,
                             qreal bottom,
                             qreal &y,
                             int &pageNumber);

      void generateInsoIsoPDF();
      void generateCalibratePDF();

      void drawCalibrateTable(
              QPainter &painter,
              QPrinter &printer,
              qreal pageWidth,
              qreal pageHeight,
              qreal bottom,
              qreal &y,
              int &pageNumber);

      void on_pushButton_selfTestSave_clicked();

      void on_pushButton_stopDiodeTest_clicked();
      void on_pushButton_backFromDiodeTest_clicked();
      void on_pushButton_saveDiodeTest_clicked();
      void on_pushButton_openDiodeTest_clicked();

      void on_pushButton_stopRelayTest_clicked();
      void on_pushButton_saveRelayTest_clicked();
      void on_pushButton_openRelayTest_clicked();
      void on_pushButton_backFromRelayTest_clicked();


      void  generateDiodeTestPDF();

      void drawDiodeTestTable(
              QPainter &painter,
              QPrinter &printer,
              qreal pageWidth,
              qreal pageHeight,
              qreal bottom,
              qreal &y,
              int &pageNumber);

       void generateRelayTestPDF();
       void drawRelayTestTable(
               QPainter &painter,
               QPrinter &printer,
               qreal pageWidth,
               qreal pageHeight,
               qreal bottom,
               qreal &y,
               int &pageNumber);


       void on_pushButton_openTwoWireFiles_clicked();

       void on_pushButton_browseImage_clicked();

       bool saveImageDetailsToDb(
               const QString &cableName,
               const QString &imagePath,
               const QString &notes,
               const QString &testPerformedBy,
               const QString &inspectedBy,
               const QString &projectName);
       void on_pushButton_addMetaData_clicked();



       void on_comboBox_files_currentTextChanged(const QString &arg1);
       void on_pushButton_backFromGetDetails_clicked();

       void on_pushButton_addDiodeRow_clicked();

       void on_pushButton_delDiodeRow_clicked();

       void on_pushButton_addRelayRow_clicked();

       void on_pushButton_delRelayRow_clicked();

       void on_pushButton_editTableViewDiode_clicked();

       void on_pushButton_saveChangesDiode_clicked();

       void on_pushButton_revertChangesDiode_clicked();

       void on_pushButton_editTableViewRelay_clicked();

       void on_pushButton_saveChangesRelay_clicked();

       void on_pushButton_revertChangesRelay_clicked();

       void on_pushButton_clearErrorLog_clicked();

       void on_pushButton_addPatchRow_clicked();

       void on_pushButton_delPatchRow_clicked();

       void on_pushButton_exportPatch_clicked();

       void on_pushButton_exportHarness_clicked();

       void on_pushButton_exportDiode_clicked();

       void on_pushButton_exportRelay_clicked();

       void on_pushButton_importPatch_clicked();

       void on_pushButton_importHarness_clicked();

       void on_pushButton_importDiode_clicked();

       void on_pushButton_impotRelay_clicked();

       void on_checkBox_adjustRelayColumns_clicked(bool checked);

       void on_checkBox_adjustDiodeColumns_clicked(bool checked);

       void on_checkBox_adjustInsuIsoColumns_clicked(bool checked);

       void on_checkBox_adjustTwoWireColumns_clicked(bool checked);

       void on_checkBox_editRemarksTwoWire_clicked(bool checked);

       void on_checkBox_editRemarksIsoInsu_clicked(bool checked);

       void on_checkBox_editRemarksDiode_clicked(bool checked);

       void on_checkBox_editRemarksRelay_clicked(bool checked);

public slots:

      void portStatus(const QString &data);

      void processTwoWireResults();

      bool checkTwoWireExpected(
              float measuredOhms,
              const QString &expected);


      QString formatTwoWireExpected(
              const QString &expected);

      //Self Test result slots
      void onSelfTestSlotResult(
              int slotNumber,
              const QByteArray &slotResult);

      void onSelfTestResultsCompleted(
              const QByteArray &allResults);

      void onSelfTestSlotStarted(int slotNumber);

      //Calibration Test result slots
      void onCalibrationSlotReceived(
              int slotNumber,
              const QByteArray &slotData);

      void onCalibrationSlotMissing(
              int slotNumber);

      void onCalibrationCompleted();

      float calibrationBytesToFloat(
          const QByteArray &data,
          int offset);

      //Insulation/Isolation test slots
      void onInsulationLoomPassed(
          int loomNo);

      void onInsulationLoomFailed(
          int loomNo,
          const QByteArray &loomData);

       void onInsulationResultsCompleted();

       void onInsulationNetMarkerReceived(int loomNo, int totalNets);

       // Relay Test
       void onRelayResultReceived(int row, float measuredValue);
       void onRelayResultsCompleted();

       bool checkRelayExpected(
              float measuredOhms,
              const QString &expected);

       void onRelayNackReceived(quint8 relayId, int resultIndex);






private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    static QFile logFile;
    static QTextStream logStream;
    QString role;
    QString currentAdmin;

    bool adminPermissionFlag = false;


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

    QSqlTableModel *patchModel;
    QSqlTableModel *harnessModel;
    QSqlTableModel *relayModel;
    QSqlTableModel *diodeModel;

    TestController *test;
    bool simulate;

    QStandardItemModel *m_twoWireTestModel = nullptr;
    QStandardItemModel *model =nullptr;

    int expectedResults = 0;

    //Self test variables
    QVector<int> m_selfTestFailedPins[15];
    bool m_selfTestBoardMissing[15] = {false};

    //Calibration Test
    QStandardItemModel *m_calibrationModel = nullptr;
    int failedPinsCount = 0;
    int failedPinsInso= 0;
    int failedPinsCalibrate=0;
    bool testCalibration=false;




    //Insu/Iso Test Progress bar
    double currentPercentage = 0.0;
    int totalInsulationLooms = 0;

    double currentLoomNetProgress = 0.0;

    //Diode Test table filling
    QStandardItemModel *m_diodeTestModel = nullptr;

    //Relay Test table filling
    QStandardItemModel *m_relayTestModel = nullptr;
    QVector<float> m_relayMeasuredValues;

    int failedDiodeCount = 0;
    int failedRelayCount = 0;

    QByteArray d_imageData;
    QString d_projectName;
    QString d_notes;
    QString d_testPerformedBy;
    QString d_inspectedBy;

};

#endif // MAINWINDOW_H

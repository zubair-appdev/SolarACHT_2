#ifndef LOGINCLASS_H
#define LOGINCLASS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QUrl>
#include <QTextStream>
#include <QRegularExpression>


class LoginClass : public QObject
{
    Q_OBJECT
public:
    explicit LoginClass(QObject *parent = nullptr);
    ~LoginClass();

    Q_INVOKABLE bool saveUser(const QString &username, const QString &password, const QString &role);

    //For Saving log Data
    Q_INVOKABLE void writeToNotes(const QString &message);
    void resetLogFile();
    void initializeLogFile();
    void closeLogFile();

    Q_INVOKABLE bool validateAdmin(const QString &username, const QString &password);

    Q_INVOKABLE bool updatePassword(const QString &username,
                                    const QString &newPassword,
                                    const QString &role);

    Q_INVOKABLE QString loginUser(const QString &username, const QString &password);

    Q_INVOKABLE bool deleteUser(const QString &username)
    {
        if (!db.isOpen())
            return false;

        QSqlQuery q(db);
        q.prepare("DELETE FROM loginData WHERE username = :u");
        q.bindValue(":u", username);

        if (!q.exec()) {
            qWarning() << "Delete user failed:" << q.lastError();
            return false;
        }

        return q.numRowsAffected() > 0;
    }



    Q_INVOKABLE QVariantList getAllUsers()
    {
        QVariantList list;

        if (!db.isOpen())
            return list;

        QSqlQuery q("SELECT username, role FROM loginData", db);

        while (q.next()) {
            QVariantMap m;
            m["username"] = q.value("username").toString();
            m["role"] = q.value("role").toString();
            list.append(m);
        }

        return list;
    }

    Q_INVOKABLE bool processCsvFile(const QString &fileUrl, const QString &cableName, const QString &type);

    bool validatePatchLine(int lineNumber,const QString &pCon,const QString &pPinStr);

    bool validateHarnessData(   const QVector<QString> &cableTemp,
                                const QVector<QString> &sourceConTemp,
                                const QVector<QString> &sourcePinTemp,
                                const QVector<QString> &destConTemp,
                                const QVector<QString> &destPinTemp,
                                const QVector<QString> &expTemp,
                                const QVector<QString> &voltTemp    );



    bool savePatchToDb(const QString &cableName);
    bool saveHarnessToDb(const QString &cableName,
                         const QVector<QString> &cableTemp,
                         const QVector<QString> &sourceConTemp,
                         const QVector<QString> &sourcePinTemp,
                         const QVector<QString> &destConTemp,
                         const QVector<QString> &destPinTemp,
                         const QVector<QString> &expTemp,
                         const QVector<QString> &voltTemp);

    Q_INVOKABLE QStringList getCableNames();

    Q_INVOKABLE QVariantList getPatchData(const QString &cableName);
    Q_INVOKABLE QVariantList getHarnessData(const QString &cableName);

    Q_INVOKABLE bool deleteCable(const QString &cableName);

    Q_INVOKABLE void prepareTestData(
        const QString &cableName,
        const QString &setNo,
        const QString &performedBy,
        const QString &inspectedBy,
        const QString &projectName,
        const QString &notes,
        const QString &testToRun,
        const QVariantList &patchData,
        const QVariantList &harnessData);

    // 🔹 getters
    QString getSetNo() const { return m_setNo; }
    QString getPerformedBy() const { return m_performedBy; }
    QString getInspectedBy() const { return m_inspectedBy; }
    QString getProjectName() const { return m_projectName; }
    QString getTestType() const { return m_testType; }



private:
    bool initializeDatabase();
    QSqlDatabase db;

    QFile logFile;
    QTextStream logStream;

    // Patch data : currently these lists are used for saving + validating (processCsv + validate func()) from excel/csv to database
    // every fresh patch update clears them
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

#endif // LOGINCLASS_H

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QTimer>

class CloudDeckManager : public QObject
{
    Q_OBJECT

public:
    explicit CloudDeckManager(QObject *parent = nullptr);
    ~CloudDeckManager();

    Q_INVOKABLE void loginWithCredentials(const QString &email, const QString &password);
    Q_INVOKABLE bool hasStoredCredentials();
    Q_INVOKABLE void clearStoredCredentials();
    Q_INVOKABLE void enterPinAndPair(const QString &pin);

signals:
    void loginCompleted(bool success, const QString &error);
    void machineInfoReady(const QString &status, const QString &password, const QString &sessionDuration);
    void serverAddressReady(const QString &serverAddress);
    void pairingPinNeeded(const QString &pin);
    void pairingCompleted(bool success, const QString &error);

private slots:
    void onPageLoadFinished(bool ok);
    void onLoginFormFilled();

private:
    void fillLoginForm(const QString &email, const QString &password);
    void submitLoginForm();
    void extractPageContent();
    void initializeWebEngine();
    void checkPageAfterSubmission();
    void parseDashboard();
    void clickShowPassword();
    void clickConnectButton();
    void printMachineInfo();
    void extractServerAddress();

    QWebEngineProfile *m_webProfile;
    QWebEnginePage *m_webPage;
    QTimer *m_timeoutTimer;
    QTimer *m_pollTimer;
    bool m_formSubmitted;
    QString m_email;
    QString m_password;
    bool m_loginInProgress;
    bool m_webEngineInitialized;
    
    // Parsed machine info
    QString m_machineStatus;
    QString m_userPassword;
    QString m_sessionDuration;
    QString m_serverAddress;
    int m_parseStep;  // 0=parse status, 1=click show password, 2=get password, 3=done
};

#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QTimer>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include "MarketHub.h"
#include "adaptormodel.h"

namespace Ui {
class Dialog;
}

using namespace MarketHub;
using std::vector;

#define CONFIG_FILE         "MarketHub.ini"
#define APP_ENCRYPT_KEY     "^5tY&MiLK>"
////////////////////////////////////////////////////////////////////////////////
class Mailer
{
public:
    Mailer();
    bool readConfig(const char* file);
    bool sendMail(const std::string& line);
    void stop();

private:
    void checkMail();
    bool sendMailOut(const char* body);

private:
    bool m_enable;
    std::string m_sender;
    std::string m_from;
    std::string m_to;
    std::string m_passwd;
    std::string m_smtpSrv;
    int m_smtpPort;
    std::string m_computerName;
    std::string m_computerUser;

    std::queue<std::string> m_mailQueue;
    std::mutex m_queueMutex;
    std::thread* m_mailerThread;
    int m_checkIntervalCount;
    bool m_mailerStop;

    typedef struct  {
        int start;
        int end;
    } SilentTime;

    std::vector<SilentTime> m_silentTimeList;
};

////////////////////////////////////////////////////////////////////////////////
class Dialog : public QDialog, public Subscriber
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();
    void reject();
    void onMessage(const Message* msg);

public slots:
    void onInit();
    void updateUI(Message msg);
    void timeElapsed();

signals:
    void msgReceived(Message msg);
    
private:
    AdaptorModel* m_adaptorModel;

    void readRestartTime();
    void checkRestartTask();
    void appendLogText(const char* text);

private:
    Ui::Dialog *ui;
    bool m_logTextScrollVisible;
    QTimer* m_timer;
    Mailer m_mailer;

    vector<long>  m_restartTime;
    vector<long>  m_stopTime;
    unsigned long m_schedLastSec;
    unsigned long m_schedCurrSec;
    unsigned long m_restartSecs;

    Controller* m_controller;
};

#endif // DIALOG_H

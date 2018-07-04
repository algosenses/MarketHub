#include <windows.h>
#include <QScrollBar>
#include <QTextBlock>
#include <QMessageBox>
#include "dialog.h"
#include "ui_dialog.h"
#include "RestartAPI.h"
#include "config.h"
#include "quickmail.h"
#include "TinyCrypt.h"

////////////////////////////////////////////////////////////////////////////////
Mailer::Mailer()
{
    m_enable = false;

    m_mailerThread = nullptr;
    m_mailerStop = false;
    m_mailerThread = new std::thread(std::bind(&Mailer::checkMail, this));
}

bool Mailer::readConfig(const char* file)
{
    rude::Config config;
    if (config.load(file)) {
        config.setSection("Mail");
        m_enable = config.getBoolValue("Enable");
        if (!m_enable) {
            return false;
        }

        const char* from = config.getStringValue("Sender");
        if (from && from[0] != '\0') {
            m_from = from;
        } else {
            m_enable = false;
            return false;
        }

        const char* to = config.getStringValue("Receiver");
        if (to && to[0] != '\0') {
            m_to = to;
        } else {
            m_enable = false;
            return false;
        }

        const char* passwd = config.getStringValue("Password");
        if (passwd && passwd[0] != '\0') {
            const unsigned char k[] = APP_ENCRYPT_KEY;
            std::string key((const char*)k);
            TinyCrypt::CryptObject cryptObj((unsigned char*)key.c_str());
            std::string pwd = cryptObj.decrypt(passwd);
            m_passwd = pwd;
        } else {
            m_enable = false;
            return false;
        }

        const char* smtpSrv = config.getStringValue("Server");
        if (smtpSrv && smtpSrv[0] != '\0') {
            m_smtpSrv = smtpSrv;
        } else {
            m_enable = false;
            return false;
        }

        m_smtpPort = config.getIntValue("Port");

        const char* silent = config.getStringValue("SilentTime");
        if (silent && silent[0] != '\0') {
            char* token = NULL;
            char line[1024] = { 0 };
            strcpy(line, silent);
            token = strtok(line, ";,");
            while (token) {
                char str[64];
                char start[16];
                char end[16];
                strcpy(str, token);
                char* p = strchr(str, '-');
                if (p) {
                    int starttime = -1;
                    int endtime = -1;
                    *p = '\0'; p++;

                    strcpy(start, str);
                    if (strlen(start) == strlen("00:00:00")) {
                        start[2] = '\0';
                        start[5] = '\0';
                        starttime = atoi(start) * 3600 + atoi(start + 3) * 60 + atoi(start + 6);
                    }
                    strcpy(end, p);
                    if (strlen(end) == strlen("00:00:00")) {
                        end[2] = '\0';
                        end[5] = '\0';
                        endtime = atoi(end) * 3600 + atoi(end + 3) * 60 + atoi(end + 6);
                    }

                    if (starttime >= 0 && endtime >= 0) {
                        SilentTime time = { starttime, endtime };
                        m_silentTimeList.push_back(time);
                    }
                }

                token = strtok(NULL, ";,");
            }
        }

        char* computerName = NULL;
        char* computerUser = NULL;
        computerName = getenv("COMPUTERNAME");
        if (computerName) {
            m_computerName = computerName;
        }
        computerUser = getenv("USERNAME");
        if (computerUser) {
            m_computerUser = computerUser;
        }
        m_sender = std::string("MarketHub@") + computerName;
    }

    return true;
}

bool Mailer::sendMailOut(const char* body)
{
    if (!m_enable) {
        return false;
    }

    std::string content;

    char header[1024];
    sprintf(header, "Machine: %s\r\n", m_computerName.empty() ? "N/A" : m_computerName.c_str());
    content += header;

    sprintf(header, "User: %s\r\n", m_computerUser.empty() ? "N/A" : m_computerUser.c_str());
    content += header;

    SYSTEMTIME lt;
    GetLocalTime(&lt);
    char datetime[64];
    sprintf(datetime, "%04d/%02d/%02d %02d:%02d:%02d.%03d", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
    sprintf(header, "DateTime: %s\r\n\r\n", datetime);
    content += header;

    content += body;

    std::string subject;
    subject = "ALARM at ";
    subject += datetime;

    int ret = -1;
    quickmail mailobj;
    ret = quickmail_initialize();
    mailobj = quickmail_create(NULL, NULL);
    quickmail_set_from(mailobj, m_from.c_str());
    quickmail_set_sender(mailobj, m_sender.c_str());
    quickmail_add_to(mailobj, m_to.c_str());
    quickmail_set_subject(mailobj, subject.c_str());
    quickmail_add_body_memory(mailobj, NULL, (char*)content.c_str(), content.size(), 0);
    const char* msg = quickmail_send_secure(mailobj, m_smtpSrv.c_str(), m_smtpPort, m_from.c_str(), m_passwd.c_str());
    quickmail_destroy(mailobj);
    ret = quickmail_cleanup();

    return ret == 0;
}

bool Mailer::sendMail(const std::string& line)
{
    if (!m_enable) {
        return false;
    }

    if (line.size() == 0) {
        return false;
    }

    SYSTEMTIME lt;
    GetLocalTime(&lt);
    int second = 3600 * lt.wHour + 60 * lt.wMinute + lt.wSecond;
    for (auto& time : m_silentTimeList) {
        if (second >= time.start && second <= time.end) {
            return false;
        }
    }

    m_queueMutex.lock();
    m_mailQueue.push(line);
    m_queueMutex.unlock();

    return true;
}

void Mailer::checkMail()
{
    m_checkIntervalCount = 0;
    while (!m_mailerStop) {
        if (m_checkIntervalCount >= 10) {
            m_checkIntervalCount = 0;

            std::string content;
            m_queueMutex.lock();
            if (!m_mailQueue.empty()) {
                content += m_mailQueue.front();
                m_mailQueue.pop();
            }
            m_queueMutex.unlock();

            if (!content.empty()) {
                sendMailOut(content.c_str());
            }
        }

        m_checkIntervalCount++;

        Sleep(500);
    }
}

void Mailer::stop()
{
    m_mailerStop = true;
    m_mailerThread->join();
}

////////////////////////////////////////////////////////////////////////////////
Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);

    m_timer = new QTimer(this);

    QObject::connect(this, SIGNAL(msgReceived(Message)),
        this, SLOT(updateUI(Message)));

    QObject::connect(m_timer, SIGNAL(timeout()), this, SLOT(timeElapsed()));

    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMinimizeButtonHint);

    /* With queued connections, Qt must store a copy of the arguments that were passed to 
     * the signal so that it can pass them to the slot later on. Qt knows how to take of copy 
     * of many C++ and Qt types, but "Message" isn't one of them. We must therefore call 
     * the template function qRegisterMetaType() before we can use "Message" as parameter 
     * in queued connections.
     */
    /* When using signals and slots across threads, we must register argument's type of slots */
    /* For example, DataHub.dll callback function emits signal, this signal connect to Quote model's
     * slot which in Qt's main thread, in this case signal-slot connection types is 'Queued Connection',
     * so we need to register QVector<int> which is one argument type of signal dataChanged().
     */
    /* Reference: http://doc.qt.io/qt-5/threads-qobject.html#signals-and-slots-across-threads */
    qRegisterMetaType<Message>("Message");

    m_adaptorModel = new AdaptorModel(this);
    ui->adaptorList->setModel(m_adaptorModel);
    ui->adaptorList->setSelectionMode(QTableView::NoSelection);
    ui->adaptorList->setFocusPolicy(Qt::NoFocus);

    m_logTextScrollVisible = false;

    m_schedLastSec = 0;
    m_schedCurrSec = 0;
    m_restartSecs = 0;

    readRestartTime();

    QTimer::singleShot(300, this, SLOT(onInit()));
}

void Dialog::onInit()
{
    std::vector<std::string> adaptors;
    rude::Config config;
    if (config.load(CONFIG_FILE)) {
        config.setSection("Adaptors");
        int num = config.getNumDataMembers();
        if (num > 0) {
            for (int i = 0; i < num; i++) {
                const char *data = config.getDataNameAt(i);
                if (data != nullptr) {
                    adaptors.push_back(data);
                }
            }
        }
    }

    m_mailer.readConfig(CONFIG_FILE);

    m_controller = Controller::getInstance();
    m_controller->subscribeMsg(MsgType_Log, this);
    m_controller->subscribeMsg(MsgType_AdaptorStatus, this);
    m_controller->initialize();

    for (size_t i = 0; i < adaptors.size(); i++) {
        m_controller->loadAdaptor(adaptors[i].c_str());
    }

    m_controller->run();

    m_timer->start(1000);
}

void Dialog::onMessage(const Message* msg)
{
    emit msgReceived(*msg);
}

void Dialog::appendLogText(const char* text)
{
    Message msg = { 0 };
    msg.header.type = MsgType_Log;
    strcpy(msg.body.log.text, text);

    emit msgReceived(msg);
}

void Dialog::readRestartTime()
{
    rude::Config config;
    if (config.load(CONFIG_FILE)) {
        config.setSection("Restart");
        int num = config.getNumDataMembers();
        if (num > 0) {
            for (int i = 0; i < num; i++) {
                const char *data = config.getDataNameAt(i);
                if (data != NULL) {
                    if (strlen(data) != strlen("xx:xx:xx")) {
                        continue;
                    }
                    char temp[16];
                    strcpy_s(temp, data);
                    temp[2] = '\0';
                    temp[5] = '\0';
                    int secs = atoi(temp) * 3600 + atoi(temp + 3) * 60 + atoi(temp + 6);
                    m_restartTime.push_back(secs);
                }
            }
        } else {
            appendLogText("Please specify restart time in the config file.");
        }
    } else {
        appendLogText("Can not load config file.");
    }

    SYSTEMTIME lt;
    GetLocalTime(&lt);
    m_schedCurrSec = lt.wHour * 3600 + lt.wMinute * 60 + lt.wSecond;
    m_schedLastSec = m_schedCurrSec;
}

void Dialog::checkRestartTask()
{
    for (size_t i = 0; i < m_restartTime.size(); i++) {
        m_restartSecs = m_restartTime[i];
        if (m_schedLastSec < m_restartSecs && m_schedCurrSec >= m_restartSecs) {
            if (!RA_ActivateRestartProcess())
            {
                // Handle restart error here
                return;
            }

            QApplication::exit();
        }
    }
}

void Dialog::timeElapsed()
{
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    m_schedCurrSec = lt.wHour * 3600 + lt.wMinute * 60 + lt.wSecond;

    checkRestartTask();

    m_schedLastSec = m_schedCurrSec;
}

void Dialog::updateUI(Message msg)
{
    int type = msg.header.type;
    if (type == MsgType_Log) {
        int date = msg.body.log.date;
        int time = msg.body.log.time;
        int ms = msg.body.log.ms;

        int year = date / 10000;
        int month = (date % 10000) / 100;
        int day = date % 100;
        int hour = time / 10000;
        int min = (time % 10000) / 100;
        int sec = time % 100;

        char c;
        switch (msg.body.log.level) {
        default:
        case LOG_INFO:
            c = 'I';
            break;
        case LOG_DEBUG:
            c = 'D';
            break;
        case LOG_WARN:
            c = 'W';
            break;
        case LOG_ERROR:
            c = 'E';
            break;
        case LOG_FATAL:
            c = 'F';
            break;
        }
        char text[1024] = { 0 };
        sprintf(text, "[%04d/%02d/%02d %02d:%02d:%02d.%03d][%c] %s", year, month, day, hour, min, sec, ms, c, msg.body.log.text);

        ui->logsText->appendPlainText(QString().fromLocal8Bit(text));
        QScrollBar *sb = ui->logsText->verticalScrollBar();
        sb->setValue(sb->maximum());

        if (msg.body.log.level == LOG_ERROR ||
            msg.body.log.level == LOG_FATAL) {
            m_mailer.sendMail(std::string(msg.body.log.text));
        }
#if 0
        bool doScroll = (ui->logsText->verticalScrollBar()->isVisible() && 
            !m_logTextScrollVisible) || 
            ui->logsText->verticalScrollBar()->sliderPosition() == ui->logsText->verticalScrollBar()->maximum();

        QTextCursor c = ui->logsText->textCursor();
        c.beginEditBlock();
        c.movePosition(QTextCursor::End);
        c.insertText(QString().fromLocal8Bit(m->body.log.text));
        c.endEditBlock();

        if (doScroll)
        {
            ui->logsText->verticalScrollBar()->setSliderPosition(ui->logsText->verticalScrollBar()->maximum());
        }

        m_logTextScrollVisible = ui->logsText->verticalScrollBar()->isVisible();
#endif
    } else if (type == MsgType_AdaptorStatus) {
        m_adaptorModel->updateAdaptorStatus(&msg.body.adaptorStatus);
    }
}

void Dialog::reject()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;

    resBtn = QMessageBox::question(this, "MarketHub",
        tr("Are you sure you want to close Application?\n"),
        QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
        QMessageBox::Yes);

    if (resBtn == QMessageBox::Yes) {
        QDialog::reject();
    }
}

Dialog::~Dialog()
{
    m_controller->stop();
    m_controller->release();
    m_controller = nullptr;

    delete ui;
}
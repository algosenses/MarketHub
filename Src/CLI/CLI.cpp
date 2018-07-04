#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include "MarketHub.h"
#include "config.h"
#include "RestartAPI.h"
#include "TimerManager.h"
//#include "vld.h"

static MarketHub::Controller* controller = nullptr;
static TimerManager timerMgr;
static TimerContext timerCtx = { 0 };
static std::vector<long> restartTimeList;
static unsigned long sysPrevTimestamp = 0;
static unsigned long sysCurrTimestamp = 0;
static volatile int appExit = 0;

static void timerProc(int timerId, const TimerContext& context)
{
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    sysCurrTimestamp = lt.wHour * 3600 + lt.wMinute * 60 + lt.wSecond;

    for (size_t i = 0; i < restartTimeList.size(); i++) {
        unsigned long restartSecond = restartTimeList[i];
        if (sysPrevTimestamp < restartSecond && sysCurrTimestamp >= restartSecond) {
            
            appExit = 1;

            if (!RA_ActivateRestartProcess()) {
                // Handle restart error here
                return;
            }

            return;
        }
    }

    sysPrevTimestamp = sysCurrTimestamp;

    timerMgr.registerTimer(1000, 0, timerCtx);
}

static void registerTimer()
{
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    sysCurrTimestamp = lt.wHour * 3600 + lt.wMinute * 60 + lt.wSecond;
    sysPrevTimestamp = sysCurrTimestamp;

    timerMgr.setHandler(&timerProc);
    timerMgr.registerTimer(1000, 0, timerCtx);
}

int main()
{
	controller = MarketHub::Controller::getInstance();
    if (controller == nullptr) {
        return -1;
    }

    std::vector<std::string> adaptors;

    rude::Config config;
    if (config.load("MarketHub.ini")) {
        config.setSection("Adaptors");
        int num = config.getNumDataMembers();
        if (num > 0) {
            for (int i = 0; i < num; i++) {
                const char *line = config.getDataNameAt(i);
                if (line != NULL) {
                    adaptors.push_back(line);
                }
            }
        }

        config.setSection("Restart");
        num = config.getNumDataMembers();
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
                    restartTimeList.push_back(secs);
                }
            }
        }
	} else {
		std::cerr << "Could not load config file!" << std::endl;
	}

    controller->initialize();
    for (size_t i = 0; i < adaptors.size(); i++) {
        controller->loadAdaptor(adaptors[i].c_str());
    }

    if (restartTimeList.size() > 0) {
        registerTimer();
    }

    controller->run();

    while (!appExit) {
        Sleep(1000);
    }
    
    controller->stop();
    controller->release();

    return 0;
}
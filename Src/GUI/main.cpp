#include <iostream>
#include <QApplication>
#include <QStyleFactory>
#include <QTextCodec>
#include <windows.h>
#include "dialog.h"
#include "RestartAPI.h"
//#include "vld.h"

int main(int argc, char *argv[])
{
#if 0
    if (!AllocConsole()) {
        return 0;
    }

    freopen("CONIN$", "rb", stdin);
    freopen("CONOUT$", "wb", stdout);
    freopen("CONOUT$", "wb", stderr);
#endif

    // Initialize restart code
    // Check if this instance is restarted and 
    // wait while previous instance finish
    if (RA_CheckForRestartProcessStart()) {
        RA_WaitForPreviousProcessFinish();
    }

    // It is important to put QApplication into a local domain, thus ui widgets 
    // will be totally released before exit main function.
    int ret = 0;
    { 
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("system"));

        QApplication a(argc, argv);
#if QT_VERSION >= 0x050000
        a.setStyle(QStyleFactory::create("Fusion"));
#endif

        Dialog w;
        w.show();

        ret = a.exec();
    }

    // Finish restarting process if needed
    RA_DoRestartProcessFinish();

    return ret;
}

#include <iostream>
#include <windows.h>
#include <Shlwapi.h>
#include <Tchar.h>
#include "RestartAPI.h"

#pragma comment(lib, "Shlwapi.lib")

// Global Variables
HANDLE g_RA_hMutexOtherRestarting = NULL;	// Mutex
bool g_RA_bWasRestarted = FALSE;			// Restarted Flag

bool RA_CheckProcessWasRestarted()
{
	return g_RA_bWasRestarted;
}

bool RA_CheckForRestartProcessStart()
{
	// Simple find substring in command line
	LPTSTR szCmdLine = ::GetCommandLine();

	return ::StrStr(szCmdLine, RA_CMDLINE_RESTART_PROCESS) != NULL;
}

bool RA_WaitForPreviousProcessFinish()
{
    TCHAR szAppPath[MAX_PATH] = { 0 };
    ::GetModuleFileName(NULL, szAppPath, MAX_PATH);

    // http://stackoverflow.com/questions/20714120/could-not-find-a-part-of-the-path-error-while-creating-mutex/20714164#20714164
    /* On a server that is running Terminal Services, a named system mutex can have two levels of visibility. 
       If its name begins with the prefix "Global\", the mutex is visible in all terminal server sessions. 
       If its name begins with the prefix "Local\", the mutex is visible only in the terminal server session 
       where it was created. In that case, a separate mutex with the same name can exist in each of the other 
       terminal server sessions on the server. If you do not specify a prefix when you create a named mutex, 
       it takes the prefix "Local\". Within a terminal server session, two mutexes whose names differ only by 
       their prefixes are separate mutexes, and both are visible to all processes in the terminal server session. 
       That is, the prefix names "Global\" and "Local\" describe the scope of the mutex name relative to terminal
       server sessions, not relative to processes.

       Because there were backslash in szAppPath (\) it assumes you are trying to specify a visibility level, 
       and then discovers it isn't a valid visibility level - hence generating the exception.
       In short, mutex name had '\' in it, which windows was interpreting as a path character. So we replace all
       '\' with '-'.
    */
    LPTSTR ptr = ::StrChr(szAppPath, TEXT('\\'));
    while (ptr != NULL) {
        *ptr = TEXT('-');
        ptr = ::StrChr(szAppPath, TEXT('\\'));
    }

	// App restarting
	BOOL AlreadyRunning;
	// Try to Create Mutex
	g_RA_hMutexOtherRestarting = ::CreateMutex( NULL, FALSE, szAppPath);
	DWORD dwLastError = ::GetLastError();
	AlreadyRunning = (dwLastError == ERROR_ALREADY_EXISTS || dwLastError == ERROR_ACCESS_DENIED);
	if ( AlreadyRunning )
	{
		// Waiting for previous instance release mutex
		::WaitForSingleObject(g_RA_hMutexOtherRestarting, INFINITE);
		::ReleaseMutex(g_RA_hMutexOtherRestarting);
		g_RA_bWasRestarted = TRUE;
	}
	::CloseHandle(g_RA_hMutexOtherRestarting);
	g_RA_hMutexOtherRestarting = NULL;
	return TRUE;
}

bool RA_DoRestartProcessFinish()
{
	// Releasing mutex signal that process finished
	DWORD dwWaitResult = WaitForSingleObject(g_RA_hMutexOtherRestarting, 0);
	if (dwWaitResult == WAIT_TIMEOUT)
		::ReleaseMutex(g_RA_hMutexOtherRestarting);
	::CloseHandle(g_RA_hMutexOtherRestarting);
	g_RA_hMutexOtherRestarting = NULL;
	return (dwWaitResult == WAIT_TIMEOUT);
}

bool RA_ActivateRestartProcess()
{
    TCHAR szAppPath[MAX_PATH] = {0};
    ::GetModuleFileName(NULL, szAppPath, MAX_PATH);

    LPTSTR ptr = ::StrChr(szAppPath, TEXT('\\'));
    while (ptr != NULL) {
        *ptr = TEXT('-');
        ptr = ::StrChr(szAppPath, TEXT('\\'));
    }

	// Restart App
	BOOL AlreadyRunning;
	g_RA_hMutexOtherRestarting = ::CreateMutex( NULL, TRUE, szAppPath);
	DWORD dwLastError = ::GetLastError();
	AlreadyRunning = (dwLastError == ERROR_ALREADY_EXISTS || dwLastError == ERROR_ACCESS_DENIED);
	if (AlreadyRunning)
	{
		::WaitForSingleObject(g_RA_hMutexOtherRestarting, INFINITE);
		::ReleaseMutex(g_RA_hMutexOtherRestarting);
		::CloseHandle(g_RA_hMutexOtherRestarting);
		return FALSE;
	}

	STARTUPINFO				si = {0};
	PROCESS_INFORMATION		pi = {0};
	si.cb = sizeof(STARTUPINFO);
	// Create New Instance command line
    ::GetModuleFileName(NULL, szAppPath, MAX_PATH);
	::PathQuoteSpaces(szAppPath);
	::lstrcat(szAppPath, _T(" "));
	::lstrcat(szAppPath, RA_CMDLINE_RESTART_PROCESS); // Add command line key for restart
	// Create another copy of processS
	return ::CreateProcess(NULL, szAppPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
}

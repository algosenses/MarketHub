#ifndef EXPORT_H
#define EXPORT_H

#if defined(_MSC_VER)
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MARKETHUB_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DllExport functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef MARKETHUB_EXPORTS
#define DllExport __declspec(dllexport)
#else
#define DllExport __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define DllExport __attribute__((visibility("default")))
#define DllImport
#else
//  do nothing and hope for the best?
#define DllExport
#define DllImport
#pragma warning Unknown dynamic link import/export semantics.
#endif

#endif
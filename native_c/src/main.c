#define _WIN32_WINNT 0x0A00
#include "ui.h"
#include "core.h"
#include "logger.h"
#include <commctrl.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

static void show_startup_error(const wchar_t *message){MessageBoxW(NULL,message,L"ZakonDownloader — помилка запуску",MB_OK|MB_ICONERROR);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show_command){(void)previous;(void)command_line;(void)show_command;SetProcessDPIAware();HRESULT com=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_LISTVIEW_CLASSES|ICC_DATE_CLASSES|ICC_PROGRESS_CLASS};if(!InitCommonControlsEx(&icc)){show_startup_error(L"Не вдалося ініціалізувати елементи інтерфейсу Windows.");if(SUCCEEDED(com))CoUninitialize();return 1;}ZDPaths paths;char error[ZD_ERROR_MAX];if(!zd_paths_init(&paths,error,sizeof(error))){wchar_t text[ZD_ERROR_MAX];MultiByteToWideChar(CP_UTF8,0,error,-1,text,ZD_ERROR_MAX);show_startup_error(text);if(SUCCEEDED(com))CoUninitialize();return 1;}ZDSettings settings;if(!zd_settings_load(paths.settings_file,&settings,error,sizeof(error))){wchar_t text[ZD_ERROR_MAX];MultiByteToWideChar(CP_UTF8,0,error,-1,text,ZD_ERROR_MAX);show_startup_error(text);if(SUCCEEDED(com))CoUninitialize();return 1;}if(!settings.documents_dir[0])snprintf(settings.documents_dir,sizeof(settings.documents_dir),"%s",paths.documents);ZDSqliteApi sqlite;if(!zd_sqlite_load(&sqlite,error,sizeof(error))){wchar_t text[ZD_ERROR_MAX];MultiByteToWideChar(CP_UTF8,0,error,-1,text,ZD_ERROR_MAX);show_startup_error(text);if(SUCCEEDED(com))CoUninitialize();return 1;}zd_log_write(&paths,"INFO","startup","Запуск нативної C-версії.");int rc=zd_gui_run(instance,&paths,&settings,&sqlite);zd_log_write(&paths,"INFO","startup","Завершення нативної C-версії.");zd_sqlite_unload(&sqlite);if(SUCCEEDED(com))CoUninitialize();return rc;}

#include "sqlite_api.h"
#include <stdio.h>
#include <string.h>

static void set_error(char *out,size_t cap,const char *message){if(out&&cap){snprintf(out,cap,"%s",message?message:"SQLite initialization error");out[cap-1]='\0';}}
#define LOAD(member,symbol) do { FARPROC p=GetProcAddress(api->module,symbol); if(!p){set_error(error,error_cap,"Windows SQLite API is missing a required function.");zd_sqlite_unload(api);return 0;} memcpy(&api->member,&p,sizeof(api->member)); } while(0)
int zd_sqlite_load(ZDSqliteApi*api,char*error,size_t error_cap){if(error&&error_cap)error[0]=0;if(!api){set_error(error,error_cap,"SQLite initialization failed.");return 0;}memset(api,0,sizeof(*api));api->module=LoadLibraryExW(L"winsqlite3.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!api->module){set_error(error,error_cap,"Windows SQLite (winsqlite3.dll) is not available. Use Windows 10/11 with current system components.");return 0;}LOAD(open,"sqlite3_open");LOAD(open_v2,"sqlite3_open_v2");LOAD(close,"sqlite3_close");LOAD(errmsg,"sqlite3_errmsg");LOAD(prepare_v2,"sqlite3_prepare_v2");LOAD(bind_text,"sqlite3_bind_text");LOAD(bind_int,"sqlite3_bind_int");LOAD(bind_int64,"sqlite3_bind_int64");LOAD(busy_timeout,"sqlite3_busy_timeout");LOAD(step,"sqlite3_step");LOAD(column_int64,"sqlite3_column_int64");LOAD(column_text,"sqlite3_column_text");LOAD(finalize,"sqlite3_finalize");LOAD(exec,"sqlite3_exec");LOAD(free_mem,"sqlite3_free");return 1;}
void zd_sqlite_unload(ZDSqliteApi*api){if(!api)return;if(api->module)FreeLibrary(api->module);memset(api,0,sizeof(*api));}

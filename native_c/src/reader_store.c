#include "reader_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS reader_progress("
    "file_path TEXT PRIMARY KEY NOT NULL,position INTEGER NOT NULL,updated_at TEXT NOT NULL);"
    "CREATE TABLE IF NOT EXISTS reader_bookmarks("
    "file_path TEXT NOT NULL,position INTEGER NOT NULL,label TEXT NOT NULL,created_at TEXT NOT NULL,"
    "PRIMARY KEY(file_path,position));"
    "CREATE INDEX IF NOT EXISTS idx_reader_bookmarks_path ON reader_bookmarks(file_path,position);";

static void set_error(char *out,size_t cap,const char *s){if(out&&cap){snprintf(out,cap,"%s",s?s:"Помилка сховища читання.");out[cap-1]=0;}}
static int open_db(ZDSqliteApi *api,const char *path,sqlite3 **db,char *error,size_t cap){
    *db=NULL;
    int rc=api->open_v2(path,db,ZD_SQLITE_OPEN_READWRITE|ZD_SQLITE_OPEN_CREATE|ZD_SQLITE_OPEN_FULLMUTEX,NULL);
    if(rc!=ZD_SQLITE_OK){set_error(error,cap,*db?api->errmsg(*db):"Не вдалося відкрити базу даних.");if(*db){api->close(*db);*db=NULL;}return 0;}
    if(api->busy_timeout(*db,5000)!=ZD_SQLITE_OK){set_error(error,cap,api->errmsg(*db));api->close(*db);*db=NULL;return 0;}
    char *message=NULL;rc=api->exec(*db,SCHEMA,NULL,NULL,&message);
    if(rc!=ZD_SQLITE_OK){set_error(error,cap,message?message:api->errmsg(*db));if(message)api->free_mem(message);api->close(*db);*db=NULL;return 0;}
    if(message)api->free_mem(message);
    return 1;
}
static int bind_path(ZDSqliteApi *api,sqlite3_stmt *stmt,int index,const char *path){return api->bind_text(stmt,index,path,-1,ZD_SQLITE_TRANSIENT)==ZD_SQLITE_OK;}
static void timestamp(char out[20]){SYSTEMTIME t;GetLocalTime(&t);snprintf(out,20,"%04u-%02u-%02uT%02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);}
static char *copy_text(const unsigned char *s){if(!s)return NULL;size_t n=strlen((const char*)s);char *p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}

int zd_reader_progress_get(ZDSqliteApi *api,const char *db_path,const char *file_path,int64_t *position,char *error,size_t cap){
    if(error&&cap)error[0]=0;if(position)*position=0;
    if(!api||!db_path||!file_path||!position){set_error(error,cap,"Неповні параметри позиції читання.");return 0;}
    sqlite3 *db=NULL;if(!open_db(api,db_path,&db,error,cap))return 0;
    sqlite3_stmt *stmt=NULL;int ok=api->prepare_v2(db,"SELECT position FROM reader_progress WHERE file_path=?",-1,&stmt,NULL)==ZD_SQLITE_OK;
    if(ok)ok=bind_path(api,stmt,1,file_path);
    if(ok){int rc=api->step(stmt);if(rc==ZD_SQLITE_ROW)*position=api->column_int64(stmt,0);else if(rc!=ZD_SQLITE_DONE){set_error(error,cap,api->errmsg(db));ok=0;}}
    if(!ok&&(!error||!error[0]))set_error(error,cap,api->errmsg(db));
    if(stmt)api->finalize(stmt);api->close(db);return ok;
}

int zd_reader_progress_put(ZDSqliteApi *api,const char *db_path,const char *file_path,int64_t position,char *error,size_t cap){
    if(error&&cap)error[0]=0;
    if(!api||!db_path||!file_path||position<0){set_error(error,cap,"Некоректні параметри позиції читання.");return 0;}
    sqlite3 *db=NULL;if(!open_db(api,db_path,&db,error,cap))return 0;
    sqlite3_stmt *stmt=NULL;const char *sql="INSERT OR REPLACE INTO reader_progress(file_path,position,updated_at) VALUES(?,?,?)";
    int ok=api->prepare_v2(db,sql,-1,&stmt,NULL)==ZD_SQLITE_OK;char now[20];timestamp(now);
    if(ok)ok=bind_path(api,stmt,1,file_path)&&api->bind_int64(stmt,2,(long long)position)==ZD_SQLITE_OK&&api->bind_text(stmt,3,now,-1,ZD_SQLITE_TRANSIENT)==ZD_SQLITE_OK;
    if(ok)ok=api->step(stmt)==ZD_SQLITE_DONE;
    if(!ok)set_error(error,cap,api->errmsg(db));if(stmt)api->finalize(stmt);api->close(db);return ok;
}

void zd_reader_bookmarks_free(ZDReaderBookmarkList *list){if(!list)return;for(size_t i=0;i<list->count;i++)free(list->items[i].label);free(list->items);memset(list,0,sizeof(*list));}

int zd_reader_bookmarks_load(ZDSqliteApi *api,const char *db_path,const char *file_path,ZDReaderBookmarkList *list,char *error,size_t cap){
    if(error&&cap)error[0]=0;
    if(!api||!db_path||!file_path||!list){set_error(error,cap,"Неповні параметри закладок.");return 0;}
    zd_reader_bookmarks_free(list);sqlite3 *db=NULL;if(!open_db(api,db_path,&db,error,cap))return 0;
    sqlite3_stmt *stmt=NULL;const char *sql="SELECT position,label FROM reader_bookmarks WHERE file_path=? ORDER BY position";
    int ok=api->prepare_v2(db,sql,-1,&stmt,NULL)==ZD_SQLITE_OK;if(ok)ok=bind_path(api,stmt,1,file_path);
    size_t capacity=0;
    while(ok){int rc=api->step(stmt);if(rc==ZD_SQLITE_DONE)break;if(rc!=ZD_SQLITE_ROW){set_error(error,cap,api->errmsg(db));ok=0;break;}
        if(list->count==capacity){size_t next=capacity?capacity*2:8;if(next>1000)next=1000;if(next<=capacity){set_error(error,cap,"Забагато закладок для одного документа.");ok=0;break;}ZDReaderBookmark *p=(ZDReaderBookmark*)realloc(list->items,next*sizeof(*p));if(!p){set_error(error,cap,"Недостатньо пам'яті для закладок.");ok=0;break;}list->items=p;capacity=next;}
        ZDReaderBookmark item;item.position=api->column_int64(stmt,0);item.label=copy_text(api->column_text(stmt,1));if(!item.label){set_error(error,cap,"Недостатньо пам'яті для назви закладки.");ok=0;break;}list->items[list->count++]=item;
    }
    if(!ok&&(!error||!error[0]))set_error(error,cap,api->errmsg(db));if(stmt)api->finalize(stmt);api->close(db);if(!ok)zd_reader_bookmarks_free(list);return ok;
}

int zd_reader_bookmark_put(ZDSqliteApi *api,const char *db_path,const char *file_path,int64_t position,const char *label,char *error,size_t cap){
    if(error&&cap)error[0]=0;if(!api||!db_path||!file_path||position<0||!label){set_error(error,cap,"Некоректні параметри закладки.");return 0;}
    sqlite3 *db=NULL;if(!open_db(api,db_path,&db,error,cap))return 0;sqlite3_stmt *stmt=NULL;
    const char *sql="INSERT OR REPLACE INTO reader_bookmarks(file_path,position,label,created_at) VALUES(?,?,?,?)";
    int ok=api->prepare_v2(db,sql,-1,&stmt,NULL)==ZD_SQLITE_OK;char now[20];timestamp(now);
    if(ok)ok=bind_path(api,stmt,1,file_path)&&api->bind_int64(stmt,2,(long long)position)==ZD_SQLITE_OK&&api->bind_text(stmt,3,label,-1,ZD_SQLITE_TRANSIENT)==ZD_SQLITE_OK&&api->bind_text(stmt,4,now,-1,ZD_SQLITE_TRANSIENT)==ZD_SQLITE_OK;
    if(ok)ok=api->step(stmt)==ZD_SQLITE_DONE;if(!ok)set_error(error,cap,api->errmsg(db));if(stmt)api->finalize(stmt);api->close(db);return ok;
}

int zd_reader_bookmark_delete(ZDSqliteApi *api,const char *db_path,const char *file_path,int64_t position,char *error,size_t cap){
    if(error&&cap)error[0]=0;if(!api||!db_path||!file_path||position<0){set_error(error,cap,"Некоректні параметри видалення закладки.");return 0;}
    sqlite3 *db=NULL;if(!open_db(api,db_path,&db,error,cap))return 0;sqlite3_stmt *stmt=NULL;
    int ok=api->prepare_v2(db,"DELETE FROM reader_bookmarks WHERE file_path=? AND position=?",-1,&stmt,NULL)==ZD_SQLITE_OK;
    if(ok)ok=bind_path(api,stmt,1,file_path)&&api->bind_int64(stmt,2,(long long)position)==ZD_SQLITE_OK;
    if(ok)ok=api->step(stmt)==ZD_SQLITE_DONE;if(!ok)set_error(error,cap,api->errmsg(db));if(stmt)api->finalize(stmt);api->close(db);return ok;
}

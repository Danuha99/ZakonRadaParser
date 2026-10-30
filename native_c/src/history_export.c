#include "history_export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_LIMIT (256u * 1024u * 1024u)
typedef struct { char *data; size_t length,capacity; } Buffer;

static void set_error(char*out,size_t cap,const char*s){if(out&&cap){snprintf(out,cap,"%s",s?s:"Помилка експорту CSV.");out[cap-1]=0;}}
static int append(Buffer*b,const void*data,size_t n){if(n>CSV_LIMIT-b->length)return 0;size_t need=b->length+n;if(need>b->capacity){size_t cap=b->capacity?b->capacity:4096;while(cap<need){if(cap>CSV_LIMIT/2){cap=CSV_LIMIT;break;}cap*=2;}char*p=(char*)realloc(b->data,cap);if(!p)return 0;b->data=p;b->capacity=cap;}memcpy(b->data+b->length,data,n);b->length+=n;return 1;}
static int cell(Buffer*b,const char*text){if(!text)text="";const unsigned char*p=(const unsigned char*)text;while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;int formula=*p=='='||*p=='+'||*p=='-'||*p=='@';if(!append(b,"\"",1))return 0;if(formula&&!append(b,"'",1))return 0;for(const char*s=text;*s;s++){if(*s=='"'&&!append(b,"\"",1))return 0;if(!append(b,s,1))return 0;}return append(b,"\"",1);}
static int row(Buffer*b,const char*const*fields,size_t count){for(size_t i=0;i<count;i++){if(i&&!append(b,",",1))return 0;if(!cell(b,fields[i]))return 0;}return append(b,"\r\n",2);}
static int to_wide(const char*s,wchar_t*out,size_t cap){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);if(n<=0||(size_t)n>cap)return 0;return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,(int)cap)>0;}
static int write_atomic(const wchar_t*path,const char*data,size_t length){wchar_t temp[ZD_PATH_MAX];int n=swprintf(temp,ZD_PATH_MAX,L"%ls.%lu.%lu.tmp",path,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount());if(n<0||(size_t)n>=ZD_PATH_MAX)return 0;HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;size_t at=0;int ok=1;while(at<length){DWORD part=(DWORD)((length-at)>0x7fffffff?0x7fffffff:length-at),written=0;if(!WriteFile(f,data+at,part,&written,NULL)||written!=part){ok=0;break;}at+=written;}if(ok)ok=FlushFileBuffers(f)!=0;CloseHandle(f);if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok)DeleteFileW(temp);return ok;}

int zd_history_export_csv(const ZDPaths*paths,const ZDHistoryList*history,char*saved_path,size_t path_cap,char*error,size_t error_cap){
    if(error&&error_cap)error[0]=0;if(saved_path&&path_cap)saved_path[0]=0;
    if(!paths||!history||!saved_path||!path_cap){set_error(error,error_cap,"Неповні параметри експорту історії.");return 0;}
    Buffer b={0};static const unsigned char bom[]={0xef,0xbb,0xbf};
    if(!append(&b,bom,sizeof(bom))){set_error(error,error_cap,"Недостатньо пам'яті для CSV.");return 0;}
    const char*headers[]={"ID","URL","Статус","Почато","Завершено","Назва документа","Збережений файл","Помилка"};
    if(!row(&b,headers,sizeof(headers)/sizeof(headers[0]))){free(b.data);set_error(error,error_cap,"Не вдалося сформувати CSV-заголовок.");return 0;}
    for(size_t i=0;i<history->count;i++){const ZDHistoryEntry*e=&history->items[i];char id[32];snprintf(id,sizeof(id),"%lld",(long long)e->id);const char*fields[]={id,e->url,e->status,e->started_at,e->finished_at,e->document_title,e->saved_path,e->error_message};if(!row(&b,fields,sizeof(fields)/sizeof(fields[0]))){free(b.data);set_error(error,error_cap,"CSV перевищив ліміт 256 МБ або не вистачило пам'яті.");return 0;}}
    SYSTEMTIME t;GetLocalTime(&t);char path[ZD_PATH_MAX];int n=snprintf(path,sizeof(path),"%s\\history_%04u%02u%02u_%02u%02u%02u_%lu_%lu.csv",paths->exported,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount());if(n<0||(size_t)n>=sizeof(path)){free(b.data);set_error(error,error_cap,"Шлях експорту надто довгий.");return 0;}
    wchar_t wide[ZD_PATH_MAX];if(!to_wide(path,wide,ZD_PATH_MAX)){free(b.data);set_error(error,error_cap,"Некоректний шлях папки експорту.");return 0;}
    if(!write_atomic(wide,b.data,b.length)){free(b.data);set_error(error,error_cap,"Не вдалося атомарно створити CSV у папці експорту.");return 0;}
    if(strlen(path)+1>path_cap){DeleteFileW(wide);free(b.data);set_error(error,error_cap,"Буфер шляху CSV замалий.");return 0;}
    strcpy(saved_path,path);free(b.data);return 1;
}

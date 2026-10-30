#define UNICODE
#define _UNICODE
#include "local_document.h"
#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

static void set_error(char*out,size_t cap,const char*text){if(out&&cap){snprintf(out,cap,"%s",text?text:"");out[cap-1]=0;}}
static wchar_t *dup_wide_n(const wchar_t*s,size_t n){wchar_t*p=(wchar_t*)malloc((n+1)*sizeof(*p));if(!p)return NULL;memcpy(p,s,n*sizeof(*p));p[n]=0;return p;}
static int compare_documents(const void*a,const void*b){const ZDLocalDocument*x=(const ZDLocalDocument*)a,*y=(const ZDLocalDocument*)b;int c=CompareFileTime(&y->modified,&x->modified);if(c)return c;return CompareStringOrdinal(x->name,-1,y->name,-1,TRUE)-CSTR_EQUAL;}
void zd_local_documents_free(ZDLocalDocumentList*list){if(!list)return;for(size_t i=0;i<list->count;i++){free(list->items[i].path);free(list->items[i].name);}free(list->items);memset(list,0,sizeof(*list));}
int zd_local_documents_load(const wchar_t*folder,ZDLocalDocumentList*list,char*error,size_t error_cap){
    if(error&&error_cap)error[0]=0;if(!folder||!list){set_error(error,error_cap,"Не передано папку документів.");return 0;}
    zd_local_documents_free(list);size_t n=wcslen(folder);if(n>32700){set_error(error,error_cap,"Шлях до папки надто довгий.");return 0;}
    wchar_t*pattern=(wchar_t*)malloc((n+8)*sizeof(*pattern));if(!pattern){set_error(error,error_cap,"Недостатньо пам'яті для пошуку файлів.");return 0;}
    memcpy(pattern,folder,n*sizeof(*pattern));if(n&&folder[n-1]!=L'\\'&&folder[n-1]!=L'/')pattern[n++]=L'\\';memcpy(pattern+n,L"*.txt",6*sizeof(*pattern));
    WIN32_FIND_DATAW data;HANDLE find=FindFirstFileW(pattern,&data);free(pattern);if(find==INVALID_HANDLE_VALUE){DWORD e=GetLastError();if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)return 1;set_error(error,error_cap,"Не вдалося прочитати папку документів.");return 0;}
    size_t capacity=0;int ok=1;
    do{
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
        if(data.cFileName[0]==L'.'||data.cFileName[0]==0)continue;
        if(list->count>=ZD_LOCAL_DOC_LIMIT)break;
        if(list->count==capacity){size_t next=capacity?capacity*2:64;if(next>ZD_LOCAL_DOC_LIMIT)next=ZD_LOCAL_DOC_LIMIT;ZDLocalDocument*p=(ZDLocalDocument*)realloc(list->items,next*sizeof(*p));if(!p){ok=0;break;}list->items=p;capacity=next;}
        size_t fn=wcslen(folder),nn=wcslen(data.cFileName);int sep=fn&&folder[fn-1]!=L'\\'&&folder[fn-1]!=L'/';
        wchar_t*path=(wchar_t*)malloc((fn+(size_t)sep+nn+1)*sizeof(*path));wchar_t*name=dup_wide_n(data.cFileName,nn);if(!path||!name){free(path);free(name);ok=0;break;}
        memcpy(path,folder,fn*sizeof(*path));size_t at=fn;if(sep)path[at++]=L'\\';memcpy(path+at,data.cFileName,(nn+1)*sizeof(*path));
        ZDLocalDocument*d=&list->items[list->count++];d->path=path;d->name=name;d->bytes=((ULONGLONG)data.nFileSizeHigh<<32)|data.nFileSizeLow;d->modified=data.ftLastWriteTime;
    }while(FindNextFileW(find,&data));
    FindClose(find);
    if(!ok){zd_local_documents_free(list);set_error(error,error_cap,"Недостатньо пам'яті для списку локальних документів.");return 0;}
    qsort(list->items,list->count,sizeof(*list->items),compare_documents);return 1;
}
static wchar_t *line_value(const wchar_t*start,const wchar_t*limit,const wchar_t*label){size_t k=wcslen(label);for(const wchar_t*p=start;p<limit;){const wchar_t*end=p;while(end<limit&&*end!=L'\n'&&*end!=L'\r')end++;if((size_t)(end-p)>=k&&wcsncmp(p,label,k)==0){const wchar_t*v=p+k;while(v<end&&(*v==L' '||*v==L'\t'))v++;while(end>v&&(end[-1]==L' '||end[-1]==L'\t'))end--;return dup_wide_n(v,(size_t)(end-v));}while(end<limit&&(*end==L'\n'||*end==L'\r'))end++;p=end;}return NULL;}
static void trim_range(const wchar_t**begin,const wchar_t**end){while(*begin<*end&&iswspace(**begin))(*begin)++;while(*end>*begin&&iswspace((*end)[-1]))(*end)--;}
static int wide_to_utf8(const wchar_t*s,char*out,size_t cap){if(!s||!out||cap<2)return 0;int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,NULL,0,NULL,NULL);if(n<=0||(size_t)n>cap)return 0;return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,out,(int)cap,NULL,NULL)>0;}
int zd_local_document_read(const wchar_t*path,ZDLoadedDocument*doc,char*error,size_t error_cap){
    if(error&&error_cap)error[0]=0;if(!doc||!path){set_error(error,error_cap,"Не передано файл документа.");return 0;}memset(doc,0,sizeof(*doc));
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);if(f==INVALID_HANDLE_VALUE){set_error(error,error_cap,"Не вдалося відкрити локальний текстовий файл.");return 0;}
    LARGE_INTEGER size;if(!GetFileSizeEx(f,&size)||size.QuadPart<0||(ULONGLONG)size.QuadPart>ZD_LOCAL_TEXT_LIMIT){CloseHandle(f);set_error(error,error_cap,"Файл порожній або перевищує обмеження 64 МБ.");return 0;}
    size_t bytes=(size_t)size.QuadPart;char*raw=(char*)malloc(bytes+1);if(!raw){CloseHandle(f);set_error(error,error_cap,"Недостатньо пам'яті для локального документа.");return 0;}
    size_t offset=0;while(offset<bytes){DWORD part=(DWORD)((bytes-offset)>0x7fffffff?0x7fffffff:bytes-offset),got=0;if(!ReadFile(f,raw+offset,part,&got,NULL)||got==0){free(raw);CloseHandle(f);set_error(error,error_cap,"Помилка читання локального документа.");return 0;}offset+=got;}CloseHandle(f);raw[bytes]=0;
    if(bytes>=3&&(unsigned char)raw[0]==0xef&&(unsigned char)raw[1]==0xbb&&(unsigned char)raw[2]==0xbf){memmove(raw,raw+3,bytes-2);bytes-=3;raw[bytes]=0;}
    if(memchr(raw,0,bytes)){free(raw);set_error(error,error_cap,"Файл містить нульові байти, це не UTF-8 текст.");return 0;}
    if(bytes>(size_t)0x7fffffff){free(raw);set_error(error,error_cap,"UTF-8 документ надто довгий для декодування.");return 0;}
    int chars=bytes?MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,raw,(int)bytes,NULL,0):0;if(bytes&&chars<=0){free(raw);set_error(error,error_cap,"Файл не є коректним UTF-8 текстом.");return 0;}
    doc->full_text=(wchar_t*)malloc(((size_t)chars+1)*sizeof(wchar_t));if(!doc->full_text){free(raw);set_error(error,error_cap,"Недостатньо пам'яті для локального документа.");return 0;}
    if(chars&&!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,raw,(int)bytes,doc->full_text,chars)){free(raw);zd_loaded_document_free(doc);set_error(error,error_cap,"Не вдалося перетворити UTF-8 текст.");return 0;}doc->full_text[chars]=0;free(raw);
    if(!chars){zd_loaded_document_free(doc);set_error(error,error_cap,"Локальний текстовий файл порожній.");return 0;}
    const wchar_t*all_end=doc->full_text+wcslen(doc->full_text);const wchar_t*sep=wcsstr(doc->full_text,L"========================================================================");const wchar_t*header_end=sep?sep:all_end;
    doc->title=line_value(doc->full_text,header_end,L"Назва:");
    wchar_t*url=line_value(doc->full_text,header_end,L"Джерело:");if(url){wide_to_utf8(url,doc->source_url,sizeof(doc->source_url));free(url);char validation[ZD_ERROR_MAX];if(!zd_validate_url(doc->source_url,validation,sizeof(validation)))doc->source_url[0]=0;}
    const wchar_t*meta_start=doc->full_text;while(meta_start<header_end&&*meta_start!=L'\n'&&*meta_start!=L'\r')meta_start++;while(meta_start<header_end&&(*meta_start==L'\n'||*meta_start==L'\r'))meta_start++;
    const wchar_t*meta_end=header_end;trim_range(&meta_start,&meta_end);if(meta_start<meta_end)doc->metadata=dup_wide_n(meta_start,(size_t)(meta_end-meta_start));
    if(sep){doc->clean_text=sep+72;while(*doc->clean_text==L'\r'||*doc->clean_text==L'\n')doc->clean_text++;}else doc->clean_text=doc->full_text;
    if(!doc->title){const wchar_t*base=wcsrchr(path,L'\\');if(!base)base=wcsrchr(path,L'/');base=base?base+1:path;size_t n=wcslen(base);if(n>=4&&_wcsicmp(base+n-4,L".txt")==0)n-=4;doc->title=dup_wide_n(base,n);}
    if(!doc->title){zd_loaded_document_free(doc);set_error(error,error_cap,"Недостатньо пам'яті для назви документа.");return 0;}
    if(!*doc->clean_text){zd_loaded_document_free(doc);set_error(error,error_cap,"У локальному файлі немає тексту закону.");return 0;}
    return 1;
}
void zd_loaded_document_free(ZDLoadedDocument*doc){if(!doc)return;free(doc->full_text);free(doc->title);free(doc->metadata);memset(doc,0,sizeof(*doc));}

#define UNICODE
#define _UNICODE
#include "../src/local_document.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures=0;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static int write_file(const wchar_t*path,const char*text,size_t bytes){HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;DWORD wrote=0;int ok=WriteFile(f,text,(DWORD)bytes,&wrote,NULL)&&wrote==bytes;CloseHandle(f);return ok;}
int wmain(void){wchar_t temp[MAX_PATH],folder[MAX_PATH],path[MAX_PATH],bad[MAX_PATH];DWORD n=GetTempPathW(MAX_PATH,temp);CHECK(n>0&&n<MAX_PATH);if(!n||n>=MAX_PATH)return 1;wchar_t unique[MAX_PATH];CHECK(GetTempFileNameW(temp,L"ZDL",0,unique)!=0);DeleteFileW(unique);CHECK(CreateDirectoryW(unique,NULL));swprintf(folder,MAX_PATH,L"%ls",unique);swprintf(path,MAX_PATH,L"%ls\\law.txt",folder);swprintf(bad,MAX_PATH,L"%ls\\bad.txt",folder);
    const char*content="Назва: Закон України про електротехніку\nТип: Закон\nНомер: 123-20\nПрийнятий: 01.01.2024\nРедакція: 02.03.2025\nСтатус: чинний\nДжерело: https://zakon.rada.gov.ua/laws/show/123-20\nЗавантажено: 02.03.2025 10:00\n========================================================================\n\nРозділ I. Загальні положення\nСтаття 1. Основні визначення.\nТекст українською.\n";
    CHECK(write_file(path,content,strlen(content)));ZDLocalDocumentList list={0};char error[512];CHECK(zd_local_documents_load(folder,&list,error,sizeof(error))==1);CHECK(list.count==1);if(list.count){CHECK(_wcsicmp(list.items[0].name,L"law.txt")==0);CHECK(list.items[0].bytes==strlen(content));ZDLoadedDocument doc;CHECK(zd_local_document_read(list.items[0].path,&doc,error,sizeof(error))==1);if(doc.full_text){CHECK(wcsstr(doc.title,L"Закон України про електротехніку")!=NULL);CHECK(wcsstr(doc.clean_text,L"Розділ I. Загальні положення")==doc.clean_text);CHECK(strcmp(doc.source_url,"https://zakon.rada.gov.ua/laws/show/123-20")==0);CHECK(doc.metadata&&wcsstr(doc.metadata,L"Редакція: 02.03.2025")!=NULL);zd_loaded_document_free(&doc);}}
    CHECK(write_file(bad,"\xff\xfe",2));ZDLoadedDocument invalid;CHECK(zd_local_document_read(bad,&invalid,error,sizeof(error))==0);CHECK(strstr(error,"UTF-8")!=NULL);zd_local_documents_free(&list);DeleteFileW(path);DeleteFileW(bad);RemoveDirectoryW(folder);if(failures){fprintf(stderr,"%d local document test(s) failed.\n",failures);return 1;}puts("Local document tests passed.");return 0;}

#define UNICODE
#define _UNICODE
#include "../src/pdf_export.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int wmain(int argc,wchar_t**argv){if(argc<2){fwprintf(stderr,L"Expected a temporary output path.\n");return 2;}const wchar_t*title=L"Закон України про електричні установки та безпеку";const wchar_t*metadata=L"Тип: Закон\r\nНомер: 123-20\r\nРедакція: 02.03.2025\r\nСтатус: чинний";const wchar_t*paragraph=L"Стаття 1. Загальні вимоги. Електротехнічне обладнання та електроустановки мають відповідати вимогам безпеки, надійності й належної експлуатації. Вимірювання та технічне обслуговування виконуються відповідно до чинних нормативних документів.\r\n\r\n";size_t plen=wcslen(paragraph),cap=plen*220+1;wchar_t*body=(wchar_t*)malloc(cap*sizeof(*body));if(!body)return 3;size_t at=0;for(int i=0;i<220;i++){memcpy(body+at,paragraph,plen*sizeof(*body));at+=plen;}body[at]=0;char error[512];DeleteFileW(argv[1]);int ok=zd_pdf_export(argv[1],title,metadata,body,error,sizeof(error));free(body);if(!ok){fprintf(stderr,"PDF export failed: %s\n",error);return 4;}HANDLE f=CreateFileW(argv[1],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 5;char sig[5]={0};DWORD got=0;LARGE_INTEGER size;ok=GetFileSizeEx(f,&size)&&size.QuadPart>4096&&ReadFile(f,sig,5,&got,NULL)&&got==5&&memcmp(sig,"%PDF-",5)==0;CloseHandle(f);DeleteFileW(argv[1]);if(!ok){fprintf(stderr,"PDF signature/size validation failed.\n");return 6;}puts("PDF export test passed.");return 0;}

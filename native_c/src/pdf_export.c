#define UNICODE
#define _UNICODE
#include "pdf_export.h"
#include "settings.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

typedef struct {
    HDC dc;
    HFONT body, bold, title, small;
    int page, dpi_x, dpi_y, left, right, top, bottom, y, line_h;
    const wchar_t *doc_title;
    COLORREF navy, accent, ink, muted;
} Layout;

static void set_error(char *out,size_t cap,const char *text){if(out&&cap){snprintf(out,cap,"%s",text?text:"");out[cap-1]=0;}}
static int start_page(Layout*l,int cover){
    if(StartPage(l->dc)<=0)return 0;
    l->page++;
    SetBkMode(l->dc,TRANSPARENT);
    if(cover){
        SelectObject(l->dc,l->small);SetTextColor(l->dc,l->accent);
        const wchar_t*kicker=L"НОРМАТИВНИЙ ДОКУМЕНТ УКРАЇНИ  ·  ТЕКСТОВЕ ВИДАННЯ";
        TextOutW(l->dc,l->left,l->top,kicker,(int)wcslen(kicker));
        HPEN p=CreatePen(PS_SOLID,4,l->accent);HGDIOBJ old=SelectObject(l->dc,p);
        MoveToEx(l->dc,l->left,l->top+48,NULL);LineTo(l->dc,l->right,l->top+48);
        SelectObject(l->dc,old);DeleteObject(p);l->y=l->top+72;
    }else{
        SelectObject(l->dc,l->small);SetTextColor(l->dc,l->muted);
        const wchar_t*name=l->doc_title?l->doc_title:L"Нормативний документ";
        int n=(int)wcslen(name);if(n>110)n=110;
        TextOutW(l->dc,l->left,l->top,name,n);
        HPEN p=CreatePen(PS_SOLID,1,l->accent);HGDIOBJ old=SelectObject(l->dc,p);
        MoveToEx(l->dc,l->left,l->top+24,NULL);LineTo(l->dc,l->right,l->top+24);
        SelectObject(l->dc,old);DeleteObject(p);l->y=l->top+40;
    }
    return 1;
}
static void finish_page(Layout*l){
    SelectObject(l->dc,l->small);SetTextColor(l->dc,l->muted);
    wchar_t page[48];swprintf(page,48,L"СТОРІНКА  %d",l->page);
    SIZE s={0};GetTextExtentPoint32W(l->dc,page,(int)wcslen(page),&s);
    TextOutW(l->dc,l->right-s.cx,l->bottom+MulDiv(l->dpi_y,18,72),page,(int)wcslen(page));
    HPEN p=CreatePen(PS_SOLID,1,RGB(205,208,211));HGDIOBJ old=SelectObject(l->dc,p);
    MoveToEx(l->dc,l->left,l->bottom+MulDiv(l->dpi_y,10,72),NULL);LineTo(l->dc,l->right,l->bottom+MulDiv(l->dpi_y,10,72));
    SelectObject(l->dc,old);DeleteObject(p);EndPage(l->dc);
}
static int next_page(Layout*l){finish_page(l);if(!start_page(l,0))return 0;l->y+=MulDiv(l->dpi_y,8,72);return 1;}
static int is_heading(const wchar_t*s,size_t n){static const wchar_t*prefixes[]={L"Стаття ",L"Стаття\t",L"Розділ ",L"Глава ",L"Підрозділ ",L"Прикінцеві положення",L"Перехідні положення",L"Загальні положення"};for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]);i++){size_t k=wcslen(prefixes[i]);if(n>=k&&_wcsnicmp(s,prefixes[i],k)==0)return 1;}return n>3&&n<100&&iswupper(s[0])&&iswupper(s[1]);}
static int print_wrapped(Layout*l,const wchar_t*s,size_t n,HFONT font,COLORREF color,int paragraph_gap,int justify){
    SelectObject(l->dc,font);SetTextColor(l->dc,color);TEXTMETRICW tm;if(!GetTextMetricsW(l->dc,&tm))return 0;
    int step=tm.tmHeight+tm.tmExternalLeading+MulDiv(l->dpi_y,2,72);if(step<1)step=1;
    size_t pos=0;int any=0;
    while(pos<n){
        while(pos<n&&iswspace(s[pos]))pos++;
        if(pos>=n)break;
        int remain=(int)((n-pos)>0x7fffffff?(size_t)0x7fffffff:n-pos),fit=0;SIZE extent={0};
        int width=l->right-l->left;
        if(!GetTextExtentExPointW(l->dc,s+pos,remain,width,&fit,NULL,&extent))return 0;
        if(fit<=0)fit=1;
        size_t take=(size_t)fit;
        if(pos+take<n){size_t cut=take;while(cut>0&&!iswspace(s[pos+cut]))cut--;if(cut>0)take=cut;}
        if(l->y+step>l->bottom){if(!next_page(l))return 0;SelectObject(l->dc,font);SetTextColor(l->dc,color);}
        int len=(int)take;SIZE actual={0};GetTextExtentPoint32W(l->dc,s+pos,len,&actual);
        int spaces=0;for(int i=0;i<len;i++)if(s[pos+i]==L' ')spaces++;
        if(justify&&pos+take<n&&spaces>0){int extra=width-actual.cx;if(extra>0&&extra<width/2)SetTextJustification(l->dc,extra,spaces);}
        if(!TextOutW(l->dc,l->left,l->y,s+pos,len)){SetTextJustification(l->dc,0,0);return 0;}
        SetTextJustification(l->dc,0,0);l->y+=step;pos+=take;any=1;
    }
    if(any)l->y+=paragraph_gap;
    return 1;
}
static int print_block(Layout*l,const wchar_t*s,size_t n,HFONT font,COLORREF color,int gap,int justify){
    size_t start=0;
    while(start<n){size_t end=start;while(end<n&&s[end]!=L'\n'&&s[end]!=L'\r')end++;
        if(end==start){l->y+=MulDiv(l->dpi_y,5,72);}
        else{int heading=is_heading(s+start,end-start);HFONT use=heading?l->bold:font;COLORREF ink=heading?l->navy:color;
            if(heading)l->y+=MulDiv(l->dpi_y,4,72);
            if(!print_wrapped(l,s+start,end-start,use,ink,heading?MulDiv(l->dpi_y,5,72):gap,justify&&!heading))return 0;
        }
        while(end<n&&(s[end]==L'\n'||s[end]==L'\r'))end++;
        start=end;
    }
    return 1;
}
static void destroy_layout(Layout*l){if(l->body)DeleteObject(l->body);if(l->bold)DeleteObject(l->bold);if(l->title)DeleteObject(l->title);if(l->small)DeleteObject(l->small);if(l->dc)DeleteDC(l->dc);}
int zd_pdf_export(const wchar_t*output_path,const wchar_t*title,const wchar_t*metadata,const wchar_t*body,char*error,size_t error_cap){
    if(error&&error_cap)error[0]=0;
    if(!output_path||!output_path[0]||!title||!body||!body[0]){set_error(error,error_cap,"Для PDF потрібен шлях, назва й непорожній текст.");return 0;}
    HDC dc=CreateDCW(L"WINSPOOL",L"Microsoft Print to PDF",NULL,NULL);
    if(!dc){set_error(error,error_cap,"Не вдалося відкрити принтер Microsoft Print to PDF.");return 0;}
    Layout l;memset(&l,0,sizeof(l));l.dc=dc;l.doc_title=title;l.dpi_x=GetDeviceCaps(dc,LOGPIXELSX);l.dpi_y=GetDeviceCaps(dc,LOGPIXELSY);
    int width=GetDeviceCaps(dc,HORZRES),height=GetDeviceCaps(dc,VERTRES);
    if(l.dpi_x<72)l.dpi_x=300;if(l.dpi_y<72)l.dpi_y=300;
    l.left=MulDiv(l.dpi_x,72,100);l.right=width-l.left;l.top=MulDiv(l.dpi_y,62,100);l.bottom=height-l.top-MulDiv(l.dpi_y,35,100);
    l.navy=RGB(28,48,67);l.accent=RGB(240,180,0);l.ink=RGB(37,42,47);l.muted=RGB(105,111,118);
    l.body=CreateFontW(-MulDiv(l.dpi_y,10,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    l.bold=CreateFontW(-MulDiv(l.dpi_y,11,72),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    l.title=CreateFontW(-MulDiv(l.dpi_y,23,72),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    l.small=CreateFontW(-MulDiv(l.dpi_y,8,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    if(!l.body||!l.bold||!l.title||!l.small){destroy_layout(&l);set_error(error,error_cap,"Не вдалося створити шрифти для PDF.");return 0;}
    size_t path_n=wcslen(output_path);if(path_n>ZD_PATH_MAX-64){destroy_layout(&l);set_error(error,error_cap,"Шлях PDF надто довгий.");return 0;}
    wchar_t*temp=(wchar_t*)malloc((path_n+64)*sizeof(wchar_t));if(!temp){destroy_layout(&l);set_error(error,error_cap,"Недостатньо пам'яті для шляху PDF.");return 0;}
    swprintf(temp,path_n+64,L"%ls.%lu.%lu.tmp.pdf",output_path,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount());DeleteFileW(temp);
    DOCINFOW di;memset(&di,0,sizeof(di));di.cbSize=sizeof(di);di.lpszDocName=title;di.lpszOutput=temp;
    int job=StartDocW(dc,&di);if(job<=0){free(temp);destroy_layout(&l);set_error(error,error_cap,"Microsoft Print to PDF не почав завдання друку.");return 0;}
    int ok=start_page(&l,1);
    if(ok){ok=print_wrapped(&l,title,wcslen(title),l.title,l.navy,MulDiv(l.dpi_y,6,72),0);}
    if(ok&&metadata&&metadata[0]){ok=print_block(&l,metadata,wcslen(metadata),l.small,l.muted,MulDiv(l.dpi_y,2,72),0);l.y+=MulDiv(l.dpi_y,10,72);}
    if(ok){HPEN p=CreatePen(PS_SOLID,1,l.accent);HGDIOBJ old=SelectObject(dc,p);MoveToEx(dc,l.left,l.y,NULL);LineTo(dc,l.right,l.y);SelectObject(dc,old);DeleteObject(p);l.y+=MulDiv(l.dpi_y,12,72);ok=print_block(&l,body,wcslen(body),l.body,l.ink,MulDiv(l.dpi_y,5,72),1);}
    if(ok)finish_page(&l);else{EndPage(dc);AbortDoc(dc);}
    if(ok&&EndDoc(dc)<=0)ok=0;
    if(ok){HANDLE f=CreateFileW(temp,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)ok=0;else{LARGE_INTEGER n;DWORD got=0;char head[5]={0};ok=GetFileSizeEx(f,&n)&&n.QuadPart>1024&&ReadFile(f,head,5,&got,NULL)&&got==5&&memcmp(head,"%PDF-",5)==0;CloseHandle(f);}}
    if(ok)ok=MoveFileExW(temp,output_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok){DeleteFileW(temp);set_error(error,error_cap,"PDF не створено або принтер не сформував коректний файл.");}
    free(temp);destroy_layout(&l);return ok?1:0;
}

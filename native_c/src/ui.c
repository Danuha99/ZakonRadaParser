#define _WIN32_WINNT 0x0A00
#define UNICODE
#define _UNICODE
#include "ui.h"
#include "download_service.h"
#include "logger.h"
#include "local_document.h"
#include "pdf_export.h"
#include "reader_store.h"
#include "history_export.h"
#include "i18n.h"
#include "resource.h"
#include <commctrl.h>
#include <richedit.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define APP_TITLE L"ZakonDownloader"
#define APP_VERSION L"1.3.0-C"
#define CLASS_NAME L"ZakonDownloaderCMainWindow"
#define SIDE_W 220
#define HEADER_H 108
#define STATUS_H 28
#define PAGE_MAX 64
#define FILTER_TIMER 41
#define DOC_FILTER_TIMER 42
#define READER_SAVE_TIMER 43
#define WM_APP_PROGRESS (WM_APP+1)
#define WM_APP_FINISHED (WM_APP+2)
#define WM_APP_PDF_FINISHED (WM_APP+3)
#define WM_APP_CSV_FINISHED (WM_APP+4)

enum { PAGE_OVERVIEW, PAGE_DOWNLOAD, PAGE_DOCUMENTS, PAGE_HISTORY, PAGE_READER, PAGE_LOGS, PAGE_SETTINGS, PAGE_ABOUT, PAGE_COUNT };
enum {
    ID_NAV_BASE=1000, ID_MODE=1020, ID_THEME,
    ID_OV_NEW=1100, ID_OV_TABLE,
    ID_D_URL=1200, ID_D_PASTE, ID_D_START, ID_D_OPEN,
    ID_DOC_SEARCH=1240, ID_DOC_TABLE, ID_DOC_REFRESH, ID_DOC_VIEW, ID_DOC_BROWSER,
    ID_H_SEARCH=1300, ID_H_STATUS, ID_H_DATE_CHECK, ID_H_FROM, ID_H_TO,
    ID_H_REFRESH, ID_H_TABLE, ID_H_OPEN, ID_H_FOLDER, ID_H_VIEW, ID_H_BROWSER, ID_H_EXPORT=1311,
    ID_R_SEARCH=1350, ID_R_NEXT, ID_R_CLEAN, ID_R_BROWSER, ID_R_TXT, ID_R_PDF, ID_R_OPEN_PDF, ID_R_TEXT=1357, ID_R_TOC=1358, ID_R_BOOKMARKS, ID_R_ADD_BOOKMARK, ID_R_DELETE_BOOKMARK, ID_R_FONT_DOWN, ID_R_FONT_UP, ID_R_COPY_CITATION,
    ID_L_TEXT=1400, ID_L_REFRESH, ID_L_EXPORT, ID_L_FOLDER,
    ID_S_THEME=1500, ID_S_DOCS, ID_S_BROWSE, ID_S_CONNECT, ID_S_READ,
    ID_S_RETRIES, ID_S_LEVEL, ID_S_DEFAULTS, ID_S_SAVE, ID_S_LANGUAGE=1510,
    ID_A_OPEN_DOCS=1600, ID_A_OPEN_DATA, ID_A_OPEN_SITE
};

typedef struct {
    COLORREF bg, surface, surface2, border, text, muted, accent, accent_hover,
             accent_press, accent_text, success, error;
} Palette;
typedef struct App App;
typedef struct { App *app; int stage; wchar_t text[256]; } ProgressMessage;
typedef struct { App *app; char url[ZD_URL_MAX]; } WorkerJob;
typedef struct { ZDDownloadResult result; } WorkerResult;
typedef struct { App *app; wchar_t *path,*title,*metadata,*body; } PdfJob;
typedef struct { int success; wchar_t path[ZD_PATH_MAX]; wchar_t error[ZD_ERROR_MAX]; } PdfResult;
typedef struct { App *app; } CsvJob;
typedef struct { int success; char path[ZD_PATH_MAX]; char error[ZD_ERROR_MAX]; } CsvResult;

struct App {
    HINSTANCE instance;
    HICON app_icon_large, app_icon_small, logo_mark;
    HWND window, status, page_title, page_subtitle, mode_combo, theme_button;
    HWND sidebar_brand, sidebar_section, sidebar_note;
    HWND nav[PAGE_COUNT], controls[PAGE_COUNT][PAGE_MAX];
    int control_count[PAGE_COUNT], page, dark, busy, localizing;
    HBRUSH bg_brush, surface_brush, surface2_brush;
    HFONT font_normal, font_small, font_title, font_brand, font_bold, font_reader;
    Palette colors;
    ZDPaths *paths;
    ZDSettings *settings;
    ZDSqliteApi *sqlite;
    ZDHistoryList history;
    HWND overview_table, metric_total, metric_success, metric_issues;
    HWND download_url, download_start, download_status, download_progress, download_open;
    HWND history_search, history_status, history_date_check, history_from, history_to, history_table, history_export;
    HWND documents_search, documents_table;
    HWND reader_info, reader_search, reader_text, reader_clean, reader_browser, reader_txt, reader_pdf, reader_open_pdf;
    HWND reader_toc, reader_bookmarks, reader_add_bookmark, reader_delete_bookmark;
    HWND reader_font_down, reader_font_up, reader_copy_citation;
    ZDReaderBookmarkList reader_bookmark_list;
    int reader_font_px;
    int64_t reader_saved_position;
    ZDLocalDocumentList local_documents;
    ZDLoadedDocument loaded_document;
    wchar_t current_doc_path[ZD_PATH_MAX], current_pdf_path[ZD_PATH_MAX];
    int pdf_busy, csv_busy;
    HWND logs_text, settings_theme, settings_docs, settings_connect, settings_read,
         settings_retries, settings_level, settings_language;
    HWND about_text;
    HANDLE worker;
    HANDLE pdf_worker;
    HANDLE csv_worker;
    char last_saved_path[ZD_PATH_MAX];
};
static App *g_app;
#define UI_TEXT_PROP L"ZakonDownloader.UiSource"

static const wchar_t *tr(App *a, const wchar_t *source) {
    return zd_i18n_translate(a && a->settings ? a->settings->language : "uk", source ? source : L"");
}
static void remember_ui_text(HWND h, const wchar_t *source) {
    if (h && source && *source) SetPropW(h, UI_TEXT_PROP, (HANDLE)source);
}
static void set_ui_text(App *a, HWND h, const wchar_t *source) {
    if (!h) return;
    if (source && *source) remember_ui_text(h, source);
    SetWindowTextW(h, tr(a, source));
}
static int app_message(App *a, const wchar_t *text, const wchar_t *title, UINT flags) {
    return MessageBoxW(a ? a->window : NULL, tr(a, text), tr(a, title), flags);
}
static const wchar_t *ui_font_face(App *a) {
    return a && a->settings && strcmp(a->settings->language, "zh") == 0 ? L"Microsoft YaHei UI" : L"Segoe UI";
}
static const wchar_t *reader_font_face(App *a) {
    return a && a->settings && strcmp(a->settings->language, "zh") == 0 ? L"Microsoft YaHei UI" : L"Georgia";
}

static COLORREF hex_color(const char *hex){unsigned long v=strtoul(hex[0]=='#'?hex+1:hex,NULL,16);return RGB((v>>16)&255,(v>>8)&255,v&255);}
static Palette palette_for(int dark){Palette p;if(dark){p.bg=hex_color("#111214");p.surface=hex_color("#1C1E22");p.surface2=hex_color("#22252A");p.border=hex_color("#2E3138");p.text=hex_color("#EEECEA");p.muted=hex_color("#888890");p.accent=hex_color("#F0B400");p.accent_hover=hex_color("#FFD000");p.accent_press=hex_color("#C89500");p.accent_text=hex_color("#1A1A1A");p.success=hex_color("#3DBE75");p.error=hex_color("#E05C4B");}else{p.bg=hex_color("#F4F3EF");p.surface=hex_color("#FFFFFF");p.surface2=hex_color("#F9F8F5");p.border=hex_color("#E4E3DE");p.text=hex_color("#1A1A1A");p.muted=hex_color("#6B6B6B");p.accent=hex_color("#F0B400");p.accent_hover=hex_color("#FFD000");p.accent_press=hex_color("#C89500");p.accent_text=hex_color("#1A1A1A");p.success=hex_color("#2D7A4F");p.error=hex_color("#C0392B");}return p;}
static void repaint_status_label(App*a,HWND control,const wchar_t*text){if(!a||!control)return;SetWindowTextW(control,tr(a,text?text:L""));RedrawWindow(control,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW);}
static void set_status(App*a,const wchar_t*text){repaint_status_label(a,a?a->status:NULL,text);}
static void set_download_status(App*a,const wchar_t*text){repaint_status_label(a,a?a->download_status:NULL,text);}
static void trim_ascii_whitespace(char*text){if(!text)return;size_t first=0,last=strlen(text);while(first<last&&(text[first]==' '||text[first]=='\t'||text[first]=='\r'||text[first]=='\n'))first++;while(last>first&&(text[last-1]==' '||text[last-1]=='\t'||text[last-1]=='\r'||text[last-1]=='\n'))last--;if(first)memmove(text,text+first,last-first);text[last-first]='\0';}
static void set_download_validation(App*a,const wchar_t*message){set_download_status(a,message);set_status(a,L"");if(a&&a->download_url){SetFocus(a->download_url);SendMessageW(a->download_url,EM_SETSEL,0,-1);}}
static int utf8_wide(const char*s,wchar_t*out,size_t cap){if(!s||!out||!cap)return 0;int n=MultiByteToWideChar(CP_UTF8,0,s,-1,NULL,0);if(n<=0||(size_t)n>cap)return 0;return MultiByteToWideChar(CP_UTF8,0,s,-1,out,(int)cap)>0;}
static char *wide_utf8(const wchar_t*s){int n=WideCharToMultiByte(CP_UTF8,0,s,-1,NULL,0,NULL,NULL);if(n<=0)return NULL;char*p=(char*)malloc((size_t)n);if(!p)return NULL;if(!WideCharToMultiByte(CP_UTF8,0,s,-1,p,n,NULL,NULL)){free(p);return NULL;}return p;}
static HWND create_control(App*a,int page,const wchar_t*cls,const wchar_t*text,int id,DWORD style,DWORD ex){HWND h=CreateWindowExW(ex,cls,text&&*text?tr(a,text):L"",WS_CHILD|style,0,0,0,0,a->window,(HMENU)(INT_PTR)id,a->instance,NULL);if(h){remember_ui_text(h,text);SendMessageW(h,WM_SETFONT,(WPARAM)a->font_normal,TRUE);if(page>=0&&page<PAGE_COUNT&&a->control_count[page]<PAGE_MAX)a->controls[page][a->control_count[page]++]=h;}return h;}
static HWND label(App*a,int page,const wchar_t*text,HFONT font){HWND h=create_control(a,page,L"STATIC",text,0,SS_LEFT|SS_NOPREFIX,0);if(h&&font)SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);return h;}
static HWND button(App*a,int page,const wchar_t*text,int id){HWND h=create_control(a,page,L"BUTTON",text,id,BS_OWNERDRAW|WS_TABSTOP,0);if(h)SendMessageW(h,WM_SETFONT,(WPARAM)a->font_bold,TRUE);return h;}
static void add_columns(App*a,HWND lv,const wchar_t**names,const int*widths,int count){for(int i=0;i<count;i++){LVCOLUMNW c;memset(&c,0,sizeof(c));c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;c.pszText=(LPWSTR)tr(a,names[i]);c.cx=widths[i];c.iSubItem=i;ListView_InsertColumn(lv,i,&c);}}
static void set_date_today(HWND dtp,int days_ago){SYSTEMTIME t;GetLocalTime(&t);if(days_ago){FILETIME f;ULARGE_INTEGER u;if(SystemTimeToFileTime(&t,&f)){u.LowPart=f.dwLowDateTime;u.HighPart=f.dwHighDateTime;u.QuadPart-=(ULONGLONG)days_ago*864000000000ULL;f.dwLowDateTime=u.LowPart;f.dwHighDateTime=u.HighPart;FileTimeToSystemTime(&f,&t);}}SendMessageW(dtp,DTM_SETSYSTEMTIME,GDT_VALID,(LPARAM)&t);}
static void create_controls(App*a){
    a->font_normal=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,ui_font_face(a));
    a->font_small=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,ui_font_face(a));
    a->font_title=CreateFontW(-28,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,ui_font_face(a));
    a->font_brand=CreateFontW(-19,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,ui_font_face(a));
    a->font_bold=CreateFontW(-16,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,ui_font_face(a));
    a->font_reader=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,reader_font_face(a));
    a->page_title=CreateWindowExW(0,L"STATIC",tr(a,L"Огляд"),WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->page_title,L"Огляд");SendMessageW(a->page_title,WM_SETFONT,(WPARAM)a->font_title,TRUE);
    a->page_subtitle=CreateWindowExW(0,L"STATIC",tr(a,L"Зведення останніх операцій та швидкий доступ до завантаження."),WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->page_subtitle,L"Зведення останніх операцій та швидкий доступ до завантаження.");SendMessageW(a->page_subtitle,WM_SETFONT,(WPARAM)a->font_small,TRUE);
    a->status=CreateWindowExW(0,L"STATIC",tr(a,L"Готово."),WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->status,L"Готово.");SendMessageW(a->status,WM_SETFONT,(WPARAM)a->font_small,TRUE);
    const wchar_t*nav_text[PAGE_COUNT]={L"▦   Огляд",L"↓   Завантаження",L"▤   Мої документи",L"◷   Історія",L"▣   Перегляд",L"≡   Журнали",L"⚙   Налаштування",L"ⓘ   Про програму"};
    for(int i=0;i<PAGE_COUNT;i++){a->nav[i]=CreateWindowExW(0,L"BUTTON",tr(a,nav_text[i]),WS_CHILD|BS_OWNERDRAW|WS_TABSTOP,0,0,0,0,a->window,(HMENU)(INT_PTR)(ID_NAV_BASE+i),a->instance,NULL);remember_ui_text(a->nav[i],nav_text[i]);SendMessageW(a->nav[i],WM_SETFONT,(WPARAM)a->font_bold,TRUE);}
    a->mode_combo=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,0,0,a->window,(HMENU)ID_MODE,a->instance,NULL);SendMessageW(a->mode_combo,WM_SETFONT,(WPARAM)a->font_normal,TRUE);SendMessageW(a->mode_combo,CB_ADDSTRING,0,(LPARAM)tr(a,L"Користувач"));SendMessageW(a->mode_combo,CB_ADDSTRING,0,(LPARAM)tr(a,L"Розробник"));SendMessageW(a->mode_combo,CB_SETCURSEL,!strcmp(a->settings->ui_mode,"developer")?1:0,0);
    a->theme_button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,0,0,0,0,a->window,(HMENU)ID_THEME,a->instance,NULL);
    a->sidebar_brand=CreateWindowExW(0,L"STATIC",L"ZakonDownloader",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->sidebar_brand,L"ZakonDownloader");SendMessageW(a->sidebar_brand,WM_SETFONT,(WPARAM)a->font_small,TRUE);
    a->sidebar_section=CreateWindowExW(0,L"STATIC",tr(a,L"РОБОЧИЙ ПРОСТІР"),WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->sidebar_section,L"РОБОЧИЙ ПРОСТІР");SendMessageW(a->sidebar_section,WM_SETFONT,(WPARAM)a->font_small,TRUE);
    a->sidebar_note=CreateWindowExW(0,L"STATIC",tr(a,L"ДОКУМЕНТИ\nЗБЕРІГАЮТЬСЯ ЛОКАЛЬНО"),WS_CHILD|WS_VISIBLE|SS_LEFT|SS_NOPREFIX,0,0,0,0,a->window,NULL,a->instance,NULL);remember_ui_text(a->sidebar_note,L"ДОКУМЕНТИ\nЗБЕРІГАЮТЬСЯ ЛОКАЛЬНО");SendMessageW(a->sidebar_note,WM_SETFONT,(WPARAM)a->font_small,TRUE);

    label(a,PAGE_OVERVIEW,L"НОРМАТИВНІ ДОКУМЕНТИ УКРАЇНИ",a->font_small);
    label(a,PAGE_OVERVIEW,L"Усі потрібні документи — в одному місці.",a->font_title);
    label(a,PAGE_OVERVIEW,L"Збереження відкритих чинних редакцій у зручному текстовому форматі.",a->font_normal);
    button(a,PAGE_OVERVIEW,L"↓   Нове завантаження",ID_OV_NEW);
    label(a,PAGE_OVERVIEW,L"УСЬОГО ОПЕРАЦІЙ",a->font_small);a->metric_total=label(a,PAGE_OVERVIEW,L"0",a->font_title);
    label(a,PAGE_OVERVIEW,L"УСПІШНО",a->font_small);a->metric_success=label(a,PAGE_OVERVIEW,L"0",a->font_title);
    label(a,PAGE_OVERVIEW,L"ПОТРЕБУЮТЬ УВАГИ",a->font_small);a->metric_issues=label(a,PAGE_OVERVIEW,L"0",a->font_title);
    label(a,PAGE_OVERVIEW,L"Останні операції",a->font_brand);label(a,PAGE_OVERVIEW,L"Подвійне натискання відкриває збережений файл.",a->font_small);
    a->overview_table=create_control(a,PAGE_OVERVIEW,WC_LISTVIEWW,L"",ID_OV_TABLE,LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP,WS_EX_CLIENTEDGE);ListView_SetExtendedListViewStyle(a->overview_table,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);const wchar_t*oh[]={L"Дата",L"Документ / адреса",L"Статус",L"Результат"};int ow[]={135,360,120,300};add_columns(a,a->overview_table,oh,ow,4);

    label(a,PAGE_DOWNLOAD,L"Вставте пряме посилання на документ із zakon.rada.gov.ua.",a->font_normal);
    label(a,PAGE_DOWNLOAD,L"Посилання на документ",a->font_bold);
    a->download_url=create_control(a,PAGE_DOWNLOAD,L"EDIT",L"",ID_D_URL,ES_AUTOHSCROLL|WS_TABSTOP|ES_LEFT,WS_EX_CLIENTEDGE);button(a,PAGE_DOWNLOAD,L"Вставити",ID_D_PASTE);a->download_start=button(a,PAGE_DOWNLOAD,L"Завантажити документ",ID_D_START);
    a->download_status=label(a,PAGE_DOWNLOAD,L"Готовий до роботи.",a->font_normal);a->download_progress=create_control(a,PAGE_DOWNLOAD,PROGRESS_CLASSW,L"",0,0,0);SendMessageW(a->download_progress,PBM_SETRANGE,0,MAKELPARAM(0,4));SendMessageW(a->download_progress,PBM_SETPOS,0,0);a->download_open=button(a,PAGE_DOWNLOAD,L"Відкрити збережений файл",ID_D_OPEN);EnableWindow(a->download_open,FALSE);
    label(a,PAGE_DOWNLOAD,L"Завантажується лише загальнодоступна сторінка; обмеження сайту не обходяться.",a->font_small);

    a->documents_search=create_control(a,PAGE_DOCUMENTS,L"EDIT",L"",ID_DOC_SEARCH,ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->documents_search,EM_SETCUEBANNER,TRUE,(LPARAM)L"Пошук у локальних документах…");
    button(a,PAGE_DOCUMENTS,L"Оновити список",ID_DOC_REFRESH);a->documents_table=create_control(a,PAGE_DOCUMENTS,WC_LISTVIEWW,L"",ID_DOC_TABLE,LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP,WS_EX_CLIENTEDGE);ListView_SetExtendedListViewStyle(a->documents_table,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);const wchar_t*dh[]={L"Файл / закон",L"Змінено",L"Розмір"};int dw[]={560,190,130};add_columns(a,a->documents_table,dh,dw,3);button(a,PAGE_DOCUMENTS,L"Читати в програмі",ID_DOC_VIEW);button(a,PAGE_DOCUMENTS,L"Відкрити на сайті",ID_DOC_BROWSER);

    a->history_search=create_control(a,PAGE_HISTORY,L"EDIT",L"",ID_H_SEARCH,ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->history_search,EM_SETCUEBANNER,TRUE,(LPARAM)L"Пошук за URL, назвою або статусом…");
    a->history_status=create_control(a,PAGE_HISTORY,L"COMBOBOX",L"",ID_H_STATUS,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);const wchar_t*statuses[]={L"Усі статуси",L"Розпочато",L"Успішно",L"Помилка",L"Пропущено"};for(int i=0;i<5;i++)SendMessageW(a->history_status,CB_ADDSTRING,0,(LPARAM)tr(a,statuses[i]));SendMessageW(a->history_status,CB_SETCURSEL,0,0);
    a->history_date_check=create_control(a,PAGE_HISTORY,L"BUTTON",L"Період",ID_H_DATE_CHECK,BS_AUTOCHECKBOX|WS_TABSTOP,0);
    a->history_from=create_control(a,PAGE_HISTORY,L"SysDateTimePick32",L"",ID_H_FROM,DTS_SHORTDATEFORMAT|WS_TABSTOP,WS_EX_CLIENTEDGE);a->history_to=create_control(a,PAGE_HISTORY,L"SysDateTimePick32",L"",ID_H_TO,DTS_SHORTDATEFORMAT|WS_TABSTOP,WS_EX_CLIENTEDGE);set_date_today(a->history_from,30);set_date_today(a->history_to,0);EnableWindow(a->history_from,FALSE);EnableWindow(a->history_to,FALSE);
    button(a,PAGE_HISTORY,L"Оновити",ID_H_REFRESH);a->history_table=create_control(a,PAGE_HISTORY,WC_LISTVIEWW,L"",ID_H_TABLE,LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP,WS_EX_CLIENTEDGE);ListView_SetExtendedListViewStyle(a->history_table,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);const wchar_t*hh[]={L"ID",L"Запущено",L"Завершено",L"URL",L"Назва документа",L"Статус",L"Файл"};int hw[]={60,140,140,250,260,110,260};add_columns(a,a->history_table,hh,hw,7);button(a,PAGE_HISTORY,L"Читати",ID_H_VIEW);button(a,PAGE_HISTORY,L"На сайті",ID_H_BROWSER);button(a,PAGE_HISTORY,L"Відкрити TXT",ID_H_OPEN);button(a,PAGE_HISTORY,L"Папка",ID_H_FOLDER);a->history_export=button(a,PAGE_HISTORY,L"Експорт CSV",ID_H_EXPORT);

    a->reader_info=label(a,PAGE_READER,L"Оберіть закон у розділі «Мої документи» або «Історія».",a->font_bold);
    a->reader_search=create_control(a,PAGE_READER,L"EDIT",L"",ID_R_SEARCH,ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->reader_search,EM_SETCUEBANNER,TRUE,(LPARAM)L"Знайти фразу в тексті…");button(a,PAGE_READER,L"Знайти далі",ID_R_NEXT);
    a->reader_clean=create_control(a,PAGE_READER,L"BUTTON",L"Показувати лише чистий текст закону",ID_R_CLEAN,BS_AUTOCHECKBOX|WS_TABSTOP,0);SendMessageW(a->reader_clean,BM_SETCHECK,BST_CHECKED,0);
    a->reader_browser=button(a,PAGE_READER,L"Відкрити оригінал у браузері",ID_R_BROWSER);a->reader_txt=button(a,PAGE_READER,L"Відкрити TXT",ID_R_TXT);a->reader_pdf=button(a,PAGE_READER,L"Експортувати PDF",ID_R_PDF);a->reader_open_pdf=button(a,PAGE_READER,L"Відкрити PDF",ID_R_OPEN_PDF);EnableWindow(a->reader_browser,FALSE);EnableWindow(a->reader_txt,FALSE);EnableWindow(a->reader_pdf,FALSE);EnableWindow(a->reader_open_pdf,FALSE);a->reader_font_down=button(a,PAGE_READER,L"A−",ID_R_FONT_DOWN);a->reader_font_up=button(a,PAGE_READER,L"A+",ID_R_FONT_UP);a->reader_copy_citation=button(a,PAGE_READER,L"Копіювати цитату",ID_R_COPY_CITATION);
    a->reader_toc=create_control(a,PAGE_READER,L"COMBOBOX",L"",ID_R_TOC,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->reader_toc,CB_ADDSTRING,0,(LPARAM)tr(a,L"Зміст — розділи та статті"));SendMessageW(a->reader_toc,CB_SETCURSEL,0,0);EnableWindow(a->reader_toc,FALSE);a->reader_bookmarks=create_control(a,PAGE_READER,L"COMBOBOX",L"",ID_R_BOOKMARKS,CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->reader_bookmarks,CB_ADDSTRING,0,(LPARAM)tr(a,L"Мої закладки"));SendMessageW(a->reader_bookmarks,CB_SETCURSEL,0,0);EnableWindow(a->reader_bookmarks,FALSE);a->reader_add_bookmark=button(a,PAGE_READER,L"Закладка +",ID_R_ADD_BOOKMARK);a->reader_delete_bookmark=button(a,PAGE_READER,L"Видалити",ID_R_DELETE_BOOKMARK);EnableWindow(a->reader_add_bookmark,FALSE);EnableWindow(a->reader_delete_bookmark,FALSE);
    a->reader_text=create_control(a,PAGE_READER,L"RICHEDIT50W",L"",ID_R_TEXT,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP|ES_NOHIDESEL,WS_EX_CLIENTEDGE);if(!a->reader_text)a->reader_text=create_control(a,PAGE_READER,L"EDIT",L"",ID_R_TEXT,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP|ES_NOHIDESEL,WS_EX_CLIENTEDGE);if(a->reader_text){SendMessageW(a->reader_text,WM_SETFONT,(WPARAM)a->font_reader,TRUE);SendMessageW(a->reader_text,EM_EXLIMITTEXT,0,(LPARAM)(ZD_LOCAL_TEXT_LIMIT/2));}

    a->logs_text=create_control(a,PAGE_LOGS,L"EDIT",L"",ID_L_TEXT,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);button(a,PAGE_LOGS,L"Оновити журнал",ID_L_REFRESH);button(a,PAGE_LOGS,L"Експортувати всі журнали",ID_L_EXPORT);button(a,PAGE_LOGS,L"Відкрити папку журналів",ID_L_FOLDER);

    label(a,PAGE_SETTINGS,L"Вигляд і збереження",a->font_brand);label(a,PAGE_SETTINGS,L"Тема інтерфейсу",a->font_normal);a->settings_theme=create_control(a,PAGE_SETTINGS,L"COMBOBOX",L"",ID_S_THEME,CBS_DROPDOWNLIST|WS_TABSTOP,WS_EX_CLIENTEDGE);SendMessageW(a->settings_theme,CB_ADDSTRING,0,(LPARAM)tr(a,L"Світла"));SendMessageW(a->settings_theme,CB_ADDSTRING,0,(LPARAM)tr(a,L"Темна"));label(a,PAGE_SETTINGS,L"Мова інтерфейсу",a->font_normal);a->settings_language=create_control(a,PAGE_SETTINGS,L"COMBOBOX",L"",ID_S_LANGUAGE,CBS_DROPDOWNLIST|WS_TABSTOP,WS_EX_CLIENTEDGE);size_t locale_count=0;const ZDLocale*locale_list=zd_i18n_locales(&locale_count);for(size_t li=0;li<locale_count;li++)SendMessageW(a->settings_language,CB_ADDSTRING,0,(LPARAM)locale_list[li].native_name);SendMessageW(a->settings_language,CB_SETCURSEL,zd_i18n_index(a->settings->language),0);
    label(a,PAGE_SETTINGS,L"Папка документів",a->font_normal);a->settings_docs=create_control(a,PAGE_SETTINGS,L"EDIT",L"",ID_S_DOCS,ES_AUTOHSCROLL|WS_TABSTOP,WS_EX_CLIENTEDGE);button(a,PAGE_SETTINGS,L"Огляд…",ID_S_BROWSE);
    label(a,PAGE_SETTINGS,L"Мережа та повторні спроби",a->font_brand);label(a,PAGE_SETTINGS,L"Таймаут підключення (сек.)",a->font_normal);a->settings_connect=create_control(a,PAGE_SETTINGS,L"EDIT",L"",ID_S_CONNECT,ES_NUMBER|WS_TABSTOP,WS_EX_CLIENTEDGE);label(a,PAGE_SETTINGS,L"Таймаут читання (сек.)",a->font_normal);a->settings_read=create_control(a,PAGE_SETTINGS,L"EDIT",L"",ID_S_READ,ES_NUMBER|WS_TABSTOP,WS_EX_CLIENTEDGE);label(a,PAGE_SETTINGS,L"Максимум спроб",a->font_normal);a->settings_retries=create_control(a,PAGE_SETTINGS,L"EDIT",L"",ID_S_RETRIES,ES_NUMBER|WS_TABSTOP,WS_EX_CLIENTEDGE);
    label(a,PAGE_SETTINGS,L"Журналювання",a->font_brand);label(a,PAGE_SETTINGS,L"Рівень журналу",a->font_normal);a->settings_level=create_control(a,PAGE_SETTINGS,L"COMBOBOX",L"",ID_S_LEVEL,CBS_DROPDOWNLIST|WS_TABSTOP,WS_EX_CLIENTEDGE);const wchar_t*levels[]={L"Інформація (INFO)",L"Попередження (WARNING)",L"Помилки (ERROR)",L"Налагодження (DEBUG)"};for(int i=0;i<4;i++)SendMessageW(a->settings_level,CB_ADDSTRING,0,(LPARAM)tr(a,levels[i]));button(a,PAGE_SETTINGS,L"Типові значення",ID_S_DEFAULTS);button(a,PAGE_SETTINGS,L"Зберегти налаштування",ID_S_SAVE);

    a->about_text=label(a,PAGE_ABOUT,L"",a->font_normal);button(a,PAGE_ABOUT,L"Відкрити папку документів",ID_A_OPEN_DOCS);button(a,PAGE_ABOUT,L"Відкрити папку даних",ID_A_OPEN_DATA);button(a,PAGE_ABOUT,L"Сайт Верховної Ради України",ID_A_OPEN_SITE);
    for(int i=0;i<PAGE_COUNT;i++)for(int j=0;j<a->control_count[i];j++)ShowWindow(a->controls[i][j],SW_HIDE);
    SendMessageW(a->settings_theme,CB_SETCURSEL,!strcmp(a->settings->theme,"dark")?1:0,0);SetWindowTextW(a->settings_docs,L"");wchar_t temp[ZD_PATH_MAX];if(utf8_wide(a->settings->documents_dir,temp,ZD_PATH_MAX))SetWindowTextW(a->settings_docs,temp);wchar_t number[32];
    char num[32];snprintf(num,sizeof(num),"%d",a->settings->request_timeout_connect);utf8_wide(num,number,32);SetWindowTextW(a->settings_connect,number);snprintf(num,sizeof(num),"%d",a->settings->request_timeout_read);utf8_wide(num,number,32);SetWindowTextW(a->settings_read,number);snprintf(num,sizeof(num),"%d",a->settings->max_retry_attempts);utf8_wide(num,number,32);SetWindowTextW(a->settings_retries,number);int lv=!strcmp(a->settings->log_level,"WARNING")?1:!strcmp(a->settings->log_level,"ERROR")?2:!strcmp(a->settings->log_level,"DEBUG")?3:0;SendMessageW(a->settings_level,CB_SETCURSEL,lv,0);
}
static void apply_theme(App*a){
    a->dark=!strcmp(a->settings->theme,"dark");
    a->colors=palette_for(a->dark);
    if(a->bg_brush)DeleteObject(a->bg_brush);
    if(a->surface_brush)DeleteObject(a->surface_brush);
    if(a->surface2_brush)DeleteObject(a->surface2_brush);
    a->bg_brush=CreateSolidBrush(a->colors.bg);
    a->surface_brush=CreateSolidBrush(a->colors.surface);
    a->surface2_brush=CreateSolidBrush(a->colors.surface2);
    set_ui_text(a,a->theme_button,a->dark?L"☼  Світла тема":L"☾  Темна тема");
    if(a->overview_table){ListView_SetBkColor(a->overview_table,a->colors.surface);ListView_SetTextBkColor(a->overview_table,a->colors.surface);ListView_SetTextColor(a->overview_table,a->colors.text);}
    if(a->history_table){ListView_SetBkColor(a->history_table,a->colors.surface);ListView_SetTextBkColor(a->history_table,a->colors.surface);ListView_SetTextColor(a->history_table,a->colors.text);}
    if(a->documents_table){ListView_SetBkColor(a->documents_table,a->colors.surface);ListView_SetTextBkColor(a->documents_table,a->colors.surface);ListView_SetTextColor(a->documents_table,a->colors.text);}
    if(a->reader_text){SendMessageW(a->reader_text,EM_SETBKGNDCOLOR,0,a->colors.surface);CHARFORMAT2W cf;memset(&cf,0,sizeof(cf));cf.cbSize=sizeof(cf);cf.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE;cf.crTextColor=a->colors.text;cf.yHeight=(a->reader_font_px>0?a->reader_font_px*15:270);wcscpy(cf.szFaceName,reader_font_face(a));SendMessageW(a->reader_text,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);PARAFORMAT2 pf;memset(&pf,0,sizeof(pf));pf.cbSize=sizeof(pf);pf.dwMask=PFM_STARTINDENT|PFM_RIGHTINDENT|PFM_SPACEAFTER;pf.dxStartIndent=260;pf.dxRightIndent=260;pf.dySpaceAfter=100;SendMessageW(a->reader_text,EM_SETPARAFORMAT,0,(LPARAM)&pf);}
    if(a->window)RedrawWindow(a->window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void load_settings_controls(App*a){SendMessageW(a->settings_theme,CB_SETCURSEL,!strcmp(a->settings->theme,"dark")?1:0,0);SendMessageW(a->settings_language,CB_SETCURSEL,zd_i18n_index(a->settings->language),0);wchar_t w[ZD_PATH_MAX];if(utf8_wide(a->settings->documents_dir,w,ZD_PATH_MAX))SetWindowTextW(a->settings_docs,w);char num[32];wchar_t wn[32];snprintf(num,sizeof(num),"%d",a->settings->request_timeout_connect);utf8_wide(num,wn,32);SetWindowTextW(a->settings_connect,wn);snprintf(num,sizeof(num),"%d",a->settings->request_timeout_read);utf8_wide(num,wn,32);SetWindowTextW(a->settings_read,wn);snprintf(num,sizeof(num),"%d",a->settings->max_retry_attempts);utf8_wide(num,wn,32);SetWindowTextW(a->settings_retries,wn);int lv=!strcmp(a->settings->log_level,"WARNING")?1:!strcmp(a->settings->log_level,"ERROR")?2:!strcmp(a->settings->log_level,"DEBUG")?3:0;SendMessageW(a->settings_level,CB_SETCURSEL,lv,0);}
static void refresh_logs(App*a){char*text=zd_log_read_recent(a->paths,512000);wchar_t*w=text?NULL:NULL;if(text){int n=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0);if(n>0){w=(wchar_t*)malloc((size_t)n*sizeof(*w));if(w)MultiByteToWideChar(CP_UTF8,0,text,-1,w,n);}}SetWindowTextW(a->logs_text,w?w:(LPWSTR)tr(a,L"Журнал поки порожній."));if(w)free(w);free(text);}
static const ZDHistoryEntry *find_entry(App*a,int64_t id){for(size_t i=0;i<a->history.count;i++)if(a->history.items[i].id==id)return &a->history.items[i];return NULL;}
static int date_string(const char*s,char*out,size_t cap){if(!s||strlen(s)<10)return snprintf(out,cap,"—")>0;return snprintf(out,cap,"%.2s.%.2s.%.4s %.5s",s+8,s+5,s,s+11)>0;}
static void lv_set_text(HWND lv,int row,int col,const char*text){wchar_t w[ZD_PATH_MAX<8192?ZD_PATH_MAX:8192];if(!utf8_wide(text?text:"",w,sizeof(w)/sizeof(w[0])))wcscpy(w,L"");ListView_SetItemText(lv,row,col,w);}
static void lv_translate_cell(App*a,HWND lv,int row,int col,const char*text){wchar_t w[ZD_PATH_MAX<8192?ZD_PATH_MAX:8192];if(!utf8_wide(text?text:"",w,sizeof(w)/sizeof(w[0])))return;ListView_SetItemText(lv,row,col,(LPWSTR)tr(a,w));}
static int lv_add_row(HWND lv,int row,int64_t id,const char**values,int columns){wchar_t w[ZD_PATH_MAX<8192?ZD_PATH_MAX:8192];LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row;item.iSubItem=0;item.lParam=(LPARAM)id;if(!utf8_wide(values[0]?values[0]:"",w,sizeof(w)/sizeof(w[0])))wcscpy(w,L"");item.pszText=w;int i=ListView_InsertItem(lv,&item);if(i<0)return 0;for(int c=1;c<columns;c++)lv_set_text(lv,i,c,values[c]);return 1;}
static void refresh_history(App*a){ZDHistoryList fresh={0};char err[ZD_ERROR_MAX];if(!zd_history_load(a->sqlite,a->paths->history_db,&fresh,err,sizeof(err))){wchar_t w[ZD_ERROR_MAX];utf8_wide(err,w,ZD_ERROR_MAX);set_status(a,w);return;}zd_history_free(&a->history);a->history=fresh;}
static void fill_dashboard(App*a){
    size_t ok=0,issues=0;
    for(size_t i=0;i<a->history.count;i++){
        const char*s=a->history.items[i].status;
        if(s&&!strcmp(s,"Успішно"))ok++;
        else if(s&&(!strcmp(s,"Помилка")||!strcmp(s,"Пропущено")))issues++;
    }
    wchar_t n[32];
    swprintf(n,32,L"%llu",(unsigned long long)a->history.count);SetWindowTextW(a->metric_total,n);
    swprintf(n,32,L"%llu",(unsigned long long)ok);SetWindowTextW(a->metric_success,n);
    swprintf(n,32,L"%llu",(unsigned long long)issues);SetWindowTextW(a->metric_issues,n);
    ListView_DeleteAllItems(a->overview_table);
    size_t shown=0;
    for(size_t i=0;i<a->history.count&&shown<6;i++){
        ZDHistoryEntry*e=&a->history.items[i];
        char date[40];date_string(e->started_at,date,sizeof(date));
        const char*title=e->document_title&&*e->document_title?e->document_title:e->url;
        const char*result=e->saved_path&&*e->saved_path?"Збережено":e->error_message&&*e->error_message?e->error_message:"—";
        const char*row[]={date,title,e->status?e->status:"—",result};
        int row_index=(int)shown;if(lv_add_row(a->overview_table,row_index,e->id,row,4)){lv_translate_cell(a,a->overview_table,row_index,2,row[2]);lv_translate_cell(a,a->overview_table,row_index,3,row[3]);shown++;}
    }
    if(!shown){const char*row[]={"—","Історія поки порожня. Розпочніть із першого завантаження.","—","—"};if(lv_add_row(a->overview_table,0,0,row,4))lv_translate_cell(a,a->overview_table,0,1,row[1]);}
}
static int contains_w_ci(const wchar_t*hay,const wchar_t*needle){if(!hay||!needle||!*needle)return 1;size_t hn=wcslen(hay),nn=wcslen(needle);if(nn>hn)return 0;for(size_t i=0;i<=hn-nn;i++)if(CompareStringOrdinal(hay+i,(int)nn,needle,(int)nn,TRUE)==CSTR_EQUAL)return 1;return 0;}
static void render_local_documents(App*a){
    if(!a->documents_table)return;
    wchar_t query[512];GetWindowTextW(a->documents_search,query,512);query[511]=0;
    SendMessageW(a->documents_table,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(a->documents_table);int row=0;
    for(size_t i=0;i<a->local_documents.count;i++){
        ZDLocalDocument*d=&a->local_documents.items[i];if(query[0]&&!contains_w_ci(d->name,query))continue;
        SYSTEMTIME local;FILETIME ft;wchar_t date[32],size[48];
        if(FileTimeToLocalFileTime(&d->modified,&ft)&&FileTimeToSystemTime(&ft,&local))swprintf(date,32,L"%02u.%02u.%04u",local.wDay,local.wMonth,local.wYear);else wcscpy(date,L"—");
        if(d->bytes<1024)swprintf(size,48,tr(a,L"%llu Б"),(unsigned long long)d->bytes);else if(d->bytes<1024*1024)swprintf(size,48,tr(a,L"%llu КБ"),(unsigned long long)((d->bytes+512)/1024));else swprintf(size,48,tr(a,L"%llu МБ"),(unsigned long long)((d->bytes+524288)/(1024*1024)));
        LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row;item.iSubItem=0;item.lParam=(LPARAM)i;item.pszText=d->name;int inserted=ListView_InsertItem(a->documents_table,&item);if(inserted>=0){ListView_SetItemText(a->documents_table,inserted,1,date);ListView_SetItemText(a->documents_table,inserted,2,size);row++;}
    }
    if(!row){LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=0;item.lParam=(LPARAM)-1;item.pszText=(LPWSTR)tr(a,a->local_documents.count?L"Немає збігів із пошуком.":L"Папка поки порожня — завантажте закон або оберіть папку в налаштуваннях.");ListView_InsertItem(a->documents_table,&item);}
    SendMessageW(a->documents_table,WM_SETREDRAW,TRUE,0);InvalidateRect(a->documents_table,NULL,TRUE);
    wchar_t status[96];swprintf(status,96,tr(a,L"Локальних документів: %llu"),(unsigned long long)a->local_documents.count);set_status(a,status);
}
static void refresh_local_documents(App*a){
    if(!a->documents_table)return;
    wchar_t folder[ZD_PATH_MAX];char error[ZD_ERROR_MAX];
    if(!utf8_wide(a->settings->documents_dir,folder,ZD_PATH_MAX)){set_status(a,L"Не вдалося прочитати шлях до папки документів.");return;}
    if(!zd_local_documents_load(folder,&a->local_documents,error,sizeof(error))){wchar_t w[ZD_ERROR_MAX];utf8_wide(error,w,ZD_ERROR_MAX);set_status(a,w);return;}
    render_local_documents(a);
}static void get_search_text(HWND edit,wchar_t*out,size_t cap){GetWindowTextW(edit,out,(int)cap);out[cap-1]=0;}
static int matches_history(App*a,const ZDHistoryEntry*e,const wchar_t*query,int status_index,const char*from,const char*to){(void)a;if(status_index>0){wchar_t status[128];if(!utf8_wide(e->status?e->status:"",status,128))return 0;const wchar_t*names[]={L"",L"Розпочато",L"Успішно",L"Помилка",L"Пропущено"};if(_wcsicmp(status,names[status_index])!=0)return 0;}if(from&&to&&e->started_at&&strlen(e->started_at)>=10){if(strncmp(e->started_at,from,10)<0||strncmp(e->started_at,to,10)>0)return 0;}if(query&&*query){const char*fields[]={e->url,e->document_title,e->status,e->error_message,e->saved_path};int found=0;for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++){wchar_t w[ZD_PATH_MAX<8192?ZD_PATH_MAX:8192];if(fields[i]&&utf8_wide(fields[i],w,sizeof(w)/sizeof(w[0]))&&contains_w_ci(w,query)){found=1;break;}}if(!found)return 0;}return 1;}
static void systemtime_date(const SYSTEMTIME*t,char out[11]){snprintf(out,11,"%04u-%02u-%02u",t->wYear,t->wMonth,t->wDay);}
static void fill_history_table(App*a){if(!a->history_table)return;wchar_t query[512];get_search_text(a->history_search,query,512);int status=(int)SendMessageW(a->history_status,CB_GETCURSEL,0,0);int checked=(int)SendMessageW(a->history_date_check,BM_GETCHECK,0,0)==BST_CHECKED;char from[11]="",to[11]="";if(checked){SYSTEMTIME f,t;DateTime_GetSystemtime(a->history_from,&f);DateTime_GetSystemtime(a->history_to,&t);systemtime_date(&f,from);systemtime_date(&t,to);if(strcmp(from,to)>0){char temp[11];strcpy(temp,from);strcpy(from,to);strcpy(to,temp);}}ListView_DeleteAllItems(a->history_table);int row=0;for(size_t i=0;i<a->history.count;i++){ZDHistoryEntry*e=&a->history.items[i];if(!matches_history(a,e,query,status,checked?from:NULL,checked?to:NULL))continue;char id[32],started[40],finished[40];snprintf(id,sizeof(id),"%lld",(long long)e->id);date_string(e->started_at,started,sizeof(started));date_string(e->finished_at,finished,sizeof(finished));const char*values[]={id,started,finished,e->url?e->url:"",e->document_title?e->document_title:"",e->status?e->status:"",e->saved_path?e->saved_path:""};if(lv_add_row(a->history_table,row,e->id,values,7)){lv_translate_cell(a,a->history_table,row,5,values[5]);row++;}}}
static int selected_id(HWND list,int64_t*out){int row=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(row<0)return 0;LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_PARAM;item.iItem=row;if(!ListView_GetItem(list,&item))return 0;*out=(int64_t)item.lParam;return *out>0;}
static int open_file_path(App*a,const char*path,int folder){if(!path||!*path)return 0;char target[ZD_PATH_MAX];if(folder){snprintf(target,sizeof(target),"%s",path);char*p=strrchr(target,'\\');char*q=strrchr(target,'/');if(q&&(!p||q>p))p=q;if(p)*p=0;else return 0;path=target;}wchar_t*w=NULL;int n=MultiByteToWideChar(CP_UTF8,0,path,-1,NULL,0);if(n>0){w=(wchar_t*)malloc((size_t)n*sizeof(*w));if(w)MultiByteToWideChar(CP_UTF8,0,path,-1,w,n);}if(!w)return 0;INT_PTR rc=(INT_PTR)ShellExecuteW(a->window,L"open",w,NULL,NULL,SW_SHOWNORMAL);free(w);return rc>32;}
static void show_page(App*a,int page);
static int derive_pdf_path(const wchar_t*txt,wchar_t*out,size_t cap){size_t n=wcslen(txt);if(n+5>cap)return 0;memcpy(out,txt,(n+1)*sizeof(*out));if(n>=4&&_wcsicmp(out+n-4,L".txt")==0){wcscpy(out+n-4,L".pdf");return 1;}wcscpy(out+n,L".pdf");return 1;}
static int64_t reader_current_position(App*a){
    if(!a->reader_text||!a->loaded_document.full_text)return 0;DWORD start=0,end=0;SendMessageW(a->reader_text,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);int64_t pos=(int64_t)start;
    if(SendMessageW(a->reader_clean,BM_GETCHECK,0,0)==BST_CHECKED&&a->loaded_document.clean_text)pos+=(int64_t)(a->loaded_document.clean_text-a->loaded_document.full_text);
    return pos<0?0:pos;
}
static void reader_set_position(App*a,int64_t absolute){
    if(!a->reader_text||!a->loaded_document.full_text)return;int64_t offset=absolute;
    if(SendMessageW(a->reader_clean,BM_GETCHECK,0,0)==BST_CHECKED&&a->loaded_document.clean_text){int64_t base=(int64_t)(a->loaded_document.clean_text-a->loaded_document.full_text);offset=absolute>=base?absolute-base:0;}
    size_t length=wcslen(SendMessageW(a->reader_clean,BM_GETCHECK,0,0)==BST_CHECKED?a->loaded_document.clean_text:a->loaded_document.full_text);if(offset>(int64_t)length)offset=(int64_t)length;
    SendMessageW(a->reader_text,EM_SETSEL,(WPARAM)offset,(LPARAM)offset);SendMessageW(a->reader_text,EM_SCROLLCARET,0,0);
}
static void reader_save_progress(App*a){
    if(!a||!a->loaded_document.full_text||!a->current_doc_path[0])return;int64_t position=reader_current_position(a);if(position==a->reader_saved_position)return;char*file=wide_utf8(a->current_doc_path);if(!file)return;char error[ZD_ERROR_MAX];if(zd_reader_progress_put(a->sqlite,a->paths->history_db,file,position,error,sizeof(error)))a->reader_saved_position=position;else zd_log_write(a->paths,"WARNING","reader","Не вдалося зберегти позицію читання: %s",error);free(file);
}
static int reader_heading_line(const wchar_t*line,size_t length){while(length&&(*line==L' '||*line==L'\t')){line++;length--;}static const wchar_t*prefixes[]={L"Стаття",L"Розділ",L"Підрозділ",L"Глава",L"Частина",L"Прикінцеві положення",L"Перехідні положення"};for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]);i++){size_t n=wcslen(prefixes[i]);if(length>=n&&_wcsnicmp(line,prefixes[i],n)==0)return 1;}return 0;}
static void reader_fill_toc(App*a){
    if(!a->reader_toc)return;SendMessageW(a->reader_toc,CB_RESETCONTENT,0,0);SendMessageW(a->reader_toc,CB_ADDSTRING,0,(LPARAM)tr(a,L"Зміст — розділи та статті"));
    const wchar_t*text=a->loaded_document.clean_text;if(text){size_t len=wcslen(text),start=0;int added=0;while(start<len&&added<1000){size_t end=start;while(end<len&&text[end]!=L'\n'&&text[end]!=L'\r')end++;if(reader_heading_line(text+start,end-start)){size_t first=start;while(first<end&&(text[first]==L' '||text[first]==L'\t'))first++;size_t last=end;while(last>first&&(text[last-1]==L' '||text[last-1]==L'\t'))last--;size_t n=last-first;if(n>240)n=240;if(n){wchar_t heading[256];memcpy(heading,text+first,n*sizeof(wchar_t));heading[n]=0;if(SendMessageW(a->reader_toc,CB_ADDSTRING,0,(LPARAM)heading)>=0)added++;}}start=end;while(start<len&&(text[start]==L'\n'||text[start]==L'\r'))start++;}}
    SendMessageW(a->reader_toc,CB_SETCURSEL,0,0);EnableWindow(a->reader_toc,SendMessageW(a->reader_toc,CB_GETCOUNT,0,0)>1);
}
static void reader_fill_bookmarks(App*a){
    if(!a->reader_bookmarks||!a->current_doc_path[0])return;char*file=wide_utf8(a->current_doc_path);if(!file)return;char error[ZD_ERROR_MAX];if(!zd_reader_bookmarks_load(a->sqlite,a->paths->history_db,file,&a->reader_bookmark_list,error,sizeof(error))){free(file);set_status(a,L"Не вдалося прочитати закладки.");return;}free(file);
    SendMessageW(a->reader_bookmarks,CB_RESETCONTENT,0,0);SendMessageW(a->reader_bookmarks,CB_ADDSTRING,0,(LPARAM)tr(a,L"Мої закладки"));
    for(size_t i=0;i<a->reader_bookmark_list.count;i++){ZDReaderBookmark*b=&a->reader_bookmark_list.items[i];wchar_t label[512],wide[400];if(!utf8_wide(b->label,wide,400))wcscpy(wide,(LPWSTR)tr(a,L"Закладка"));swprintf(label,512,tr(a,L"%ls  ·  %lld"),wide,(long long)b->position);LRESULT index=SendMessageW(a->reader_bookmarks,CB_ADDSTRING,0,(LPARAM)label);if(index>=0)SendMessageW(a->reader_bookmarks,CB_SETITEMDATA,(WPARAM)index,(LPARAM)b->position);}
    SendMessageW(a->reader_bookmarks,CB_SETCURSEL,0,0);EnableWindow(a->reader_bookmarks,a->reader_bookmark_list.count>0);EnableWindow(a->reader_add_bookmark,TRUE);EnableWindow(a->reader_delete_bookmark,a->reader_bookmark_list.count>0);
}
static void reader_jump_toc(App*a){int index=(int)SendMessageW(a->reader_toc,CB_GETCURSEL,0,0);if(index<=0)return;wchar_t heading[512];if(SendMessageW(a->reader_toc,CB_GETLBTEXT,index,(LPARAM)heading)==CB_ERR)return;FINDTEXTW find;find.chrg.cpMin=0;find.chrg.cpMax=-1;find.lpstrText=heading;LRESULT pos=SendMessageW(a->reader_text,EM_FINDTEXTW,FR_DOWN,(LPARAM)&find);if(pos>=0){SendMessageW(a->reader_text,EM_SETSEL,(WPARAM)pos,(LPARAM)(pos+wcslen(heading)));SendMessageW(a->reader_text,EM_SCROLLCARET,0,0);set_status(a,L"Перейшов до вибраного розділу або статті.");}}
static void reader_jump_bookmark(App*a){int index=(int)SendMessageW(a->reader_bookmarks,CB_GETCURSEL,0,0);if(index<=0)return;LRESULT position=SendMessageW(a->reader_bookmarks,CB_GETITEMDATA,(WPARAM)index,0);if(position>=0){reader_set_position(a,(int64_t)position);set_status(a,L"Відкрито збережену закладку.");}}
static void reader_bookmark_add(App*a){if(!a->loaded_document.full_text)return;int64_t position=reader_current_position(a);const wchar_t*text=a->loaded_document.full_text;size_t len=wcslen(text),start=(size_t)(position<(int64_t)len?position:(int64_t)len);while(start&&text[start-1]!=L'\n'&&text[start-1]!=L'\r')start--;size_t end=start;while(end<len&&text[end]!=L'\n'&&text[end]!=L'\r')end++;while(start<end&&(text[start]==L' '||text[start]==L'\t'))start++;while(end>start&&(text[end-1]==L' '||text[end-1]==L'\t'))end--;size_t count=end-start;if(count>160)count=160;wchar_t label[192];if(count){memcpy(label,text+start,count*sizeof(wchar_t));label[count]=0;}else wcscpy(label,(LPWSTR)tr(a,L"Місце читання"));char*file=wide_utf8(a->current_doc_path);char*utf8_label=wide_utf8(label);if(!file||!utf8_label){free(file);free(utf8_label);set_status(a,L"Не вдалося підготувати закладку.");return;}char error[ZD_ERROR_MAX];int ok=zd_reader_bookmark_put(a->sqlite,a->paths->history_db,file,position,utf8_label,error,sizeof(error));free(file);free(utf8_label);if(!ok){set_status(a,L"Не вдалося зберегти закладку.");return;}reader_fill_bookmarks(a);for(int i=1;i<(int)SendMessageW(a->reader_bookmarks,CB_GETCOUNT,0,0);i++)if((int64_t)SendMessageW(a->reader_bookmarks,CB_GETITEMDATA,(WPARAM)i,0)==position){SendMessageW(a->reader_bookmarks,CB_SETCURSEL,i,0);break;}set_status(a,L"Закладку збережено для цього документа.");}
static void reader_bookmark_delete(App*a){int index=(int)SendMessageW(a->reader_bookmarks,CB_GETCURSEL,0,0);if(index<=0){set_status(a,L"Оберіть закладку для видалення.");return;}LRESULT value=SendMessageW(a->reader_bookmarks,CB_GETITEMDATA,(WPARAM)index,0);if(value<0)return;char*file=wide_utf8(a->current_doc_path);if(!file)return;char error[ZD_ERROR_MAX];int ok=zd_reader_bookmark_delete(a->sqlite,a->paths->history_db,file,(int64_t)value,error,sizeof(error));free(file);if(ok){reader_fill_bookmarks(a);set_status(a,L"Закладку видалено.");}else set_status(a,L"Не вдалося видалити закладку.");}
static void reader_change_font(App*a,int delta){int next=a->reader_font_px+delta*2;if(next<14)next=14;if(next>30)next=30;if(next==a->reader_font_px)return;HFONT font=CreateFontW(-next,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,reader_font_face(a));if(!font){set_status(a,L"Не вдалося змінити шрифт читання.");return;}HFONT old=a->font_reader;a->font_reader=font;a->reader_font_px=next;SendMessageW(a->reader_text,WM_SETFONT,(WPARAM)font,TRUE);CHARFORMAT2W cf;memset(&cf,0,sizeof(cf));cf.cbSize=sizeof(cf);cf.dwMask=CFM_FACE|CFM_SIZE;cf.yHeight=next*15;wcscpy(cf.szFaceName,reader_font_face(a));SendMessageW(a->reader_text,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&cf);if(old)DeleteObject(old);wchar_t msg[80];swprintf(msg,80,tr(a,L"Розмір шрифту читання: %d px."),next);set_status(a,msg);}
static void reader_copy_citation(App*a){
    if(!a->loaded_document.full_text){set_status(a,L"Спочатку відкрийте локальний документ.");return;}DWORD start=0,end=0;SendMessageW(a->reader_text,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);if(end<=start){set_status(a,L"Виділіть уривок тексту, який потрібно процитувати.");return;}if(end-start>20000){set_status(a,L"Для цитати виділіть коротший уривок (до 20 000 символів).");return;}
    size_t selected=(size_t)(end-start);wchar_t*quote=(wchar_t*)calloc(selected+1,sizeof(wchar_t));if(!quote){set_status(a,L"Недостатньо пам'яті для цитати.");return;}SendMessageW(a->reader_text,EM_GETSELTEXT,0,(LPARAM)quote);wchar_t source[ZD_URL_MAX];if(a->loaded_document.source_url[0])utf8_wide(a->loaded_document.source_url,source,ZD_URL_MAX);else wcscpy(source,a->current_doc_path);size_t cap=selected+wcslen(a->loaded_document.title)+wcslen(source)+128;wchar_t*out=(wchar_t*)calloc(cap,sizeof(wchar_t));if(!out){free(quote);set_status(a,L"Недостатньо пам'яті для цитати.");return;}swprintf(out,cap,tr(a,L"%ls\r\n\r\n— %ls\r\nДжерело: %ls"),quote,a->loaded_document.title,source);free(quote);SIZE_T bytes=(wcslen(out)+1)*sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory){free(out);set_status(a,L"Не вдалося підготувати буфер обміну.");return;}void*dest=GlobalLock(memory);if(!dest){GlobalFree(memory);free(out);set_status(a,L"Не вдалося підготувати буфер обміну.");return;}memcpy(dest,out,bytes);GlobalUnlock(memory);free(out);if(!OpenClipboard(a->window)){GlobalFree(memory);set_status(a,L"Буфер обміну зайнятий іншою програмою.");return;}EmptyClipboard();HANDLE set=SetClipboardData(CF_UNICODETEXT,memory);CloseClipboard();if(!set){GlobalFree(memory);set_status(a,L"Не вдалося скопіювати цитату.");return;}set_status(a,L"Цитату з назвою закону та джерелом скопійовано.");
}static int open_reader_file(App*a,const wchar_t*path){
    if(a->loaded_document.full_text)reader_save_progress(a);
    size_t path_len=wcslen(path);if(path_len>=ZD_PATH_MAX){set_status(a,L"Шлях до файла надто довгий.");return 0;}
    ZDLoadedDocument fresh;char error[ZD_ERROR_MAX];if(!zd_local_document_read(path,&fresh,error,sizeof(error))){wchar_t w[ZD_ERROR_MAX];utf8_wide(error,w,ZD_ERROR_MAX);app_message(a,w,L"Не вдалося відкрити локальний документ",MB_OK|MB_ICONWARNING);return 0;}
    zd_loaded_document_free(&a->loaded_document);a->loaded_document=fresh;wcscpy(a->current_doc_path,path);a->current_pdf_path[0]=0;derive_pdf_path(path,a->current_pdf_path,ZD_PATH_MAX);int exists=a->current_pdf_path[0]&&GetFileAttributesW(a->current_pdf_path)!=INVALID_FILE_ATTRIBUTES;
    wchar_t info[ZD_TITLE_MAX+64];swprintf(info,sizeof(info)/sizeof(info[0]),L"%ls",a->loaded_document.title);SetWindowTextW(a->reader_info,info);RemovePropW(a->reader_info,UI_TEXT_PROP);SendMessageW(a->reader_clean,BM_SETCHECK,BST_CHECKED,0);SetWindowTextW(a->reader_text,a->loaded_document.clean_text);
    int64_t saved=0;char*file=wide_utf8(path);if(file){if(!zd_reader_progress_get(a->sqlite,a->paths->history_db,file,&saved,error,sizeof(error)))zd_log_write(a->paths,"WARNING","reader","Не вдалося прочитати позицію читання: %s",error);free(file);}size_t full_len=wcslen(a->loaded_document.full_text);if(saved<0||(uint64_t)saved>full_len)saved=0;a->reader_saved_position=saved;reader_set_position(a,saved);
    reader_fill_toc(a);reader_fill_bookmarks(a);EnableWindow(a->reader_txt,TRUE);EnableWindow(a->reader_pdf,TRUE);EnableWindow(a->reader_open_pdf,exists);EnableWindow(a->reader_browser,a->loaded_document.source_url[0]!=0);show_page(a,PAGE_READER);SetTimer(a->window,READER_SAVE_TIMER,5000,NULL);
    set_status(a,saved?L"Відновлено останню позицію читання.":L"Локальний документ відкрито; показано чистий текст.");SetFocus(a->reader_text);return 1;
}static int open_reader_browser(App*a){if(!a->loaded_document.source_url[0]){set_status(a,L"Для цього локального файла не знайдено перевірене посилання на джерело.");return 0;}char error[ZD_ERROR_MAX];if(!zd_validate_url(a->loaded_document.source_url,error,sizeof(error))){set_status(a,L"Посилання не веде на дозволений сайт нормативних документів.");return 0;}wchar_t url[ZD_URL_MAX];if(!utf8_wide(a->loaded_document.source_url,url,ZD_URL_MAX)){set_status(a,L"Не вдалося перетворити посилання.");return 0;}INT_PTR rc=(INT_PTR)ShellExecuteW(a->window,L"open",url,NULL,NULL,SW_SHOWNORMAL);if(rc<=32){set_status(a,L"Windows не змогла відкрити браузер.");return 0;}set_status(a,L"Початкову сторінку відкрито в браузері.");return 1;}
static void reader_toggle_clean(App*a){if(!a->loaded_document.full_text)return;int clean=SendMessageW(a->reader_clean,BM_GETCHECK,0,0)==BST_CHECKED;DWORD start=0,end=0;SendMessageW(a->reader_text,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);int64_t absolute=(int64_t)start;if(!clean&&a->loaded_document.clean_text)absolute+=(int64_t)(a->loaded_document.clean_text-a->loaded_document.full_text);SetWindowTextW(a->reader_text,clean?a->loaded_document.clean_text:a->loaded_document.full_text);reader_set_position(a,absolute);}
static void reader_find_next(App*a){wchar_t q[512];GetWindowTextW(a->reader_search,q,512);if(!q[0]||!a->loaded_document.full_text){set_status(a,L"Введіть фразу для пошуку у відкритому тексті.");return;}DWORD start=0,end=0;SendMessageW(a->reader_text,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);FINDTEXTW ft;ft.chrg.cpMin=(LONG)end;ft.chrg.cpMax=-1;ft.lpstrText=q;LRESULT pos=SendMessageW(a->reader_text,EM_FINDTEXTW,FR_DOWN,(LPARAM)&ft);if(pos<0){ft.chrg.cpMin=0;pos=SendMessageW(a->reader_text,EM_FINDTEXTW,FR_DOWN,(LPARAM)&ft);}if(pos<0){set_status(a,L"Фразу не знайдено в поточному поданні.");return;}SendMessageW(a->reader_text,EM_SETSEL,(WPARAM)pos,(LPARAM)(pos+wcslen(q)));SendMessageW(a->reader_text,EM_SCROLLCARET,0,0);set_status(a,L"Збіг знайдено.");}
static DWORD WINAPI pdf_worker(void*parameter){PdfJob*j=(PdfJob*)parameter;PdfResult*r=(PdfResult*)calloc(1,sizeof(*r));if(r){char error[ZD_ERROR_MAX]={0};r->success=zd_pdf_export(j->path,j->title,j->metadata,j->body,error,sizeof(error));if(r->success)wcscpy(r->path,j->path);else utf8_wide(error,r->error,ZD_ERROR_MAX);if(!PostMessageW(j->app->window,WM_APP_PDF_FINISHED,0,(LPARAM)r))free(r);}free(j->path);free(j->title);free(j->metadata);free(j->body);free(j);return 0;}
static wchar_t*dup_wide(const wchar_t*s){if(!s)s=L"";size_t n=wcslen(s);wchar_t*p=(wchar_t*)malloc((n+1)*sizeof(*p));if(p)memcpy(p,s,(n+1)*sizeof(*p));return p;}
static void start_pdf_export(App*a){if(a->pdf_busy||!a->loaded_document.full_text){set_status(a,L"Спочатку відкрийте локальний закон.");return;}if(!a->current_pdf_path[0]&&!derive_pdf_path(a->current_doc_path,a->current_pdf_path,ZD_PATH_MAX)){set_status(a,L"Шлях для PDF надто довгий.");return;}if(GetFileAttributesW(a->current_pdf_path)!=INVALID_FILE_ATTRIBUTES){if(app_message(a,L"PDF із такою назвою вже існує. Замінити його?",L"Підтвердьте заміну PDF",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;}PdfJob*j=(PdfJob*)calloc(1,sizeof(*j));if(!j){set_status(a,L"Недостатньо пам'яті для експорту PDF.");return;}j->app=a;j->path=dup_wide(a->current_pdf_path);j->title=dup_wide(a->loaded_document.title);j->metadata=dup_wide(a->loaded_document.metadata);j->body=dup_wide(a->loaded_document.clean_text);if(!j->path||!j->title||!j->metadata||!j->body){free(j->path);free(j->title);free(j->metadata);free(j->body);free(j);set_status(a,L"Недостатньо пам'яті для копії документа.");return;}a->pdf_busy=1;EnableWindow(a->reader_pdf,FALSE);set_ui_text(a,a->reader_pdf,L"Формується PDF…");set_status(a,L"Формую багатосторінковий PDF у фоновому потоці.");a->pdf_worker=CreateThread(NULL,0,pdf_worker,j,0,NULL);if(!a->pdf_worker){a->pdf_busy=0;EnableWindow(a->reader_pdf,TRUE);set_ui_text(a,a->reader_pdf,L"Експортувати PDF");free(j->path);free(j->title);free(j->metadata);free(j->body);free(j);set_status(a,L"Не вдалося запустити потік експорту PDF.");}}
static DWORD WINAPI csv_worker(void*parameter){CsvJob*j=(CsvJob*)parameter;CsvResult*r=(CsvResult*)calloc(1,sizeof(*r));if(r){ZDHistoryList list={0};char error[ZD_ERROR_MAX]={0};if(zd_history_load(j->app->sqlite,j->app->paths->history_db,&list,error,sizeof(error)))r->success=zd_history_export_csv(j->app->paths,&list,r->path,sizeof(r->path),error,sizeof(error));if(!r->success)snprintf(r->error,sizeof(r->error),"%s",error[0]?error:"Не вдалося експортувати історію.");zd_history_free(&list);if(!PostMessageW(j->app->window,WM_APP_CSV_FINISHED,0,(LPARAM)r))free(r);}free(j);return 0;}
static void start_history_csv(App*a){if(a->csv_busy)return;CsvJob*j=(CsvJob*)calloc(1,sizeof(*j));if(!j){set_status(a,L"Недостатньо пам'яті для експорту історії.");return;}j->app=a;a->csv_busy=1;EnableWindow(a->history_export,FALSE);set_status(a,L"Формую CSV у фоновому потоці…");a->csv_worker=CreateThread(NULL,0,csv_worker,j,0,NULL);if(!a->csv_worker){a->csv_busy=0;EnableWindow(a->history_export,TRUE);free(j);set_status(a,L"Не вдалося запустити експорт CSV.");}}static void open_selected_history(App*a,int folder){int64_t id;if(!selected_id(a->history_table,&id)){set_status(a,L"Виберіть запис історії.");return;}const ZDHistoryEntry*e=find_entry(a,id);if(!e||!e->saved_path||!*e->saved_path){set_status(a,L"Для цієї операції немає збереженого файла.");return;}if(!open_file_path(a,e->saved_path,folder)){set_status(a,L"Файл або папку не знайдено чи Windows не змогла їх відкрити.");return;}set_status(a,folder?L"Папку відкрито.":L"Файл відкрито.");}
static int selected_document_index(HWND list,size_t*out){int row=ListView_GetNextItem(list,-1,LVNI_SELECTED);if(row<0)return 0;LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_PARAM;item.iItem=row;if(!ListView_GetItem(list,&item)||item.lParam<0)return 0;*out=(size_t)item.lParam;return 1;}
static void open_selected_local_document(App*a,int browser){size_t i;if(!selected_document_index(a->documents_table,&i)||i>=a->local_documents.count){set_status(a,L"Оберіть текстовий документ зі списку.");return;}if(!open_reader_file(a,a->local_documents.items[i].path))return;if(browser)open_reader_browser(a);}
static void open_selected_history_reader(App*a,int browser){int64_t id;if(!selected_id(a->history_table,&id)){set_status(a,L"Виберіть запис історії.");return;}const ZDHistoryEntry*e=find_entry(a,id);if(!e||!e->saved_path||!*e->saved_path){set_status(a,L"Для цього запису немає локального текстового файла.");return;}wchar_t path[ZD_PATH_MAX];if(!utf8_wide(e->saved_path,path,ZD_PATH_MAX)||!open_reader_file(a,path))return;if(e->url&&zd_validate_url(e->url,NULL,0))snprintf(a->loaded_document.source_url,sizeof(a->loaded_document.source_url),"%s",e->url);EnableWindow(a->reader_browser,a->loaded_document.source_url[0]!=0);if(browser)open_reader_browser(a);}
static void open_recent(App*a){int64_t id;if(!selected_id(a->overview_table,&id)){set_status(a,L"Оберіть операцію зі збереженим файлом.");return;}const ZDHistoryEntry*e=find_entry(a,id);if(!e||!e->saved_path||!*e->saved_path){set_status(a,L"Для цього запису немає файла.");return;}wchar_t path[ZD_PATH_MAX];if(!utf8_wide(e->saved_path,path,ZD_PATH_MAX)){set_status(a,L"Некоректний шлях до файла.");return;}if(open_reader_file(a,path)&&e->url&&zd_validate_url(e->url,NULL,0)){snprintf(a->loaded_document.source_url,sizeof(a->loaded_document.source_url),"%s",e->url);EnableWindow(a->reader_browser,TRUE);}}
static void show_page(App*a,int page){
    if(page<0||page>=PAGE_COUNT)return;
    if(!strcmp(a->settings->ui_mode,"user")&&(page==PAGE_LOGS||page==PAGE_ABOUT))return;
    if(a->page==PAGE_READER&&page!=PAGE_READER){reader_save_progress(a);KillTimer(a->window,READER_SAVE_TIMER);}
    a->page=page;
    for(int p=0;p<PAGE_COUNT;p++)for(int i=0;i<a->control_count[p];i++)ShowWindow(a->controls[p][i],p==page?SW_SHOW:SW_HIDE);
    const wchar_t*titles[]={L"Огляд",L"Завантаження",L"Мої документи",L"Історія",L"Перегляд закону",L"Журнали",L"Налаштування",L"Про програму"};
    const wchar_t*subs[]={L"Зведення останніх операцій та швидкий доступ до завантаження.",L"Отримання відкритих нормативних документів із zakon.rada.gov.ua.",L"Знайдіть і відкрийте локально збережені закони.",L"Фільтруйте записи або експортуйте всю історію у UTF-8 CSV.",L"Зміст, закладки, зручний розмір тексту, цитати та PDF.",L"Технічні події та зведений експорт журналів.",L"Оберіть тему та папку для збереження документів.",L"Версія програми, локальні дані та правила роботи."};
    set_ui_text(a,a->page_title,titles[page]);
    set_ui_text(a,a->page_subtitle,subs[page]);
    for(int i=0;i<PAGE_COUNT;i++)InvalidateRect(a->nav[i],NULL,TRUE);
    if(page==PAGE_DOCUMENTS)refresh_local_documents(a);
    if(page==PAGE_HISTORY){refresh_history(a);fill_history_table(a);}
    else if(page==PAGE_OVERVIEW){refresh_history(a);fill_dashboard(a);}
    else if(page==PAGE_LOGS)refresh_logs(a);
    if(page==PAGE_ABOUT){wchar_t base[ZD_PATH_MAX],docs[ZD_PATH_MAX],about[ZD_PATH_MAX*3];if(!utf8_wide(a->paths->base,base,ZD_PATH_MAX))wcscpy(base,L"");if(!utf8_wide(a->settings->documents_dir,docs,ZD_PATH_MAX))wcscpy(docs,L"");swprintf(about,sizeof(about)/sizeof(about[0]),L"ZakonDownloader  ·  %ls\n%ls\n\n%ls\n\n%ls\n%ls\n\n%ls\n%ls\n\n%ls",APP_VERSION,tr(a,L"Нативна версія на C для Windows."),tr(a,L"Документи й налаштування зберігаються локально у профілі користувача."),tr(a,L"Папка даних:"),base,tr(a,L"Папка документів:"),docs,tr(a,L"Завантажуються лише відкриті сторінки zakon.rada.gov.ua. Обмеження сайту не обходяться."));SetWindowTextW(a->about_text,about);}
    RedrawWindow(a->window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void apply_mode(App*a){
    int dev=!strcmp(a->settings->ui_mode,"developer");
    for(int i=0;i<PAGE_COUNT;i++)ShowWindow(a->nav[i],dev||!(i==PAGE_LOGS||i==PAGE_ABOUT)?SW_SHOW:SW_HIDE);
    SendMessageW(a->mode_combo,CB_SETCURSEL,dev?1:0,0);
    set_ui_text(a,a->sidebar_section,dev?L"ІНСТРУМЕНТИ РОЗРОБНИКА":L"РОБОЧИЙ ПРОСТІР");
    set_ui_text(a,a->sidebar_note,dev?L"ТЕХНІЧНИЙ РЕЖИМ\nУСІ РОЗДІЛИ ДОСТУПНІ":L"ДОКУМЕНТИ\nЗБЕРІГАЮТЬСЯ ЛОКАЛЬНО");
    if(a->page==PAGE_LOGS||a->page==PAGE_ABOUT){if(!dev)show_page(a,PAGE_OVERVIEW);}
    for(int i=8;i<=17;i++)ShowWindow(a->controls[PAGE_SETTINGS][i],dev?SW_SHOW:SW_HIDE);
    if(a->page==PAGE_SETTINGS)set_ui_text(a,a->page_subtitle,dev?L"Тема, папка документів, мережеві параметри й журналювання.":L"Оберіть тему та папку для збереження документів.");
    RedrawWindow(a->window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
typedef struct { HFONT old_fonts[5]; HFONT new_fonts[5]; } FontRemap;
static BOOL CALLBACK remap_child_font(HWND child, LPARAM parameter) {
    FontRemap *map = (FontRemap *)parameter;
    HFONT current = (HFONT)SendMessageW(child, WM_GETFONT, 0, 0);
    for (int i = 0; i < 5; ++i) {
        if (current == map->old_fonts[i]) {
            SendMessageW(child, WM_SETFONT, (WPARAM)map->new_fonts[i], TRUE);
            break;
        }
    }
    return TRUE;
}
static void refresh_reader_font(App *a) {
    HFONT font = CreateFontW(-a->reader_font_px,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,reader_font_face(a));
    if (!font) return;
    HFONT old = a->font_reader;
    a->font_reader = font;
    if (a->reader_text) {
        SendMessageW(a->reader_text, WM_SETFONT, (WPARAM)font, TRUE);
        CHARFORMAT2W cf; memset(&cf,0,sizeof(cf)); cf.cbSize=sizeof(cf); cf.dwMask=CFM_FACE|CFM_SIZE; cf.yHeight=a->reader_font_px*15; wcscpy(cf.szFaceName,reader_font_face(a));
        SendMessageW(a->reader_text, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }
    if (old) DeleteObject(old);
}
static void refresh_ui_fonts(App *a) {
    FontRemap map; memset(&map,0,sizeof(map));
    map.old_fonts[0]=a->font_normal; map.old_fonts[1]=a->font_small; map.old_fonts[2]=a->font_title; map.old_fonts[3]=a->font_brand; map.old_fonts[4]=a->font_bold;
    const wchar_t *face=ui_font_face(a);
    map.new_fonts[0]=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
    map.new_fonts[1]=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
    map.new_fonts[2]=CreateFontW(-28,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
    map.new_fonts[3]=CreateFontW(-19,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
    map.new_fonts[4]=CreateFontW(-16,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,face);
    for (int i=0;i<5;i++) if (!map.new_fonts[i]) { for(int j=0;j<5;j++) if(map.new_fonts[j]) DeleteObject(map.new_fonts[j]); return; }
    a->font_normal=map.new_fonts[0]; a->font_small=map.new_fonts[1]; a->font_title=map.new_fonts[2]; a->font_brand=map.new_fonts[3]; a->font_bold=map.new_fonts[4];
    EnumChildWindows(a->window,remap_child_font,(LPARAM)&map);
    for (int i=0;i<5;i++) if(map.old_fonts[i]) DeleteObject(map.old_fonts[i]);
    refresh_reader_font(a);
}
static BOOL CALLBACK retranslate_child(HWND child, LPARAM parameter) {
    App *a=(App *)parameter;
    const wchar_t *source=(const wchar_t *)GetPropW(child,UI_TEXT_PROP);
    if(source) SetWindowTextW(child,tr(a,source));
    return TRUE;
}
static void refill_localized_combo(App *a, HWND combo, const wchar_t **items, int count, int selected) {
    if(!combo) return;
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    for(int i=0;i<count;i++) SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)tr(a,items[i]));
    if(selected<0||selected>=count) selected=0;
    SendMessageW(combo,CB_SETCURSEL,selected,0);
}
static void update_columns(App *a, HWND list, const wchar_t **names, int count) {
    for(int i=0;i<count;i++){LVCOLUMNW c;memset(&c,0,sizeof(c));c.mask=LVCF_TEXT;c.pszText=(LPWSTR)tr(a,names[i]);ListView_SetColumn(list,i,&c);}
}
static void apply_language(App *a) {
    if(!a||!a->settings||!a->window)return;
    a->localizing=1;
    EnumChildWindows(a->window,retranslate_child,(LPARAM)a);
    const wchar_t *modes[]={L"Користувач",L"Розробник"};
    const wchar_t *themes[]={L"Світла",L"Темна"};
    const wchar_t *statuses[]={L"Усі статуси",L"Розпочато",L"Успішно",L"Помилка",L"Пропущено"};
    const wchar_t *levels[]={L"Інформація (INFO)",L"Попередження (WARNING)",L"Помилки (ERROR)",L"Налагодження (DEBUG)"};
    int mode=!strcmp(a->settings->ui_mode,"developer")?1:0;
    int theme=!strcmp(a->settings->theme,"dark")?1:0;
    int status=(int)SendMessageW(a->history_status,CB_GETCURSEL,0,0);
    int level=(int)SendMessageW(a->settings_level,CB_GETCURSEL,0,0);
    refill_localized_combo(a,a->mode_combo,modes,2,mode);
    refill_localized_combo(a,a->settings_theme,themes,2,theme);
    refill_localized_combo(a,a->history_status,statuses,5,status);
    refill_localized_combo(a,a->settings_level,levels,4,level);
    SendMessageW(a->settings_language,CB_SETCURSEL,zd_i18n_index(a->settings->language),0);
    SendMessageW(a->documents_search,EM_SETCUEBANNER,TRUE,(LPARAM)tr(a,L"Пошук у локальних документах…"));
    SendMessageW(a->history_search,EM_SETCUEBANNER,TRUE,(LPARAM)tr(a,L"Пошук за URL, назвою або статусом…"));
    SendMessageW(a->reader_search,EM_SETCUEBANNER,TRUE,(LPARAM)tr(a,L"Знайти фразу в тексті…"));
    const wchar_t *oh[]={L"Дата",L"Документ / адреса",L"Статус",L"Результат"};
    const wchar_t *dh[]={L"Файл / закон",L"Змінено",L"Розмір"};
    const wchar_t *hh[]={L"ID",L"Запущено",L"Завершено",L"URL",L"Назва документа",L"Статус",L"Файл"};
    update_columns(a,a->overview_table,oh,4); update_columns(a,a->documents_table,dh,3); update_columns(a,a->history_table,hh,7);
    reader_fill_toc(a);
    if(a->current_doc_path[0]) reader_fill_bookmarks(a);
    else if(a->reader_bookmarks){SendMessageW(a->reader_bookmarks,CB_RESETCONTENT,0,0);SendMessageW(a->reader_bookmarks,CB_ADDSTRING,0,(LPARAM)tr(a,L"Мої закладки"));SendMessageW(a->reader_bookmarks,CB_SETCURSEL,0,0);EnableWindow(a->reader_bookmarks,FALSE);}
    a->localizing=0;
    refresh_ui_fonts(a);
    apply_mode(a);
    show_page(a,a->page);
    set_status(a,L"Готово.");
}

static int save_settings(App*a){char err[ZD_ERROR_MAX];if(!zd_settings_save(a->paths->settings_file,a->settings,err,sizeof(err))){wchar_t w[ZD_ERROR_MAX];utf8_wide(err,w,ZD_ERROR_MAX);app_message(a,w,L"Налаштування не збережено",MB_OK|MB_ICONERROR);return 0;}zd_log_write(a->paths,"INFO","settings","Налаштування збережено.");return 1;}
static void choose_folder(App*a){BROWSEINFOW bi;memset(&bi,0,sizeof(bi));bi.hwndOwner=a->window;bi.lpszTitle=tr(a,L"Оберіть папку для документів");bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;LPITEMIDLIST id=SHBrowseForFolderW(&bi);if(!id)return;wchar_t path[MAX_PATH];if(SHGetPathFromIDListW(id,path))SetWindowTextW(a->settings_docs,path);CoTaskMemFree(id);}
static void save_settings_from_ui(App*a){ZDSettings next=*a->settings;int theme=(int)SendMessageW(a->settings_theme,CB_GETCURSEL,0,0);strcpy(next.theme,theme==1?"dark":"light");int language=(int)SendMessageW(a->settings_language,CB_GETCURSEL,0,0);snprintf(next.language,sizeof(next.language),"%s",zd_i18n_code_at(language>=0?(size_t)language:0));int level=(int)SendMessageW(a->settings_level,CB_GETCURSEL,0,0);const char*levels[]={"INFO","WARNING","ERROR","DEBUG"};strcpy(next.log_level,levels[level>=0&&level<4?level:0]);wchar_t wpath[ZD_PATH_MAX];GetWindowTextW(a->settings_docs,wpath,ZD_PATH_MAX);char*path=wide_utf8(wpath);if(!path||!path[0]){free(path);app_message(a,L"Вкажіть папку для документів.",L"Перевірте папку",MB_OK|MB_ICONWARNING);return;}if(strlen(path)>=sizeof(next.documents_dir)){free(path);app_message(a,L"Шлях до папки надто довгий.",L"Перевірте папку",MB_OK|MB_ICONWARNING);return;}strcpy(next.documents_dir,path);free(path);if(!zd_ensure_directory_utf8(next.documents_dir)){app_message(a,L"Не вдалося створити папку документів.",L"Помилка",MB_OK|MB_ICONERROR);return;}if(!strcmp(next.ui_mode,"developer")){wchar_t n[32];char b[32];GetWindowTextW(a->settings_connect,n,32);WideCharToMultiByte(CP_UTF8,0,n,-1,b,sizeof(b),NULL,NULL);next.request_timeout_connect=atoi(b);GetWindowTextW(a->settings_read,n,32);WideCharToMultiByte(CP_UTF8,0,n,-1,b,sizeof(b),NULL,NULL);next.request_timeout_read=atoi(b);GetWindowTextW(a->settings_retries,n,32);WideCharToMultiByte(CP_UTF8,0,n,-1,b,sizeof(b),NULL,NULL);next.max_retry_attempts=atoi(b);if(next.request_timeout_connect<1||next.request_timeout_connect>300||next.request_timeout_read<1||next.request_timeout_read>600||next.max_retry_attempts<1||next.max_retry_attempts>10){app_message(a,L"Таймаут підключення: 1–300 сек.; читання: 1–600 сек.; спроби: 1–10.",L"Перевірте мережеві параметри",MB_OK|MB_ICONWARNING);return;}}*a->settings=next;if(!save_settings(a))return;apply_theme(a);load_settings_controls(a);apply_language(a);set_status(a,L"Налаштування збережено й застосовано.");}
static void reset_settings(App*a){a->settings->request_timeout_connect=10;a->settings->request_timeout_read=30;a->settings->max_retry_attempts=3;strcpy(a->settings->log_level,"INFO");strcpy(a->settings->theme,"light");snprintf(a->settings->documents_dir,sizeof(a->settings->documents_dir),"%s",a->paths->documents);load_settings_controls(a);set_status(a,L"Типові значення встановлено; натисніть «Зберегти налаштування».");}
static void post_progress(void*context,int stage,const char*message){App*a=(App*)context;ProgressMessage*m=(ProgressMessage*)calloc(1,sizeof(*m));if(!m)return;m->app=a;m->stage=stage;utf8_wide(message,m->text,256);if(!PostMessageW(a->window,WM_APP_PROGRESS,0,(LPARAM)m))free(m);}
static DWORD WINAPI download_worker(void*parameter){WorkerJob*j=(WorkerJob*)parameter;App*a=j->app;WorkerResult*r=(WorkerResult*)calloc(1,sizeof(*r));if(r)zd_download_run(j->url,a->settings,a->paths,a->sqlite,post_progress,a,&r->result);free(j);if(!r)return 1;if(!PostMessageW(a->window,WM_APP_FINISHED,0,(LPARAM)r))free(r);return 0;}
static void start_download(App*a){
    if(a->busy)return;
    wchar_t wurl[ZD_URL_MAX];GetWindowTextW(a->download_url,wurl,ZD_URL_MAX);
    char*url=wide_utf8(wurl);
    if(!url){set_download_validation(a,L"Не вдалося прочитати посилання.");return;}
    trim_ascii_whitespace(url);
    if(!url[0]){free(url);set_download_validation(a,L"Введіть пряме посилання на документ Верховної Ради України.");return;}
    char validation[ZD_ERROR_MAX]={0};
    if(!zd_validate_url(url,validation,sizeof(validation))){wchar_t message[ZD_ERROR_MAX];if(!utf8_wide(validation,message,ZD_ERROR_MAX))wcscpy(message,L"Перевірте посилання на документ.");free(url);set_download_validation(a,message);return;}
    WorkerJob*j=(WorkerJob*)calloc(1,sizeof(*j));
    if(!j){free(url);set_download_validation(a,L"Недостатньо пам’яті для запуску операції.");return;}
    j->app=a;snprintf(j->url,sizeof(j->url),"%s",url);free(url);
    a->busy=1;EnableWindow(a->download_start,FALSE);EnableWindow(a->mode_combo,FALSE);EnableWindow(a->theme_button,FALSE);EnableWindow(a->settings_docs,FALSE);
    SendMessageW(a->download_progress,PBM_SETPOS,0,0);set_download_status(a,L"Готую запит…");set_status(a,L"Виконується завантаження. Дочекайтеся завершення.");
    a->worker=CreateThread(NULL,0,download_worker,j,0,NULL);
    if(!a->worker){a->busy=0;free(j);EnableWindow(a->download_start,TRUE);EnableWindow(a->mode_combo,TRUE);EnableWindow(a->theme_button,TRUE);EnableWindow(a->settings_docs,TRUE);set_download_status(a,L"Не вдалося створити потік завантаження.");set_status(a,L"");}
}
static void finish_download(App*a,WorkerResult*wrapper){if(!wrapper)return;ZDDownloadResult*r=&wrapper->result;a->busy=0;if(a->worker){CloseHandle(a->worker);a->worker=NULL;}EnableWindow(a->download_start,TRUE);EnableWindow(a->mode_combo,TRUE);EnableWindow(a->theme_button,TRUE);EnableWindow(a->settings_docs,TRUE);SendMessageW(a->download_progress,PBM_SETPOS,r->success?4:0,0);if(r->success){snprintf(a->last_saved_path,sizeof(a->last_saved_path),"%s",r->saved_path);EnableWindow(a->download_open,TRUE);wchar_t title[ZD_TITLE_MAX];utf8_wide(r->title,title,ZD_TITLE_MAX);set_download_status(a,L"Готово — документ збережено.");set_status(a,L"Операцію завершено успішно.");wchar_t msg[ZD_TITLE_MAX+ZD_PATH_MAX+128];wchar_t wpath[ZD_PATH_MAX];utf8_wide(r->saved_path,wpath,ZD_PATH_MAX);swprintf(msg,sizeof(msg)/sizeof(msg[0]),tr(a,L"Документ збережено:\n%ls\n\nФайл: %ls"),title,wpath);app_message(a,msg,L"Завантаження завершено",MB_OK|MB_ICONINFORMATION);}else{wchar_t error[ZD_ERROR_MAX*2];utf8_wide(r->error,error,ZD_ERROR_MAX*2);set_download_status(a,error);set_status(a,L"Не вдалося завершити завантаження.");app_message(a,error,L"Помилка завантаження",MB_OK|MB_ICONWARNING);}refresh_history(a);fill_dashboard(a);fill_history_table(a);free(wrapper);}
static void paste_clipboard(App*a){if(!OpenClipboard(a->window)){set_status(a,L"Буфер обміну недоступний.");return;}HANDLE h=GetClipboardData(CF_UNICODETEXT);if(h){const wchar_t*p=(const wchar_t*)GlobalLock(h);if(p){SetWindowTextW(a->download_url,p);GlobalUnlock(h);set_status(a,L"Посилання вставлено з буфера обміну.");}else set_status(a,L"Не вдалося прочитати текст буфера обміну.");}else set_status(a,L"У буфері обміну немає текстового посилання.");CloseClipboard();}
static void draw_card(HDC dc,int x,int y,int w,int h,const Palette*p,int accent){if(w<=0||h<=0)return;int dark=p->bg==RGB(17,18,20);COLORREF shadow=dark?RGB(9,10,11):RGB(229,227,220);HBRUSH sb=CreateSolidBrush(shadow);HPEN sp=CreatePen(PS_SOLID,1,shadow);HGDIOBJ oldb=SelectObject(dc,sb),oldp=SelectObject(dc,sp);RoundRect(dc,x+1,y+3,x+w+1,y+h+3,18,18);SelectObject(dc,oldb);SelectObject(dc,oldp);DeleteObject(sb);DeleteObject(sp);HBRUSH b=CreateSolidBrush(p->surface);HPEN pen=CreatePen(PS_SOLID,1,p->border);HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,pen);RoundRect(dc,x,y,x+w,y+h,18,18);if(accent){HPEN ap=CreatePen(PS_SOLID,4,p->accent);SelectObject(dc,ap);MoveToEx(dc,x+2,y+18,NULL);LineTo(dc,x+2,y+h-18);SelectObject(dc,pen);DeleteObject(ap);}SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(pen);}
static void paint_background(App*a,HDC dc){RECT r;GetClientRect(a->window,&r);FillRect(dc,&r,a->bg_brush);RECT side={0,0,SIDE_W,r.bottom-STATUS_H};FillRect(dc,&side,a->surface_brush);HPEN pen=CreatePen(PS_SOLID,1,a->colors.border);HGDIOBJ old=SelectObject(dc,pen);MoveToEx(dc,SIDE_W-1,0,NULL);LineTo(dc,SIDE_W-1,r.bottom-STATUS_H);SelectObject(dc,old);DeleteObject(pen);SetBkMode(dc,TRANSPARENT);if(a->logo_mark)DrawIconEx(dc,16,22,a->logo_mark,42,42,0,NULL,DI_NORMAL);else{SetTextColor(dc,a->colors.accent_text);RECT mark={18,24,58,64};HBRUSH b=CreateSolidBrush(a->colors.accent);HGDIOBJ ob=SelectObject(dc,b);RoundRect(dc,mark.left,mark.top,mark.right,mark.bottom,10,10);SelectObject(dc,ob);DeleteObject(b);HFONT of=(HFONT)SelectObject(dc,a->font_brand);DrawTextW(dc,L"Z",1,&mark,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,of);}
    int x=SIDE_W+28,y=HEADER_H+10,w=r.right-x-28,h=r.bottom-STATUS_H-y-14;if(a->page==PAGE_OVERVIEW){draw_card(dc,x,y,w,112,&a->colors,1);int cw=(w-24)/3;draw_card(dc,x,y+126,cw,96,&a->colors,1);draw_card(dc,x+cw+12,y+126,cw,96,&a->colors,0);draw_card(dc,x+2*(cw+12),y+126,w-2*(cw+12),96,&a->colors,0);draw_card(dc,x,y+236,w,h-236,&a->colors,0);}else if(a->page==PAGE_DOWNLOAD||a->page==PAGE_DOCUMENTS||a->page==PAGE_HISTORY||a->page==PAGE_READER||a->page==PAGE_LOGS||a->page==PAGE_SETTINGS||a->page==PAGE_ABOUT)draw_card(dc,x,y,w,h,&a->colors,0);
}
static void draw_button(App*a,const DRAWITEMSTRUCT*d){int id=(int)d->CtlID;int nav=id>=ID_NAV_BASE&&id<ID_NAV_BASE+PAGE_COUNT;int active=nav&&(id-ID_NAV_BASE)==a->page;int primary=(id==ID_OV_NEW||id==ID_D_START||id==ID_S_SAVE||id==ID_L_EXPORT||id==ID_R_PDF||id==ID_H_EXPORT);int pressed=(d->itemState&ODS_SELECTED)!=0,disabled=(d->itemState&ODS_DISABLED)!=0,hot=(d->itemState&ODS_HOTLIGHT)!=0;COLORREF fill=primary||active?a->colors.accent:(hot?a->colors.surface2:a->colors.surface);COLORREF edge=primary||active?a->colors.accent:(hot?a->colors.accent:a->colors.border);COLORREF text=primary||active?a->colors.accent_text:a->colors.text;if(pressed&&!(primary||active))fill=a->colors.surface2;if(disabled){fill=a->colors.surface2;text=a->colors.muted;edge=a->colors.border;}HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,edge);HGDIOBJ ob=SelectObject(d->hDC,b),op=SelectObject(d->hDC,p);RECT r=d->rcItem;InflateRect(&r,-1,-1);RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,12,12);SelectObject(d->hDC,ob);SelectObject(d->hDC,op);DeleteObject(b);DeleteObject(p);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,text);HFONT old=(HFONT)SelectObject(d->hDC,a->font_bold);wchar_t label[512];GetWindowTextW(d->hwndItem,label,512);UINT flags=DT_VCENTER|DT_SINGLELINE|(nav?DT_LEFT:DT_CENTER);if(nav)r.left+=14;DrawTextW(d->hDC,label,-1,&r,flags);if(d->itemState&ODS_FOCUS){RECT f=r;InflateRect(&f,-4,-4);DrawFocusRect(d->hDC,&f);}SelectObject(d->hDC,old);}
static void layout(App*a){RECT r;GetClientRect(a->window,&r);int w=r.right,h=r.bottom;int work_h=h-STATUS_H;MoveWindow(a->page_title,SIDE_W+28,18,w-SIDE_W-320,38,TRUE);MoveWindow(a->page_subtitle,SIDE_W+30,58,w-SIDE_W-320,34,TRUE);MoveWindow(a->mode_combo,w-322,21,150,30,TRUE);MoveWindow(a->theme_button,w-164,20,148,34,TRUE);MoveWindow(a->status,12,work_h+3,w-24,22,TRUE);MoveWindow(a->sidebar_brand,66,29,SIDE_W-76,32,TRUE);MoveWindow(a->sidebar_section,18,72,SIDE_W-36,20,TRUE);MoveWindow(a->sidebar_note,18,work_h-56,SIDE_W-36,44,TRUE);for(int i=0;i<PAGE_COUNT;i++)MoveWindow(a->nav[i],16,100+i*48,SIDE_W-32,42,TRUE);
    int x=SIDE_W+28,y=HEADER_H+10,cw=w-x-28,ch=work_h-y-14;if(cw<300)cw=300;if(ch<300)ch=300;
    HWND*c=a->controls[PAGE_OVERVIEW];(void)c;/* Dashboard positions */
    MoveWindow(a->controls[PAGE_OVERVIEW][0],x+22,y+14,cw-300,18,TRUE);MoveWindow(a->controls[PAGE_OVERVIEW][1],x+22,y+34,cw-300,34,TRUE);MoveWindow(a->controls[PAGE_OVERVIEW][2],x+22,y+72,cw-300,25,TRUE);MoveWindow(GetDlgItem(a->window,ID_OV_NEW),x+cw-238,y+52,214,42,TRUE);
    int gap=12,mc=(cw-24)/3;for(int i=0;i<3;i++){int mx=x+i*(mc+gap)+16;MoveWindow(a->controls[PAGE_OVERVIEW][4+i*2],mx,y+138,mc-24,20,TRUE);MoveWindow(a->controls[PAGE_OVERVIEW][5+i*2],mx,y+160,mc-24,44,TRUE);}MoveWindow(a->controls[PAGE_OVERVIEW][10],x+18,y+238,cw-36,26,TRUE);MoveWindow(a->controls[PAGE_OVERVIEW][11],x+18,y+263,cw-36,20,TRUE);MoveWindow(a->overview_table,x+16,y+287,cw-32,ch-302,TRUE);
    /* Download form */
    int dy=y+28;MoveWindow(a->controls[PAGE_DOWNLOAD][0],x+24,dy,cw-48,26,TRUE);MoveWindow(a->controls[PAGE_DOWNLOAD][1],x+24,dy+56,300,22,TRUE);MoveWindow(a->download_url,x+24,dy+84,cw-190,42,TRUE);MoveWindow(GetDlgItem(a->window,ID_D_PASTE),x+cw-150,dy+84,126,42,TRUE);MoveWindow(a->download_start,x+24,dy+148,230,46,TRUE);MoveWindow(a->download_progress,x+24,dy+214,cw-48,14,TRUE);MoveWindow(a->download_status,x+24,dy+240,cw-48,36,TRUE);MoveWindow(a->download_open,x+24,dy+292,240,40,TRUE);MoveWindow(a->controls[PAGE_DOWNLOAD][8],x+24,dy+350,cw-48,24,TRUE);
    /* Local document library */
    MoveWindow(a->documents_search,x+12,y+12,cw-164,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_DOC_REFRESH),x+cw-140,y+12,128,36,TRUE);MoveWindow(a->documents_table,x+12,y+58,cw-24,ch-112,TRUE);MoveWindow(GetDlgItem(a->window,ID_DOC_VIEW),x+12,y+ch-46,190,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_DOC_BROWSER),x+214,y+ch-46,190,36,TRUE);
    /* History filters and table */
    int hy=y+14;MoveWindow(a->history_search,x+12,hy,cw/3,36,TRUE);MoveWindow(a->history_status,x+cw/3+22,hy,132,36,TRUE);MoveWindow(a->history_date_check,x+cw/3+164,hy+5,75,30,TRUE);MoveWindow(a->history_from,x+cw/3+240,hy,125,36,TRUE);MoveWindow(a->history_to,x+cw/3+371,hy,125,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_H_REFRESH),x+cw-112,hy,100,36,TRUE);MoveWindow(a->history_table,x+12,hy+48,cw-24,ch-112,TRUE);MoveWindow(GetDlgItem(a->window,ID_H_VIEW),x+12,y+ch-50,108,38,TRUE);MoveWindow(GetDlgItem(a->window,ID_H_BROWSER),x+128,y+ch-50,108,38,TRUE);MoveWindow(GetDlgItem(a->window,ID_H_OPEN),x+244,y+ch-50,135,38,TRUE);MoveWindow(GetDlgItem(a->window,ID_H_FOLDER),x+387,y+ch-50,105,38,TRUE);MoveWindow(a->history_export,x+500,y+ch-50,150,38,TRUE);
    /* Law reader and action toolbar */
    MoveWindow(a->reader_info,x+20,y+12,cw-40,28,TRUE);MoveWindow(a->reader_search,x+12,y+48,300,34,TRUE);MoveWindow(GetDlgItem(a->window,ID_R_NEXT),x+320,y+48,106,34,TRUE);MoveWindow(a->reader_clean,x+440,y+48,cw-580,34,TRUE);MoveWindow(a->reader_font_down,x+cw-120,y+48,46,34,TRUE);MoveWindow(a->reader_font_up,x+cw-64,y+48,52,34,TRUE);MoveWindow(a->reader_browser,x+12,y+92,252,36,TRUE);MoveWindow(a->reader_txt,x+276,y+92,132,36,TRUE);MoveWindow(a->reader_pdf,x+420,y+92,158,36,TRUE);MoveWindow(a->reader_open_pdf,x+590,y+92,142,36,TRUE);int roww=cw-24,usable=roww-32,tocw=usable*25/100,bmw=usable*25/100,addw=usable*15/100,delw=usable*15/100,copyw=usable-tocw-bmw-addw-delw,rx=x+12;MoveWindow(a->reader_toc,rx,y+138,tocw,34,TRUE);rx+=tocw+8;MoveWindow(a->reader_bookmarks,rx,y+138,bmw,34,TRUE);rx+=bmw+8;MoveWindow(a->reader_add_bookmark,rx,y+138,addw,34,TRUE);rx+=addw+8;MoveWindow(a->reader_delete_bookmark,rx,y+138,delw,34,TRUE);rx+=delw+8;MoveWindow(a->reader_copy_citation,rx,y+138,copyw,34,TRUE);MoveWindow(a->reader_text,x+12,y+180,cw-24,ch-194,TRUE);
    /* Developer log viewer */
    MoveWindow(a->logs_text,x+12,y+12,cw-24,ch-66,TRUE);MoveWindow(GetDlgItem(a->window,ID_L_REFRESH),x+12,y+ch-44,150,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_L_EXPORT),x+174,y+ch-44,230,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_L_FOLDER),x+416,y+ch-44,224,36,TRUE);
    /* Settings */
    MoveWindow(a->controls[PAGE_SETTINGS][0],x+22,y+18,cw-44,28,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][1],x+24,y+62,190,28,TRUE);MoveWindow(a->settings_theme,x+222,y+58,220,36,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][3],x+24,y+108,190,28,TRUE);MoveWindow(a->settings_language,x+222,y+104,240,36,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][5],x+24,y+154,190,28,TRUE);MoveWindow(a->settings_docs,x+222,y+150,cw-356,38,TRUE);MoveWindow(GetDlgItem(a->window,ID_S_BROWSE),x+cw-122,y+150,98,38,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][8],x+24,y+212,cw-48,28,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][9],x+24,y+254,190,26,TRUE);MoveWindow(a->settings_connect,x+222,y+250,120,34,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][11],x+24,y+296,190,26,TRUE);MoveWindow(a->settings_read,x+222,y+292,120,34,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][13],x+24,y+338,190,26,TRUE);MoveWindow(a->settings_retries,x+222,y+334,120,34,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][15],x+24,y+388,cw-48,26,TRUE);MoveWindow(a->controls[PAGE_SETTINGS][16],x+24,y+428,190,26,TRUE);MoveWindow(a->settings_level,x+222,y+424,240,36,TRUE);MoveWindow(GetDlgItem(a->window,ID_S_DEFAULTS),x+cw-390,y+ch-52,180,38,TRUE);MoveWindow(GetDlgItem(a->window,ID_S_SAVE),x+cw-200,y+ch-52,190,38,TRUE);
    MoveWindow(a->about_text,x+28,y+24,cw-56,ch-180,TRUE);MoveWindow(GetDlgItem(a->window,ID_A_OPEN_DOCS),x+28,y+ch-134,250,40,TRUE);MoveWindow(GetDlgItem(a->window,ID_A_OPEN_DATA),x+290,y+ch-134,230,40,TRUE);MoveWindow(GetDlgItem(a->window,ID_A_OPEN_SITE),x+28,y+ch-80,270,40,TRUE);
}
static void set_theme_from_header(App*a){a->settings->theme[0]=a->dark?'l':'d';if(a->dark)strcpy(a->settings->theme,"light");else strcpy(a->settings->theme,"dark");if(save_settings(a)){apply_theme(a);load_settings_controls(a);}}
static void set_mode(App*a,int developer){strcpy(a->settings->ui_mode,developer?"developer":"user");if(!save_settings(a))return;apply_mode(a);}
static void handle_command(App*a,WPARAM wp){
    int id=LOWORD(wp),notify=HIWORD(wp);
    if(id>=ID_NAV_BASE&&id<ID_NAV_BASE+PAGE_COUNT&&notify==BN_CLICKED){int target=id-ID_NAV_BASE;show_page(a,target);if(target==PAGE_SETTINGS)apply_mode(a);return;}
    switch(id){
    case ID_MODE:if(notify==CBN_SELCHANGE)set_mode(a,SendMessageW(a->mode_combo,CB_GETCURSEL,0,0)==1);break;
    case ID_S_LANGUAGE:if(notify==CBN_SELCHANGE&&!a->localizing){int li=(int)SendMessageW(a->settings_language,CB_GETCURSEL,0,0);char previous[sizeof(a->settings->language)];snprintf(previous,sizeof(previous),"%s",a->settings->language);snprintf(a->settings->language,sizeof(a->settings->language),"%s",zd_i18n_code_at(li>=0?(size_t)li:0));if(save_settings(a)){apply_language(a);set_status(a,L"Налаштування збережено й застосовано.");}else{snprintf(a->settings->language,sizeof(a->settings->language),"%s",previous);apply_language(a);}}break;
    case ID_THEME:if(notify==BN_CLICKED)set_theme_from_header(a);break;
    case ID_OV_NEW:show_page(a,PAGE_DOWNLOAD);SetFocus(a->download_url);break;
    case ID_D_PASTE:paste_clipboard(a);break;
    case ID_D_START:if(notify==BN_CLICKED)start_download(a);break;
    case ID_D_OPEN:if(a->last_saved_path[0]){wchar_t p[ZD_PATH_MAX];if(utf8_wide(a->last_saved_path,p,ZD_PATH_MAX))open_reader_file(a,p);else set_status(a,L"Некоректний шлях до збереженого файла.");}break;
    case ID_DOC_REFRESH:refresh_local_documents(a);break;
    case ID_DOC_VIEW:open_selected_local_document(a,0);break;
    case ID_DOC_BROWSER:open_selected_local_document(a,1);break;
    case ID_H_REFRESH:refresh_history(a);fill_history_table(a);break;
    case ID_H_VIEW:open_selected_history_reader(a,0);break;
    case ID_H_BROWSER:open_selected_history_reader(a,1);break;
    case ID_H_OPEN:open_selected_history(a,0);break;
    case ID_H_FOLDER:open_selected_history(a,1);break;
    case ID_R_CLEAN:if(notify==BN_CLICKED)reader_toggle_clean(a);break;
    case ID_R_NEXT:if(notify==BN_CLICKED)reader_find_next(a);break;
    case ID_R_TOC:if(notify==CBN_SELCHANGE)reader_jump_toc(a);break;
    case ID_R_BOOKMARKS:if(notify==CBN_SELCHANGE)reader_jump_bookmark(a);break;
    case ID_R_ADD_BOOKMARK:if(notify==BN_CLICKED)reader_bookmark_add(a);break;
    case ID_R_DELETE_BOOKMARK:if(notify==BN_CLICKED)reader_bookmark_delete(a);break;
    case ID_R_FONT_DOWN:if(notify==BN_CLICKED)reader_change_font(a,-1);break;
    case ID_R_FONT_UP:if(notify==BN_CLICKED)reader_change_font(a,1);break;
    case ID_R_COPY_CITATION:if(notify==BN_CLICKED)reader_copy_citation(a);break;
    case ID_R_BROWSER:open_reader_browser(a);break;
    case ID_R_TXT:if(a->current_doc_path[0]){char*p=wide_utf8(a->current_doc_path);if(p){open_file_path(a,p,0);free(p);}}break;
    case ID_R_PDF:if(notify==BN_CLICKED)start_pdf_export(a);break;
    case ID_R_OPEN_PDF:if(a->current_pdf_path[0]){INT_PTR rc=(INT_PTR)ShellExecuteW(a->window,L"open",a->current_pdf_path,NULL,NULL,SW_SHOWNORMAL);if(rc>32)set_status(a,L"PDF відкрито у стандартному переглядачі.");else set_status(a,L"Не вдалося відкрити PDF.");}break;
    case ID_L_REFRESH:refresh_logs(a);break;
    case ID_L_EXPORT:{char path[ZD_PATH_MAX],err[ZD_ERROR_MAX];if(zd_log_export(a->paths,path,sizeof(path),err,sizeof(err))){wchar_t w[ZD_PATH_MAX];utf8_wide(path,w,ZD_PATH_MAX);set_status(a,L"Зведений журнал експортовано.");app_message(a,w,L"Журнали експортовано",MB_OK|MB_ICONINFORMATION);}else{wchar_t w[ZD_ERROR_MAX];utf8_wide(err,w,ZD_ERROR_MAX);app_message(a,w,L"Не вдалося експортувати журнали",MB_OK|MB_ICONWARNING);}break;}
    case ID_L_FOLDER:open_file_path(a,a->paths->logs,0);break;
    case ID_H_EXPORT:if(notify==BN_CLICKED)start_history_csv(a);break;
    case ID_S_BROWSE:choose_folder(a);break;
    case ID_S_SAVE:save_settings_from_ui(a);break;
    case ID_S_DEFAULTS:reset_settings(a);break;
    case ID_A_OPEN_DOCS:open_file_path(a,a->settings->documents_dir,0);break;
    case ID_A_OPEN_DATA:open_file_path(a,a->paths->base,0);break;
    case ID_A_OPEN_SITE:ShellExecuteW(a->window,L"open",L"https://zakon.rada.gov.ua/",NULL,NULL,SW_SHOWNORMAL);break;
    case ID_H_STATUS:if(notify==CBN_SELCHANGE)fill_history_table(a);break;
    case ID_H_DATE_CHECK:if(notify==BN_CLICKED){int on=SendMessageW(a->history_date_check,BM_GETCHECK,0,0)==BST_CHECKED;EnableWindow(a->history_from,on);EnableWindow(a->history_to,on);fill_history_table(a);}break;
    case ID_H_SEARCH:if(notify==EN_CHANGE){KillTimer(a->window,FILTER_TIMER);SetTimer(a->window,FILTER_TIMER,220,NULL);}break;
    case ID_DOC_SEARCH:if(notify==EN_CHANGE){KillTimer(a->window,DOC_FILTER_TIMER);SetTimer(a->window,DOC_FILTER_TIMER,180,NULL);}break;
    }
}
static LRESULT CALLBACK wnd_proc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp){
    App*a=(App*)GetWindowLongPtrW(wnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE){CREATESTRUCTW*c=(CREATESTRUCTW*)lp;a=(App*)c->lpCreateParams;SetWindowLongPtrW(wnd,GWLP_USERDATA,(LONG_PTR)a);a->window=wnd;g_app=a;}
    if(!a)return DefWindowProcW(wnd,msg,wp,lp);
    switch(msg){
    case WM_CREATE:SendMessageW(wnd,WM_SETICON,ICON_BIG,(LPARAM)a->app_icon_large);SendMessageW(wnd,WM_SETICON,ICON_SMALL,(LPARAM)a->app_icon_small);create_controls(a);apply_theme(a);apply_language(a);return 0;
    case WM_SIZE:layout(a);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(wnd,&ps);paint_background(a,dc);EndPaint(wnd,&ps);return 0;}
    case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;HWND control=(HWND)lp;SetTextColor(dc,a->colors.text);if(control==a->status){SetBkMode(dc,OPAQUE);SetBkColor(dc,a->colors.bg);return(LRESULT)a->bg_brush;}if(control==a->download_status){SetBkMode(dc,OPAQUE);SetBkColor(dc,a->colors.surface);return(LRESULT)a->surface_brush;}SetBkMode(dc,TRANSPARENT);return(LRESULT)GetStockObject(NULL_BRUSH);}
    case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,a->colors.text);SetBkColor(dc,a->colors.surface);return(LRESULT)a->surface_brush;}
    case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,a->colors.text);SetBkColor(dc,a->colors.surface);return(LRESULT)a->surface_brush;}
    case WM_DRAWITEM:draw_button(a,(DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_COMMAND:handle_command(a,wp);return 0;
    case WM_NOTIFY:{NMHDR*h=(NMHDR*)lp;if(h->code==NM_DBLCLK&&h->hwndFrom==a->overview_table){open_recent(a);return 0;}if(h->code==NM_DBLCLK&&h->hwndFrom==a->history_table){open_selected_history_reader(a,0);return 0;}if(h->code==NM_DBLCLK&&h->hwndFrom==a->documents_table){open_selected_local_document(a,0);return 0;}if(h->code==DTN_DATETIMECHANGE){fill_history_table(a);return 0;}return 0;}
    case WM_TIMER:if(wp==FILTER_TIMER){KillTimer(wnd,FILTER_TIMER);fill_history_table(a);return 0;}if(wp==DOC_FILTER_TIMER){KillTimer(wnd,DOC_FILTER_TIMER);render_local_documents(a);return 0;}if(wp==READER_SAVE_TIMER){reader_save_progress(a);return 0;}break;
    case WM_APP_PROGRESS:{ProgressMessage*m=(ProgressMessage*)lp;if(m){set_download_status(a,m->text);SendMessageW(a->download_progress,PBM_SETPOS,m->stage,0);free(m);}return 0;}
    case WM_APP_FINISHED:finish_download(a,(WorkerResult*)lp);return 0;
    case WM_APP_PDF_FINISHED:{PdfResult*r=(PdfResult*)lp;if(r){a->pdf_busy=0;if(a->pdf_worker){CloseHandle(a->pdf_worker);a->pdf_worker=NULL;}EnableWindow(a->reader_pdf,TRUE);set_ui_text(a,a->reader_pdf,L"Експортувати PDF");if(r->success){if(_wcsicmp(r->path,a->current_pdf_path)==0)EnableWindow(a->reader_open_pdf,TRUE);set_status(a,L"PDF сформовано й збережено поруч із текстовим файлом.");}else{set_status(a,r->error[0]?r->error:L"Не вдалося створити PDF.");app_message(a,r->error[0]?r->error:L"Не вдалося створити PDF.",L"Помилка експорту PDF",MB_OK|MB_ICONWARNING);}free(r);}return 0;}
    case WM_APP_CSV_FINISHED:{CsvResult*r=(CsvResult*)lp;if(r){a->csv_busy=0;if(a->csv_worker){CloseHandle(a->csv_worker);a->csv_worker=NULL;}EnableWindow(a->history_export,TRUE);if(r->success){wchar_t w[ZD_PATH_MAX];if(utf8_wide(r->path,w,ZD_PATH_MAX)){set_status(a,L"Усю історію експортовано у UTF-8 CSV.");app_message(a,w,L"CSV-файл створено",MB_OK|MB_ICONINFORMATION);}else set_status(a,L"CSV створено, але шлях не вдалося показати.");}else{wchar_t w[ZD_ERROR_MAX];utf8_wide(r->error,w,ZD_ERROR_MAX);set_status(a,w);app_message(a,w,L"Помилка експорту історії",MB_OK|MB_ICONWARNING);}free(r);}return 0;}
    case WM_GETMINMAXINFO:{MINMAXINFO*m=(MINMAXINFO*)lp;m->ptMinTrackSize.x=1020;m->ptMinTrackSize.y=680;return 0;}
    case WM_CLOSE:if(a->busy||a->pdf_busy||a->csv_busy){set_status(a,L"Дочекайтеся завершення поточної операції, перш ніж закривати програму.");return 0;}reader_save_progress(a);DestroyWindow(wnd);return 0;
    case WM_DESTROY:KillTimer(wnd,FILTER_TIMER);KillTimer(wnd,DOC_FILTER_TIMER);KillTimer(wnd,READER_SAVE_TIMER);reader_save_progress(a);zd_reader_bookmarks_free(&a->reader_bookmark_list);zd_history_free(&a->history);zd_local_documents_free(&a->local_documents);zd_loaded_document_free(&a->loaded_document);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(wnd,msg,wp,lp);
}
int zd_gui_run(HINSTANCE instance,ZDPaths*paths,ZDSettings*settings,ZDSqliteApi*sqlite){
    App a;memset(&a,0,sizeof(a));a.instance=instance;a.paths=paths;a.settings=settings;a.sqlite=sqlite;a.dark=!strcmp(settings->theme,"dark");a.reader_font_px=18;g_app=&a;
    HMODULE rich_edit=LoadLibraryW(L"Msftedit.dll");
    a.app_icon_large=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_ZD_APP),IMAGE_ICON,64,64,LR_DEFAULTCOLOR);
    a.app_icon_small=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_ZD_APP),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
    a.logo_mark=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_ZD_MARK),IMAGE_ICON,42,42,LR_DEFAULTCOLOR);
    WNDCLASSW wc;memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wnd_proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hIcon=a.app_icon_large?a.app_icon_large:LoadIconW(NULL,IDI_APPLICATION);wc.lpszClassName=CLASS_NAME;wc.hbrBackground=NULL;
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){if(rich_edit)FreeLibrary(rich_edit);if(a.app_icon_large)DestroyIcon(a.app_icon_large);if(a.app_icon_small)DestroyIcon(a.app_icon_small);if(a.logo_mark)DestroyIcon(a.logo_mark);return 1;}
    HWND w=CreateWindowExW(WS_EX_APPWINDOW,CLASS_NAME,L"ZakonDownloader  ·  1.3.0-C",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1360,900,NULL,NULL,instance,&a);
    if(!w){if(rich_edit)FreeLibrary(rich_edit);if(a.app_icon_large)DestroyIcon(a.app_icon_large);if(a.app_icon_small)DestroyIcon(a.app_icon_small);if(a.logo_mark)DestroyIcon(a.logo_mark);return 1;}
    ShowWindow(w,SW_SHOW);UpdateWindow(w);show_page(&a,PAGE_OVERVIEW);refresh_history(&a);fill_dashboard(&a);
    MSG msg;while(GetMessageW(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(a.worker)CloseHandle(a.worker);if(a.pdf_worker)CloseHandle(a.pdf_worker);if(a.csv_worker)CloseHandle(a.csv_worker);
    if(a.bg_brush)DeleteObject(a.bg_brush);if(a.surface_brush)DeleteObject(a.surface_brush);if(a.surface2_brush)DeleteObject(a.surface2_brush);
    if(a.app_icon_large)DestroyIcon(a.app_icon_large);if(a.app_icon_small)DestroyIcon(a.app_icon_small);if(a.logo_mark)DestroyIcon(a.logo_mark);if(a.font_normal)DeleteObject(a.font_normal);if(a.font_small)DeleteObject(a.font_small);if(a.font_title)DeleteObject(a.font_title);if(a.font_brand)DeleteObject(a.font_brand);if(a.font_bold)DeleteObject(a.font_bold);if(a.font_reader)DeleteObject(a.font_reader);
    if(rich_edit)FreeLibrary(rich_edit);return(int)msg.wParam;
}

#define _WIN32_WINNT 0x0A00
#include "http.h"
#include <windows.h>
#include <winhttp.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WINHTTP_OPTION_REDIRECT_POLICY
#define WINHTTP_OPTION_REDIRECT_POLICY 1018
#define WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS 0
#define WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP 1
#define WINHTTP_OPTION_REDIRECT_POLICY_NEVER 2
#endif
#define BODY_LIMIT (24u * 1024u * 1024u)
typedef struct { char *p; size_t n,cap; } HBuf;
static int hb_reserve(HBuf*b,size_t extra){if(b->n>BODY_LIMIT||extra>BODY_LIMIT-b->n)return 0;size_t need=b->n+extra+1;if(need<=b->cap)return 1;size_t c=b->cap?b->cap:65536;while(c<need){if(c>BODY_LIMIT/2){c=BODY_LIMIT+1;break;}c*=2;}if(c>BODY_LIMIT+1)return 0;char*q=(char*)realloc(b->p,c);if(!q)return 0;b->p=q;b->cap=c;if(!b->n)b->p[0]=0;return 1;}
static wchar_t *to_wide(const char*s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);if(n<=0)return NULL;wchar_t*w=(wchar_t*)malloc((size_t)n*sizeof(*w));if(!w)return NULL;if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)){free(w);return NULL;}return w;}
static char *decode_body(const char*p,size_t n){if(n>=3&&(unsigned char)p[0]==0xef&&(unsigned char)p[1]==0xbb&&(unsigned char)p[2]==0xbf){p+=3;n-=3;}if(n>INT_MAX)return NULL;int wn=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,(int)n,NULL,0);UINT cp=CP_UTF8;if(!wn){cp=1251;wn=MultiByteToWideChar(cp,0,p,(int)n,NULL,0);}if(!wn)return NULL;wchar_t*w=(wchar_t*)malloc(((size_t)wn+1)*sizeof(*w));if(!w)return NULL;if(MultiByteToWideChar(cp,cp==CP_UTF8?MB_ERR_INVALID_CHARS:0,p,(int)n,w,wn)!=wn){free(w);return NULL;}w[wn]=0;int bytes=WideCharToMultiByte(CP_UTF8,0,w,wn,NULL,0,NULL,NULL);if(bytes<=0){free(w);return NULL;}char*out=(char*)malloc((size_t)bytes+1);if(!out){free(w);return NULL;}WideCharToMultiByte(CP_UTF8,0,w,wn,out,bytes,NULL,NULL);out[bytes]=0;free(w);return out;}
static int contains_ci(const char*s,const char*needle){size_t n=strlen(needle);if(!s||!n)return 0;for(;*s;s++){size_t i=0;while(i<n&&s[i]&&(unsigned char)tolower((unsigned char)s[i])==(unsigned char)tolower((unsigned char)needle[i]))i++;if(i==n)return 1;}return 0;}
static int has_placeholder(const char*s){return contains_ci(s,"відбувається форматування тексту")||contains_ci(s,"Відбувається форматування тексту");}
static void errset(char*out,size_t cap,const char*s){if(out&&cap){snprintf(out,cap,"%s",s?s:"Network error");out[cap-1]=0;}}
static void winerr(char*out,size_t cap,const char*prefix,DWORD code){char msg[256]="";FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,0,msg,sizeof(msg),NULL);for(size_t i=strlen(msg);i&& (msg[i-1]=='\r'||msg[i-1]=='\n'||msg[i-1]==' ');i--)msg[i-1]=0;snprintf(out,cap,"%s%s (код %lu).",prefix,msg,(unsigned long)code);out[cap-1]=0;}
static int same_host_url(const wchar_t*url){URL_COMPONENTS c;memset(&c,0,sizeof(c));c.dwStructSize=sizeof(c);wchar_t host[256];c.lpszHostName=host;c.dwHostNameLength=255;if(!WinHttpCrackUrl(url,0,0,&c))return 0;host[c.dwHostNameLength]=0;return c.nScheme==INTERNET_SCHEME_HTTPS&&_wcsicmp(host,L"zakon.rada.gov.ua")==0;}
static wchar_t *redirect_url(const wchar_t*current,const wchar_t*location){if(!location||!*location)return NULL;if(_wcsnicmp(location,L"https://",8)==0)return _wcsdup(location);if(_wcsnicmp(location,L"http://",7)==0||wcsncmp(location,L"//",2)==0)return NULL;wchar_t out[ZD_URL_MAX];if(location[0]==L'/'){if(swprintf(out,ZD_URL_MAX,L"https://zakon.rada.gov.ua%ls",location)<0)return NULL;}else{URL_COMPONENTS c;memset(&c,0,sizeof(c));c.dwStructSize=sizeof(c);wchar_t host[256],path[ZD_URL_MAX];c.lpszHostName=host;c.dwHostNameLength=255;c.lpszUrlPath=path;c.dwUrlPathLength=ZD_URL_MAX-1;if(!WinHttpCrackUrl(current,0,0,&c))return NULL;host[c.dwHostNameLength]=0;path[c.dwUrlPathLength]=0;wchar_t*slash=wcsrchr(path,L'/');if(slash)slash[1]=0;else wcscpy(path,L"/");if(swprintf(out,ZD_URL_MAX,L"https://%ls%ls%ls",host,path,location)<0)return NULL;}return _wcsdup(out);}
static int read_response(HINTERNET session,const char*utf8_url,int*status,char**body,char*error,size_t error_cap){
    if(error&&error_cap)error[0]=0;*status=0;*body=NULL;wchar_t*url=to_wide(utf8_url);if(!url){errset(error,error_cap,"URL is not valid UTF-8.");return 0;}if(!same_host_url(url)){free(url);errset(error,error_cap,"Only HTTPS zakon.rada.gov.ua links are permitted.");return 0;}
    int success=0;
    for(int redirect=0;redirect<=4;redirect++){
        if(!same_host_url(url)){errset(error,error_cap,"Redirect to a different host was blocked.");break;}
        URL_COMPONENTS c;memset(&c,0,sizeof(c));c.dwStructSize=sizeof(c);wchar_t host[256],path[ZD_URL_MAX],extra[ZD_URL_MAX];c.lpszHostName=host;c.dwHostNameLength=255;c.lpszUrlPath=path;c.dwUrlPathLength=ZD_URL_MAX-1;c.lpszExtraInfo=extra;c.dwExtraInfoLength=ZD_URL_MAX-1;
        if(!WinHttpCrackUrl(url,0,0,&c)){winerr(error,error_cap,"Could not parse URL: ",GetLastError());break;}host[c.dwHostNameLength]=0;path[c.dwUrlPathLength]=0;extra[c.dwExtraInfoLength]=0;wchar_t resource[ZD_URL_MAX*2];if(swprintf(resource,sizeof(resource)/sizeof(resource[0]),L"%ls%ls",path,extra)<0){errset(error,error_cap,"Request URL is too long.");break;}
        HINTERNET conn=WinHttpConnect(session,host,c.nPort,0);if(!conn){winerr(error,error_cap,"Connection failed: ",GetLastError());break;}
        HINTERNET req=WinHttpOpenRequest(conn,L"GET",resource,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
        if(!req){winerr(error,error_cap,"Could not create HTTP request: ",GetLastError());WinHttpCloseHandle(conn);break;}
        int redirect_policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;if(!WinHttpSetOption(req,WINHTTP_OPTION_REDIRECT_POLICY,&redirect_policy,sizeof(redirect_policy))){winerr(error,error_cap,"Could not enforce safe redirect policy: ",GetLastError());WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}
        const wchar_t*headers=L"Accept-Language: uk-UA,uk;q=0.9,en;q=0.8\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
        BOOL ok=WinHttpSendRequest(req,headers,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(req,NULL);
        if(!ok){winerr(error,error_cap,"HTTP request failed: ",GetLastError());WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}
        DWORD code=0,size=sizeof(code);if(!WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)){winerr(error,error_cap,"Could not read HTTP status: ",GetLastError());WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}
        *status=(int)code;
        if(code==301||code==302||code==303||code==307||code==308){wchar_t location[ZD_URL_MAX];DWORD bytes=sizeof(location);if(redirect==4||!WinHttpQueryHeaders(req,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location,&bytes,WINHTTP_NO_HEADER_INDEX)){errset(error,error_cap,"The page redirect could not be safely followed.");WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}location[ZD_URL_MAX-1]=0;wchar_t*next=redirect_url(url,location);if(!next||!same_host_url(next)){free(next);errset(error,error_cap,"Redirect to another host or insecure URL was blocked.");WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}free(url);url=next;WinHttpCloseHandle(req);WinHttpCloseHandle(conn);continue;}
        if(code!=200){snprintf(error,error_cap,"HTTP %lu from zakon.rada.gov.ua.",(unsigned long)code);error[error_cap-1]=0;WinHttpCloseHandle(req);WinHttpCloseHandle(conn);break;}
        HBuf b={0};for(;;){DWORD available=0;if(!WinHttpQueryDataAvailable(req,&available)){winerr(error,error_cap,"Could not read response size: ",GetLastError());break;}if(!available)break;if(!hb_reserve(&b,available)){errset(error,error_cap,"Response exceeded the 24 MB limit or memory is unavailable.");break;}DWORD got=0;if(!WinHttpReadData(req,b.p+b.n,available,&got)){winerr(error,error_cap,"Could not read response: ",GetLastError());break;}b.n+=got;b.p[b.n]=0;}
        WinHttpCloseHandle(req);WinHttpCloseHandle(conn);
        if(!error[0]&&b.p){char*decoded=decode_body(b.p,b.n);if(decoded){*body=decoded;success=1;}else errset(error,error_cap,"The response text could not be decoded as UTF-8 or Windows-1251.");}free(b.p);break;
    }
    free(url);return success;
}
static void backoff(int attempt){static const DWORD delays[]={2000,5000,10000};Sleep(delays[attempt<3?attempt-1:2]);}
static int get_retry(HINTERNET session,const char*url,const ZDHttpSettings*settings,ZDHttpResult*out){int max=settings->max_attempts;if(max<1)max=1;if(max>10)max=10;char last[sizeof(out->error)]="";for(int attempt=1;attempt<=max;attempt++){int code=0;char*body=NULL;char one[sizeof(out->error)]="";int ok=read_response(session,url,&code,&body,one,sizeof(one));out->attempts+=1;out->status_code=code;if(ok){if(contains_ci(body,"captcha")||contains_ci(body,"recaptcha")||contains_ci(body,"challenge")){free(body);errset(out->error,sizeof(out->error),"Сервер повернув сторінку CAPTCHA/перевірки доступу. Програма не обходить такі обмеження.");return 0;}out->html=body;return 1;}snprintf(last,sizeof(last),"%s",one);if(code==400||code==401||code==403||code==404||code==429){errset(out->error,sizeof(out->error),last);return 0;}if(attempt<max){backoff(attempt);}}
    if(last[0])snprintf(out->error,sizeof(out->error),"Не вдалося завантажити після %d спроб. Остання помилка: %s",max,last);else snprintf(out->error,sizeof(out->error),"Не вдалося завантажити після %d спроб.",max);out->error[sizeof(out->error)-1]=0;return 0;}
int zd_http_fetch_document(const char*url,const ZDHttpSettings*settings,ZDHttpResult*out){if(!out)return 0;memset(out,0,sizeof(*out));char err[ZD_ERROR_MAX];if(!zd_validate_url(url,err,sizeof(err))){errset(out->error,sizeof(out->error),err);return 0;}ZDHttpSettings def={10,30,3};if(!settings)settings=&def;HINTERNET session=WinHttpOpen(L"ZakonDownloader-C/1.0 (Windows; WinHTTP)",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session){winerr(out->error,sizeof(out->error),"Could not initialize WinHTTP: ",GetLastError());return 0;}DWORD ct=(DWORD)settings->timeout_connect_seconds*1000,rt=(DWORD)settings->timeout_read_seconds*1000;if(!WinHttpSetTimeouts(session,(int)ct,(int)ct,10000,(int)rt)){winerr(out->error,sizeof(out->error),"Could not set network timeouts: ",GetLastError());WinHttpCloseHandle(session);return 0;}
    char print_url[ZD_URL_MAX];size_t n=strlen(url);while(n&&url[n-1]=='/')n--;if(n+6>=sizeof(print_url)){WinHttpCloseHandle(session);errset(out->error,sizeof(out->error),"URL is too long.");return 0;}memcpy(print_url,url,n);memcpy(print_url+n,"/print",7);
    int ok=get_retry(session,print_url,settings,out);if(ok&&!has_placeholder(out->html)){WinHttpCloseHandle(session);return 1;}if(!ok&&(out->status_code==401||out->status_code==403||out->status_code==429||contains_ci(out->error,"CAPTCHA"))){WinHttpCloseHandle(session);return 0;}free(out->html);out->html=NULL;out->error[0]=0;Sleep(1200);ok=get_retry(session,url,settings,out);WinHttpCloseHandle(session);if(ok&&has_placeholder(out->html)){free(out->html);out->html=NULL;errset(out->error,sizeof(out->error),"Сторінка містить лише JavaScript-заглушку; текст документа не отримано.");return 0;}return ok;}
void zd_http_result_free(ZDHttpResult*out){if(out){free(out->html);out->html=NULL;}}

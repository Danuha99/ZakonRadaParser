#include "../src/core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures=0;
#define CHECK(expr) do { if(!(expr)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); failures++; } } while(0)
static char *read_file(const char *path){FILE*f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)!=0){fclose(f);return NULL;}long n=ftell(f);if(n<0||n>24*1024*1024){fclose(f);return NULL;}rewind(f);char*p=(char*)malloc((size_t)n+1);if(!p){fclose(f);return NULL;}size_t got=fread(p,1,(size_t)n,f);fclose(f);p[got]='\0';return p;}
int main(int argc,char**argv){
    char error[ZD_ERROR_MAX];
    CHECK(zd_validate_url("https://zakon.rada.gov.ua/laws/show/2297-17#Text",error,sizeof(error))==1);
    CHECK(zd_validate_url("HTTPS://ZAKON.RADA.GOV.UA/laws/show/2297-17",error,sizeof(error))==1);
    CHECK(zd_validate_url("http://zakon.rada.gov.ua/laws/show/1",error,sizeof(error))==0);
    CHECK(zd_validate_url("https://evil.example/laws/show/1",error,sizeof(error))==0);
    CHECK(zd_validate_url("https://zakon.rada.gov.ua/laws/main",error,sizeof(error))==0);
    CHECK(zd_validate_url("",error,sizeof(error))==0);
    CHECK(strstr(error,"порожнім")!=NULL);

    const char *fixture=argc>1?argv[1]:"tests/fixtures/sample_rada_page.html";
    char *html=read_file(fixture);CHECK(html!=NULL);
    if(html){
        ZDDocument d;CHECK(zd_parse_html(html,&d,error,sizeof(error))==1);
        CHECK(strstr(d.title,"захист")!=NULL);
        CHECK(strcmp(d.document_type,"Закон")==0);
        CHECK(strcmp(d.adoption_date,"01.06.2010")==0);
        CHECK(strcmp(d.revision_date,"01.01.2024")==0);
        CHECK(d.content&&strlen(d.content)>100);
        CHECK(strstr(d.content,"Загальні положення")!=NULL);
        CHECK(strstr(d.content," | ")!=NULL);
        CHECK(strstr(d.content,"© Верховна Рада")==NULL);
        CHECK(strstr(d.content,"Меню навігації")==NULL);
        CHECK(strstr(d.content,"var x") == NULL);
        zd_free_document(&d);free(html);
    }
    ZDDocument empty;CHECK(zd_parse_html("",&empty,error,sizeof(error))==0);
    CHECK(strstr(error,"порожній")!=NULL);
    CHECK(zd_parse_html("<html><body><h1>Закон</h1><p>текст</p></body></html>",&empty,error,sizeof(error))==0);
    CHECK(strstr(error,"дату")!=NULL);
    const char *entity_html="<html><body><h1>Закон про &amp; зміни</h1><div class=\"doc-date\">01.01.2020</div><div class=\"law-text\"><p>A &amp; B &custom; &lt;x&gt; &#38;</p></div></body></html>";
    ZDDocument entity_doc;CHECK(zd_parse_html(entity_html,&entity_doc,error,sizeof(error))==1);
    if(entity_doc.content){CHECK(strstr(entity_doc.content,"A & B &custom; <x> &")!=NULL);zd_free_document(&entity_doc);}
    char deep[4096];size_t deep_n=0;deep_n+=(size_t)snprintf(deep+deep_n,sizeof(deep)-deep_n,"<html><body><h1>Закон</h1>");
    for(int i=0;i<260;i++)deep_n+=(size_t)snprintf(deep+deep_n,sizeof(deep)-deep_n,"<div>");
    deep[deep_n]='\0';CHECK(zd_parse_html(deep,&empty,error,sizeof(error))==0);
    char filename[256];CHECK(zd_build_filename("Про захист персональних даних","01.01.2024",filename,sizeof(filename))==1);
    CHECK(strcmp(filename,"pro_zakhyst_personalnykh_danykh_01.01.2024.txt")==0);
    CHECK(failures==0);
    if(failures){fprintf(stderr,"%d C core test(s) failed.\n",failures);return 1;}
    puts("All C core tests passed.");return 0;
}

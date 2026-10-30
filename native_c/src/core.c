#include "core.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZD_HTML_LIMIT (24u * 1024u * 1024u)
#define ZD_STEM_LIMIT 180u

typedef struct { char *s; size_t n, cap; } Buf;
typedef struct Node Node;
struct Node {
    char *tag, *id, *classes, *text;
    Node *parent, *children, *tail, *next;
    unsigned depth;
    int skip;
};

static int reserve(Buf *b, size_t extra) {
    if (extra > (size_t)-1 - b->n - 1) return 0;
    size_t need = b->n + extra + 1;
    if (need <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 128;
    while (cap < need) {
        if (cap > (size_t)-1 / 2) { cap = need; break; }
        cap *= 2;
    }
    char *p = (char *)realloc(b->s, cap);
    if (!p) return 0;
    b->s = p; b->cap = cap;
    if (!b->n) b->s[0] = '\0';
    return 1;
}
static int putn(Buf *b, const char *s, size_t n) {
    if (!reserve(b, n)) return 0;
    memcpy(b->s + b->n, s, n); b->n += n; b->s[b->n] = '\0'; return 1;
}
static int putsb(Buf *b, const char *s) { return s ? putn(b, s, strlen(s)) : 1; }
static void set_error(char *out, size_t cap, const char *s) {
    if (out && cap) { snprintf(out, cap, "%s", s ? s : ""); out[cap - 1] = '\0'; }
}
static char *dup_n(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n); p[n] = '\0'; return p;
}
static char *dup_s(const char *s) { return s ? dup_n(s, strlen(s)) : dup_n("", 0); }

static size_t utf8_decode(const unsigned char *s, unsigned *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xe0) == 0xc0 && s[1]) { *cp = ((s[0] & 31u) << 6) | (s[1] & 63u); return 2; }
    if ((s[0] & 0xf0) == 0xe0 && s[1] && s[2]) { *cp = ((s[0] & 15u) << 12) | ((s[1] & 63u) << 6) | (s[2] & 63u); return 3; }
    if ((s[0] & 0xf8) == 0xf0 && s[1] && s[2] && s[3]) { *cp = ((s[0] & 7u) << 18) | ((s[1] & 63u) << 12) | ((s[2] & 63u) << 6) | (s[3] & 63u); return 4; }
    *cp = s[0]; return 1;
}
static int utf8_put(Buf *b, unsigned cp) {
    char out[4]; size_t n;
    if (cp < 0x80) { out[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { out[0] = (char)(0xc0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); n = 2; }
    else if (cp < 0x10000) { out[0] = (char)(0xe0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 63)); out[2] = (char)(0x80 | (cp & 63)); n = 3; }
    else { out[0] = (char)(0xf0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 63)); out[2] = (char)(0x80 | ((cp >> 6) & 63)); out[3] = (char)(0x80 | (cp & 63)); n = 4; }
    return putn(b, out, n);
}
static unsigned lower_cp(unsigned cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp >= 0x0410 && cp <= 0x042f) return cp + 32;
    if (cp == 0x0404 || cp == 0x0406 || cp == 0x0407 || cp == 0x0490) return cp + 32;
    return cp;
}
static char *lower_dup(const char *s) {
    Buf b = {0};
    if (!s) return dup_s("");
    const unsigned char *p = (const unsigned char *)s;
    while (*p) { unsigned cp; size_t n = utf8_decode(p, &cp); if (!n) break; if (!utf8_put(&b, lower_cp(cp))) { free(b.s); return NULL; } p += n; }
    return b.s ? b.s : dup_s("");
}
static int has(const char *s, const char *needle) {
    char *a = lower_dup(s), *b = lower_dup(needle);
    int found = a && b && strstr(a, b) != NULL;
    free(a); free(b); return found;
}
static int starts(const char *s, const char *prefix) { return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0; }

static int append_entity(Buf *out, const char *s, size_t n) {
    if (n == 0 || n > 16) return 1; /* Unknown entity: keep its original spelling. */
    if (s[0] == '#') {
        unsigned cp = 0; size_t i = 1; int base = 10;
        if (i < n && (s[i] == 'x' || s[i] == 'X')) { base = 16; i++; }
        for (; i < n; i++) {
            int d = isdigit((unsigned char)s[i]) ? s[i] - '0' : (tolower((unsigned char)s[i]) >= 'a' && tolower((unsigned char)s[i]) <= 'f' ? tolower((unsigned char)s[i]) - 'a' + 10 : -1);
            if (d < 0 || d >= base || cp > 0x10ffffu / (unsigned)base) return 1;
            cp = cp * (unsigned)base + (unsigned)d;
        }
        if (cp && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff)) return utf8_put(out, cp) ? 2 : 0;
    }
    struct { const char *name; unsigned cp; } map[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''},
        {"nbsp", 0xa0}, {"ndash", 0x2013}, {"mdash", 0x2014}, {"lsquo", 0x2018},
        {"rsquo", 0x2019}, {"ldquo", 0x201c}, {"rdquo", 0x201d}, {"copy", 0xa9},
        {"reg", 0xae}, {"hellip", 0x2026}, {"sect", 0xa7}, {"deg", 0xb0}
    };
    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++)
        if (strlen(map[i].name) == n && _strnicmp(map[i].name, s, n) == 0) return utf8_put(out, map[i].cp) ? 2 : 0;
    return 1;
}
static char *decode_entities(const char *s, size_t n) {
    Buf b = {0};
    for (size_t i = 0; i < n;) {
        if (s[i] == '&') {
            size_t j = i + 1;
            while (j < n && j - i <= 16 && s[j] != ';' && !isspace((unsigned char)s[j]) && s[j] != '<') j++;
            if (j < n && s[j] == ';') {
                int decoded = append_entity(&b, s + i + 1, j - i - 1);
                if (!decoded) { free(b.s); return NULL; }
                if (decoded == 1 && !putn(&b, s + i, j - i + 1)) { free(b.s); return NULL; }
                i = j + 1; continue;
            }
        }
        if (!putn(&b, s + i, 1)) { free(b.s); return NULL; }
        i++;
    }
    return b.s ? b.s : dup_s("");
}

static int is_void(const char *tag) {
    const char *v[] = {"area","base","br","col","embed","hr","img","input","link","meta","param","source","track","wbr"};
    for (size_t i=0;i<sizeof(v)/sizeof(v[0]);i++) if (_stricmp(tag,v[i])==0) return 1;
    return 0;
}
static Node *new_node(const char *tag, Node *parent) {
    Node *n=(Node *)calloc(1,sizeof(*n)); if(!n)return NULL;
    n->tag=dup_s(tag); n->id=dup_s(""); n->classes=dup_s(""); n->parent=parent;
    n->depth=parent?parent->depth+1:0;
    if(!n->tag||!n->id||!n->classes){free(n->tag);free(n->id);free(n->classes);free(n);return NULL;}
    if(parent){if(parent->tail)parent->tail->next=n;else parent->children=n;parent->tail=n;}
    return n;
}
static void free_tree(Node *n) {
    while(n){Node *next=n->next;free_tree(n->children);free(n->tag);free(n->id);free(n->classes);free(n->text);free(n);n=next;}
}
static void attr_set(char **dst, const char *s, size_t n) { char *p=dup_n(s,n); if(p){free(*dst);*dst=p;} }
static int junk_tag(const char *t) {
    const char *v[]={"script","style","noscript","iframe","header","footer","nav","aside","form","button","input","select","svg","img","figure","figcaption"};
    for(size_t i=0;i<sizeof(v)/sizeof(v[0]);i++)if(_stricmp(t,v[i])==0)return 1; return 0;
}
static int ascii_has_ci(const char *s,const char *needle){size_t n=strlen(needle);if(!s||!n)return 0;for(;*s;s++)if(_strnicmp(s,needle,n)==0)return 1;return 0;}
static int junk_marker(const char *s) {
    const char *v[]={"menu","nav","header","footer","sidebar","banner","advert","cookie","breadcrumb","pagination","toolbar","search","social","share","popup","modal","overlay","tooltip","widget"};
    for(size_t i=0;i<sizeof(v)/sizeof(v[0]);i++)if(ascii_has_ci(s,v[i]))return 1; return 0;
}
static void parse_attrs(Node *node, const char *p, const char *end) {
    while(p<end){while(p<end&&(isspace((unsigned char)*p)||*p=='/'))p++;if(p>=end)break;
        const char *k=p;while(p<end&&!isspace((unsigned char)*p)&&*p!='='&&*p!='/')p++;if(p==k){p++;continue;}size_t kn=(size_t)(p-k);
        while(p<end&&isspace((unsigned char)*p))p++;const char *v="";size_t vn=0;
        if(p<end&&*p=='='){p++;while(p<end&&isspace((unsigned char)*p))p++;if(p<end&&(*p=='\''||*p=='"')){char q=*p++;v=p;while(p<end&&*p!=q)p++;vn=(size_t)(p-v);if(p<end)p++;}else{v=p;while(p<end&&!isspace((unsigned char)*p)&&*p!='/')p++;vn=(size_t)(p-v);}}
        if(kn==2&&_strnicmp(k,"id",2)==0)attr_set(&node->id,v,vn);
        else if(kn==5&&_strnicmp(k,"class",5)==0)attr_set(&node->classes,v,vn);
    }
}
static Node *parse_tree(const char *html) {
    enum { MAX_TAGS = 250000, MAX_DEPTH = 256 };
    Node *root=new_node("#root",NULL);
    if(!root)return NULL;
    Node *cur=root;
    const char *p=html;
    size_t tags=0;
    while(*p){
        if(tags>=MAX_TAGS)goto fail;
        if(*p!='<'){
            const char *q=strchr(p,'<');
            if(!q)q=p+strlen(p);
            if(q>p){
                Node*n=new_node("#text",cur);
                if(!n)goto fail;
                n->text=decode_entities(p,(size_t)(q-p));
                if(!n->text)goto fail;
            }
            p=q;
            continue;
        }
        if(starts(p,"<!--")){const char*q=strstr(p+4,"-->");p=q?q+3:p+strlen(p);continue;}
        if(starts(p,"<!")||starts(p,"<?")){const char*q=strchr(p,'>');p=q?q+1:p+strlen(p);continue;}
        const char *end=p+1;
        int quote=0;
        while(*end){
            if(quote){if(*end==quote)quote=0;}
            else if(*end=='\''||*end=='\"')quote=*end;
            else if(*end=='>')break;
            end++;
        }
        if(!*end)break;
        const char *a=p+1;
        while(a<end&&isspace((unsigned char)*a))a++;
        int closing=(a<end&&*a=='/');
        if(closing){a++;while(a<end&&isspace((unsigned char)*a))a++;}
        const char *ts=a;
        while(a<end&&!isspace((unsigned char)*a)&&*a!='/'&&*a!='>')a++;
        if(a==ts){p=end+1;continue;}
        char tag[64];
        size_t tn=(size_t)(a-ts);
        if(tn>=sizeof(tag))tn=sizeof(tag)-1;
        for(size_t i=0;i<tn;i++)tag[i]=(char)tolower((unsigned char)ts[i]);
        tag[tn]='\0';
        tags++;
        if(closing){
            Node*n=cur;
            while(n&&n!=root&&_stricmp(n->tag,tag)!=0)n=n->parent;
            if(n&&n!=root)cur=n->parent;
        }else{
            int leaf=is_void(tag)||(end>p&&end[-1]=='/');
            if(!leaf&&cur->depth>=MAX_DEPTH)goto fail;
            Node*n=new_node(tag,cur);
            if(!n)goto fail;
            parse_attrs(n,a,end);
            n->skip=junk_tag(tag)||junk_marker(n->id)||junk_marker(n->classes);
            if(!leaf)cur=n;
        }
        p=end+1;
    }
    return root;
fail:
    free_tree(root);
    return NULL;
}
static int is_heading(const char *t){return t&&t[0]=='h'&&t[1]>='1'&&t[1]<='6'&&t[2]=='\0';}
static int is_text_block(const char*t){return _stricmp(t,"p")==0||_stricmp(t,"li")==0||_stricmp(t,"dt")==0||_stricmp(t,"dd")==0||_stricmp(t,"blockquote")==0;}
static void inline_text(Node*n,Buf*b){for(;n;n=n->next){if(n->skip)continue;if(_stricmp(n->tag,"#text")==0){if(n->text&&*n->text){if(b->n&&!isspace((unsigned char)b->s[b->n-1]))putsb(b," ");putsb(b,n->text);}}else inline_text(n->children,b);}}
static void append_normalized_line(Buf*b,const char*s){Buf x={0};int space=0;const unsigned char*p=(const unsigned char*)(s?s:"");while(*p){unsigned cp;size_t n=utf8_decode(p,&cp);if(cp==0xa0||cp==0x202f||cp==0x2009)cp=' ';if(cp=='\r'||cp=='\n'||cp=='\t'||cp==' ')space=1;else{if(space&&x.n)putsb(&x," ");space=0;utf8_put(&x,cp);}p+=n;}if(x.n){if(b->n&&b->s[b->n-1]!='\n')putsb(b,"\n");putsb(b,x.s);}free(x.s);}
static void table_rows(Node*n,Buf*out){for(;n;n=n->next){if(n->skip)continue;if(_stricmp(n->tag,"tr")==0){Buf row={0};int count=0;for(Node*c=n->children;c;c=c->next){if(_stricmp(c->tag,"td")==0||_stricmp(c->tag,"th")==0){Buf cell={0};inline_text(c->children,&cell);if(cell.n){if(count++)putsb(&row," | ");append_normalized_line(&cell,""); /* no-op; see trim below */
                        while(cell.n&&isspace((unsigned char)cell.s[cell.n-1]))cell.s[--cell.n]='\0';
                        while(cell.n&&isspace((unsigned char)*cell.s)){memmove(cell.s,cell.s+1,--cell.n);cell.s[cell.n]='\0';}
                        putsb(&row,cell.s);}free(cell.s);}}
            if(row.n)append_normalized_line(out,row.s);free(row.s);
        }else table_rows(n->children,out);}}
static void extract_content(Node*n,Buf*out){for(;n;n=n->next){if(n->skip)continue;if(_stricmp(n->tag,"#text")==0){if(n->text)append_normalized_line(out,n->text);continue;}if(is_heading(n->tag)||is_text_block(n->tag)){Buf t={0};inline_text(n->children,&t);if(t.n)append_normalized_line(out,t.s);free(t.s);continue;}if(_stricmp(n->tag,"table")==0){table_rows(n->children,out);continue;}extract_content(n->children,out);}}
static void plain_text(Node*n,Buf*b){for(;n;n=n->next){if(n->skip)continue;if(_stricmp(n->tag,"#text")==0){if(n->text){putsb(b,n->text);putsb(b," ");}}else plain_text(n->children,b);}}
static Node *find_tag(Node*n,const char*tag){for(;n;n=n->next){if(n->skip)continue;if(_stricmp(n->tag,tag)==0)return n;Node*r=find_tag(n->children,tag);if(r)return r;}return NULL;}
static Node *find_id(Node*n,const char*tag,const char*id){for(;n;n=n->next){if(n->skip)continue;if((!tag||_stricmp(n->tag,tag)==0)&&_stricmp(n->id,id)==0)return n;Node*r=find_id(n->children,tag,id);if(r)return r;}return NULL;}
static Node *find_class(Node*n,const char*tag,const char*cl){for(;n;n=n->next){if(n->skip)continue;if((!tag||_stricmp(tag,"*")==0||_stricmp(n->tag,tag)==0)&&has(n->classes,cl))return n;Node*r=find_class(n->children,tag,cl);if(r)return r;}return NULL;}
static Node *content_block(Node*root){Node*n;
    if((n=find_id(root,NULL,"article")))return n;
    if((n=find_class(root,NULL,"law-text")))return n;
    if((n=find_class(root,NULL,"document-text")))return n;
    if((n=find_id(root,NULL,"content")))return n;
    if((n=find_tag(root,"article")))return n;
    if((n=find_tag(root,"main")))return n;
    if((n=find_class(root,NULL,"entry-content")))return n;
    n=find_tag(root,"body");return n?n:root;
}
static int date_at(const char*p,char out[16]){
    if(!p||!isdigit((unsigned char)p[0])||!isdigit((unsigned char)p[1])||p[2]!='.'||!isdigit((unsigned char)p[3])||!isdigit((unsigned char)p[4])||p[5]!='.'||!isdigit((unsigned char)p[6])||!isdigit((unsigned char)p[7])||!isdigit((unsigned char)p[8])||!isdigit((unsigned char)p[9]))return 0;
    int d=(p[0]-'0')*10+p[1]-'0',m=(p[3]-'0')*10+p[4]-'0',y=(p[6]-'0')*1000+(p[7]-'0')*100+(p[8]-'0')*10+p[9]-'0';
    if(y<1||m<1||m>12)return 0;int md[]={31,28,31,30,31,30,31,31,30,31,30,31};if((y%4==0&&y%100!=0)||y%400==0)md[1]=29;if(d<1||d>md[m-1])return 0;memcpy(out,p,10);out[10]='\0';return 1;
}
static int extract_date_after(const char*s,char out[16]){for(const char*p=s;*p;p++)if(date_at(p,out))return 1;return 0;}
static void extract_number(const char*s,char*out,size_t cap){if(!s||!cap)return;for(const unsigned char*p=(const unsigned char*)s;*p;p++){if(!isdigit(*p))continue;const unsigned char*q=p;while(isdigit(*q))q++;if(*q=='-'||(*q==0xe2&&q[1]==0x80&&q[2]==0x93)){
        const unsigned char*r=(*q=='-')?q+1:q+3;size_t n=0;while(r[n]&&(isalnum(r[n])||r[n]=='_'||r[n]=='-')&&n<cap-1)n++;if(n){size_t prefix=(size_t)(q-p)+((*q=='-')?1:3);if(prefix+n<cap){memcpy(out,p,prefix+n);out[prefix+n]='\0';return;}}}p=q-1;}}
static void extract_type(const char*title,char*out,size_t cap){const char*types[]={"Закон","Кодекс","Постанова","Указ","Декрет","Наказ","Розпорядження","Конституція"};for(size_t i=0;i<sizeof(types)/sizeof(types[0]);i++)if(has(title,types[i])){snprintf(out,cap,"%s",types[i]);return;}snprintf(out,cap,"Документ");}
static void extract_status(const char*s,char*out,size_t cap){if(has(s,"втратив чинність"))snprintf(out,cap,"втратив чинність");else if(has(s,"не набрав чинності"))snprintf(out,cap,"не набрав чинності");else if(has(s,"зупинено"))snprintf(out,cap,"зупинено");else snprintf(out,cap,"чинний");}

int zd_validate_url(const char *url,char *error,size_t cap){
    if(error&&cap)error[0]='\0';if(!url){set_error(error,cap,"URL не може бути порожнім.");return 0;}
    while(isspace((unsigned char)*url))url++;size_t n=strlen(url);while(n&&isspace((unsigned char)url[n-1]))n--;
    if(!n){set_error(error,cap,"URL не може бути порожнім.");return 0;}if(n>=ZD_URL_MAX){set_error(error,cap,"URL надто довгий.");return 0;}
    char clean[ZD_URL_MAX];memcpy(clean,url,n);clean[n]='\0';char*hash=strchr(clean,'#');if(hash)*hash='\0';
    const char*prefix="https://zakon.rada.gov.ua/";if(_strnicmp(clean,prefix,strlen(prefix))!=0){if(starts(clean,"http://"))set_error(error,cap,"URL повинен починатися з https://.");else if(_strnicmp(clean,"https://",8)==0)set_error(error,cap,"Дозволені лише посилання з zakon.rada.gov.ua.");else set_error(error,cap,"URL повинен починатися з https://zakon.rada.gov.ua/.");return 0;}
    const char*path=clean+strlen(prefix)-1;if(strchr(clean+8,'@')||strchr(clean+8,':')){set_error(error,cap,"URL містить неприпустимий домен або порт.");return 0;}
    if(_strnicmp(path,"/laws/show/",11)!=0||path[11]=='\0'){set_error(error,cap,"Посилання повинно вести на документ: /laws/show/<ідентифікатор>.");return 0;}return 1;
}

int zd_parse_html(const char *html,ZDDocument *doc,char *error,size_t cap){
    if(error&&cap)error[0]='\0';if(!doc){set_error(error,cap,"Внутрішня помилка парсера.");return 0;}memset(doc,0,sizeof(*doc));
    if(!html||!*html){set_error(error,cap,"Отримано порожній HTML.");return 0;}if(strlen(html)>ZD_HTML_LIMIT){set_error(error,cap,"Сторінка завелика для безпечного розбору.");return 0;}
    Node*root=parse_tree(html);if(!root){set_error(error,cap,"Не вдалося створити структуру HTML.");return 0;}
    Node*title=find_class(root,"h1","entry-title");if(!title)title=find_id(root,"h1","title");if(!title)title=find_class(root,"h1","law-title");if(!title)title=find_tag(root,"h1");
    if(title){Buf t={0};inline_text(title->children,&t);if(t.s){while(isspace((unsigned char)*t.s))memmove(t.s,t.s+1,strlen(t.s));snprintf(doc->title,sizeof(doc->title),"%s",t.s);}free(t.s);}
    Buf all={0};plain_text(root->children,&all);
    if(!doc->title[0]){Node*tt=find_tag(root,"title");if(tt){Buf t={0};inline_text(tt->children,&t);if(t.s){char*cut=strstr(t.s," - ");if(!cut)cut=strstr(t.s," | ");if(cut&&has(cut,"zakon"))*cut='\0';snprintf(doc->title,sizeof(doc->title),"%s",t.s);}free(t.s);}}
    if(!doc->title[0]){set_error(error,cap,"Не вдалося витягнути назву документа.");free(all.s);free_tree(root);return 0;}
    extract_number(all.s,doc->number,sizeof(doc->number));extract_type(doc->title,doc->document_type,sizeof(doc->document_type));extract_status(all.s,doc->status,sizeof(doc->status));
    Node*ad=find_class(root,NULL,"doc-date");if(ad){Buf t={0};inline_text(ad->children,&t);extract_date_after(t.s,doc->adoption_date);free(t.s);}
    char*low=lower_dup(all.s?all.s:"");const char*keys[]={"чинна редакція","поточна редакція","редакція від","в редакції","зі змінами"};int found=0;
    for(size_t i=0;low&&i<sizeof(keys)/sizeof(keys[0])&&!found;i++){char*k=lower_dup(keys[i]);char*p=k?strstr(low,k):NULL;if(p){size_t off=(size_t)(p-low);found=extract_date_after((all.s?all.s:"")+off,doc->revision_date);}free(k);}
    if(!found&&all.s){char best[16]="";for(const char*p=all.s;*p;p++){char d[16];if(date_at(p,d)){if(!best[0]||strcmp(d+6,best+6)>0||(strcmp(d+6,best+6)==0&&strcmp(d+3,best+3)>0)||(strcmp(d+6,best+6)==0&&strcmp(d+3,best+3)==0&&strcmp(d,best)>0))memcpy(best,d,11);p+=9;}}if(best[0]){snprintf(doc->revision_date,sizeof(doc->revision_date),"%s",best);found=1;}}
    free(low);free(all.s);
    if(!found){set_error(error,cap,"Не вдалося визначити дату чинної редакції. Файл не буде збережено.");free_tree(root);return 0;}
    Node*block=content_block(root);Buf content={0};extract_content(block,&content);
    if(!content.n){set_error(error,cap,"Не вдалося витягнути текст документа.");free(content.s);free_tree(root);return 0;}
    doc->content=content.s;free_tree(root);return 1;
}
void zd_free_document(ZDDocument*doc){if(doc){free(doc->content);doc->content=NULL;}}

static const char *translit(unsigned cp){switch(cp){
    case 0x0430:return "a";case 0x0431:return "b";case 0x0432:return "v";case 0x0433:return "h";case 0x0491:return "g";case 0x0434:return "d";case 0x0435:return "e";case 0x0454:return "ie";case 0x0436:return "zh";case 0x0437:return "z";case 0x0438:return "y";case 0x0456:return "i";case 0x0457:return "i";case 0x0439:return "i";case 0x043a:return "k";case 0x043b:return "l";case 0x043c:return "m";case 0x043d:return "n";case 0x043e:return "o";case 0x043f:return "p";case 0x0440:return "r";case 0x0441:return "s";case 0x0442:return "t";case 0x0443:return "u";case 0x0444:return "f";case 0x0445:return "kh";case 0x0446:return "ts";case 0x0447:return "ch";case 0x0448:return "sh";case 0x0449:return "shch";case 0x044c:return "";case 0x044e:return "iu";case 0x044f:return "ia";case 0x2019:case '\'':return "";default:return NULL;}}
int zd_build_filename(const char*title,const char*date,char*out,size_t cap){if(!title||!date||!out||cap<16)return 0;Buf slug={0};const unsigned char*p=(const unsigned char*)title;int underscore=0;
    while(*p){unsigned cp;size_t n=utf8_decode(p,&cp);cp=lower_cp(cp);const char*t=translit(cp);if(t){for(;*t;t++){if(*t==' '||*t=='_'||*t=='-'||*t=='.'){if(slug.n&&!underscore){putsb(&slug,"_");underscore=1;}}else if(isalnum((unsigned char)*t)){char c=(char)tolower((unsigned char)*t);putn(&slug,&c,1);underscore=0;}}}else if(cp<128&&isalnum((unsigned char)cp)){char c=(char)cp;putn(&slug,&c,1);underscore=0;}else if(cp==' '||cp=='\t'||cp=='\n'||cp=='/'||cp=='\\'||cp==':'||cp=='*'||cp=='?'||cp=='"'||cp=='<'||cp=='>'||cp=='|'){if(slug.n&&!underscore){putsb(&slug,"_");underscore=1;}}p+=n;}
    while(slug.n&&slug.s[slug.n-1]=='_')slug.s[--slug.n]='\0';char suffix[24];snprintf(suffix,sizeof(suffix),"_%s.txt",date);size_t lim=ZD_STEM_LIMIT-strlen(suffix);if(slug.n>lim)slug.n=lim;while(slug.n&&slug.s[slug.n-1]=='_')slug.n--;if(!reserve(&slug,strlen(suffix))||slug.n+strlen(suffix)+1>cap){free(slug.s);return 0;}if(slug.n)memcpy(out,slug.s,slug.n);memcpy(out+slug.n,suffix,strlen(suffix)+1);free(slug.s);return 1;
}

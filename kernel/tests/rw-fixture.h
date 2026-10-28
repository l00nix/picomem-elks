/* In-memory firmware with intentionally swallowed CLOSE and unreliable MKDIR. */
static int rw_active,rw_handle=-1,rw_mkdir_error,rw_lose_close,rw_corrupt_read;
static int rw_overcount,rw_short,rw_open_mode;
static unsigned rw_mutations,rw_total;
struct rw_object {char path[128],display[27],fcb[11];unsigned attr,size;unsigned char data[4096];};
static struct rw_object objects[80],saved_object;
static char rw_dir[128];
static unsigned rw_cursor;
static int rw_find(const char *p)
{for(unsigned j=0;j<rw_total;j++)if(!strcmp(objects[j].path,p))return j;return -1;}
static void rw_fcb(unsigned char *dst,const char *p)
{
    const char *s=strrchr(p,'\\');if(s)p=s+1;
    memset(dst,' ',11);unsigned n=0;
    for(;*p;p++) {if(*p=='.')n=8;else {assert(n<11);dst[n++]=*p;}}
}
static void rw_entry(unsigned char *p,int j)
{
    memset(p,0,25);p[0]=objects[j].attr;
    if(objects[j].fcb[0])memcpy(p+1,objects[j].fcb,11);
    else rw_fcb(p+1,objects[j].path);
    le32(p+16,objects[j].size);le16(p+20,3);
}
static int rw_add(const char *p,int attr)
{
    assert(rw_total<80);int n=rw_total++;memset(&objects[n],0,sizeof(objects[n]));
    strcpy(objects[n].path,p);objects[n].attr=attr;return n;
}
static void respond_rw(unsigned cmd,unsigned req)
{
    unsigned char *p=mmio+BASE;
    unsigned ax=0,len=60,n=req-60,mode=p[60],action=rd16(p+62);
    unsigned long off;
    int j=-1;
    char request[128];
    if(cmd==0x1b) {
        assert(n<128);memcpy(rw_dir,p+61,n-13);rw_dir[n-13]=0;rw_cursor=0;
    }
    if(cmd==0x1b||cmd==0x1c) {
        while(rw_cursor<rw_total) {
            j=rw_cursor++;size_t prefix=strlen(rw_dir);
            if(objects[j].path[0] && !strncmp(objects[j].path,rw_dir,prefix) &&
               !strchr(objects[j].path+prefix,'\\'))break;
            j=-1;
        }
        if(j<0)ax=18;else {rw_entry(p+60,j);le16(p+80,1);len=84;}
    } else if(cmd==0x16||cmd==0x17||cmd==0x2e) {
        assert(n>=7&&n-6<128);memcpy(request,p+66,n-6);request[n-6]=0;
        j=rw_find(request);assert(rw_handle<0);
        if(cmd==0x2e && j>=0)ax=5;
        else if(cmd==0x16 && j<0)ax=2;
        else {
            if(cmd==0x2e) {assert(mode==2&&action==16&&p[64]==2);j=rw_add(request,32);rw_mutations++;}
            if(cmd==0x17) {assert(j>=0);objects[j].size=0;memset(objects[j].data,0,4096);mode=2;rw_mutations++;}
            saved_object=objects[j];rw_handle=j;rw_open_mode=mode;opened++;
            rw_entry(p+60,j);le16(p+82,cmd==0x16?1:2);p[84]=mode;len=85;
        }
    } else if(cmd==0x09) {
        assert(drv==20&&rw_handle>=0&&rw_open_mode==2&&rd16(p+64)==3);
        off=dword(p+60);n-=6;assert(off<=objects[rw_handle].size&&off+n<=4096);
        unsigned written=rw_short>=0 && (unsigned)rw_short<n?(unsigned)rw_short:n;
        memcpy(objects[rw_handle].data+off,p+66,written);
        if(off+written>objects[rw_handle].size)objects[rw_handle].size=off+written;
        rw_mutations++;le16(p+60,rw_overcount?n+1:written);len=62;
    } else if(cmd==0x08) {
        assert(rw_handle>=0&&rd16(p+64)==3);off=dword(p+60);n=rd16(p+66);
        assert(off+n<=objects[rw_handle].size);
        memcpy(p+60,objects[rw_handle].data+off,n);if(rw_corrupt_read)p[60]^=1;len=60+n;
    } else if(cmd==0x06) {
        assert(rw_handle>=0&&rd16(p+60)==3);
        if(rw_lose_close&&rw_open_mode==2)objects[rw_handle]=saved_object;
        rw_handle=-1;closed++;
    } else if(cmd==0x0f||cmd==0x03||cmd==0x01||cmd==0x13) {
        assert(n<128);memcpy(request,p+60,n);request[n]=0;j=rw_find(request);
        if(cmd==0x0f) {
            if(j<0)ax=2;else {memset(p+60,0,9);le32(p+64,objects[j].size);p[68]=objects[j].attr;len=69;}
        } else if(cmd==0x03) {
            if(j>=0)ax=29;else {rw_add(request,16);rw_mutations++;if(rw_mkdir_error)ax=29;}
        } else {
            if(j<0)ax=2;else {objects[j].path[0]=0;rw_mutations++;}
        }
    } else assert(!"unsupported RW firmware operation");
    le16(p+52,len);le16(p+58,ax);
}

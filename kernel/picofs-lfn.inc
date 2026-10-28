/* SPDX-License-Identifier: GPL-2.0-or-later
 * Display names for the exact experimental ROM descriptor.
 * No long name is ever sent to legacy OPEN or another mutation command.
 */
static int FARPROC lfn_equal(const char *a,const char *b)
{
    unsigned char x,y;
    do {
        x=*a++;y=*b++;
        if(x>='a'&&x<='z')x-=32;
        if(y>='a'&&y<='z')y-=32;
        if(x!=y)return 0;
    }while(x);
    return 1;
}
static int FARPROC lfn_valid(const unsigned char *s,unsigned n)
{
    unsigned i;
    if(!n||n>26||s[n-1]==' '||s[n-1]=='.')return 0;
    for(i=0;i<n;i++)if(s[i]<32||s[i]>126||strchr("\\/:*?\"<>|",s[i]))return 0;
    return 1;
}
static int FARPROC lfn_query(struct pm_volume *v,unsigned op,unsigned n)
{
    memcpy(packet+60,"PMLN",4);packet[64]=1;packet[65]=op;
    packet[66]=n;packet[67]=0;
    if(n)memcpy(packet+68,path,n);
    return transaction(v->drive,LFN_CMD,8+n,LFN_SIZE);
}
static int FARPROC lfn_bad(void){broken=1;return -EIO;}
static int FARPROC lfn_header(unsigned op)
{
    return !memcmp(packet+60,"PMLN",4)&&packet[64]==1&&packet[65]==op;
}
static int FARPROC lfn_cap(struct pm_volume *v)
{
    unsigned i;
    int r=lfn_query(v,0,0);
    if(r<0)return r;
    if(!lfn_header(0)||packet[66]||packet[67]!=26)return lfn_bad();
    for(i=68;i<112;i++)if(packet[i])return lfn_bad();
    return 0;
}
static int FARPROC lfn_scan(struct pm_volume *v,unsigned start)
{
    unsigned i,j,n,status;
    unsigned char reject[NODES];
    int r;
    memset(reject,0,sizeof(reject));
    for(i=start;i<v->used;i++) {
        v->lfn[i][0]=0;
        r=makepath(v,i);if(r)return r;
        r=lfn_query(v,1,strlen(path));if(r<0)return r;
        if(!lfn_header(1))return lfn_bad();
        status=packet[66];
        if(status) {
            /* Defined fallback statuses: unsupported, missing, length, encoding. */
            if(status!=1&&status!=3&&status!=4&&status!=5)return lfn_bad();
            for(j=67;j<112;j++)if(packet[j])return lfn_bad();
            continue;
        }
        n=packet[67];
        if(!lfn_valid(packet+86,n)||packet[68]!=v->nodes[i].attr||
           dword(packet+69)!=v->nodes[i].size)return lfn_bad();
        for(j=0;j<13&&packet[73+j];j++) {}
        if(j==13||!lfn_equal((char *)packet+73,v->nodes[i].name))return lfn_bad();
        for(j=86+n;j<112;j++)if(packet[j])return lfn_bad();
        memcpy(v->lfn[i],packet+86,n);v->lfn[i][n]=0;
    }
    /* Decide before clearing any candidate; include every sibling alias. */
    for(i=start;i<v->used;i++)if(v->nodes[i].name[0]&&v->lfn[i][0]) {
        for(j=1;j<v->used;j++)if(v->nodes[j].name[0]&&i!=j&&v->nodes[i].parent==v->nodes[j].parent) {
            if(lfn_equal(v->lfn[i],v->nodes[j].name))reject[i]=1;
            if(v->lfn[j][0]&&lfn_equal(v->lfn[i],v->lfn[j]))reject[i]=reject[j]=1;
        }
    }
    for(i=1;i<v->used;i++)if(reject[i])v->lfn[i][0]=0;
    return 0;
}

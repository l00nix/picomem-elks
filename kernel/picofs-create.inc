/* SPDX-License-Identifier: GPL-2.0-or-later
 * Optional v1 USB creation. Separate descriptor; legacy firmware never probed.
 */
static int FARPROC lfn_create_query(unsigned op,unsigned p,unsigned n)
{
    memcpy(packet+60,"PMLW",4);packet[64]=1;packet[65]=op;
    packet[66]=p;packet[67]=n;
    return transaction(20,LFN_CREATE_CMD,8+p+n,52);
}
static int FARPROC lfn_create_header(unsigned op)
{return !memcmp(packet+60,"PMLW",4)&&packet[64]==1&&packet[65]==op;}
static int FARPROC lfn_create_cap(void)
{
    unsigned j;int r=lfn_create_query(0,0,0);if(r<0)return r;
    if(!lfn_create_header(0)||packet[66]||packet[67]!=26||packet[68]!=3)return lfn_bad();
    for(j=69;j<112;j++)if(packet[j])return lfn_bad();
    return 0;
}
static int FARPROC lfn_create_name(char *out,const char *name,unsigned len)
{
    if(!len||len>26)return -ENAMETOOLONG;
    if(verified_memcpy_fromfs(out,(void *)name,len))return -EFAULT;
    if(!lfn_valid((unsigned char *)out,len)||out[0]==' '||out[0]=='.')return -EINVAL;
    out[len]=0;return 0;
}
/* Caller has scanned the parent, reserved an inode, and owns writing/busy. */
static int FARPROC lfn_create_new(struct pm_volume *v,unsigned parent,
                                 const char *name,int directory,char *alias)
{
    unsigned p,n=strlen(name),j,status;int r=makepath(v,parent);
    if(r)return r;
    p=strlen(path);if(p!=1&&p+13>=PATHMAX)return -ENAMETOOLONG;
    memcpy(packet+68,path,p);memcpy(packet+68+p,name,n);
    r=lfn_create_query(directory?2:1,p,n);if(r<0)return r;
    if(!lfn_create_header(directory?2:1))return lfn_bad();
    status=packet[66];
    if(status) {
        for(j=67;j<112;j++)if(packet[j])return lfn_bad();
        /* Only defined non-mutating refusals are recoverable. */
        if(status==1)return -ENOSYS;
        if(status==2)return -EINVAL;
        if(status==3)return -ENOENT;
        if(status==7)return -EEXIST;
        if(status==8)return -EACCES;
        return lfn_bad();
    }
    if(packet[67]!=n||packet[68]!=(directory?16:32)||dword(packet+69)||
       memcmp(packet+86,name,n))return lfn_bad();
    for(j=86+n;j<112;j++)if(packet[j])return lfn_bad();
    for(j=0;j<13&&packet[73+j];j++) {}
    if(!j||j==13)return lfn_bad();
    /* Reuse strict canonical SFN validation, never accept a long reply alias. */
    {
        unsigned k,base=0,ext=0,dot=0;unsigned char c;
        for(k=0;k<j;k++) {
            c=packet[73+k];
            if(c=='.'){if(dot||!base)return lfn_bad();dot=1;continue;}
            if(c<=32||c>=127||strchr("\\/:*?\"<>|+[],;=",c))return lfn_bad();
            if(dot){if(++ext>3)return lfn_bad();}else if(++base>8)return lfn_bad();
        }
        if(dot&&!ext)return lfn_bad();
    }
    for(j++;j<13;j++)if(packet[73+j])return lfn_bad();
    strcpy(alias,(char *)packet+73);
    for(j=0;alias[j];j++)if(alias[j]>='a'&&alias[j]<='z')alias[j]-=32;
    return 0;
}

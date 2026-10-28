/* Firmware reply fixture for exercising the actual ELKS client. */
static unsigned lfn_calls,lfn_fault,lfn_fallback;
static const char *file_display="Long Filename.txt",*dir_display="Long Directory";
static void marker(void)
{ memcpy(mmio+0x3fc0,"PMLN\001\160\032\064l00nix\000\000",16); }
static void respond_lfn(void)
{
    unsigned char *p=mmio+BASE;
    unsigned op=p[65],n=p[66],size=700,attr=32;
    char alias[128];const char *a,*name;
    lfn_calls++;
    assert(rd16(p+52)==68+n && !memcmp(p+60,"PMLN",4));
    assert(p[64]==1 && p[67]==0 && n<128);
    memcpy(alias,p+68,n);alias[n]=0;
    a=drv==20?"FIXTURE.TXT":"SDTEST.TXT";name=file_display;
#ifdef PICORW_TEST
    if(op && rw_active) {
        int object=rw_find(alias);
        assert(object>=0);
        a=strrchr(objects[object].path,'\\')+1;
        name=objects[object].display[0]?objects[object].display:a;
        size=objects[object].size;attr=objects[object].attr;
    } else
#endif
    if(op) {
        if(!strcmp(alias,"\\DIR")){a="DIR";name=dir_display;size=0;attr=16;}
        else if(!strcmp(alias,"\\DIR\\SUB.TXT")){a="SUB.TXT";name="Nested file.txt";size=17;}
        else assert(!strcmp(alias,drv==20?"\\FIXTURE.TXT":"\\SDTEST.TXT"));
    } else assert(!n);
    memset(p+60,0,52);memcpy(p+60,"PMLN",4);p[64]=1;p[65]=op;
    if(!op)p[67]=26;
    else if(lfn_fallback)p[66]=lfn_fallback;
    else {p[67]=strlen(name);p[68]=attr;le32(p+69,size);strcpy((char*)p+73,a);memcpy(p+86,name,strlen(name));}
    if(lfn_fault==1)p[64]=2;
    if(lfn_fault==2&&op)p[67]=27;
    if(lfn_fault==3&&op)p[73]='X';
    if(lfn_fault==4&&op)p[69]++;
    if(lfn_fault==5&&op)p[111]=1;
    le16(p+52,lfn_fault==6?111:112);le16(p+58,0);
}

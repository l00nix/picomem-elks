static unsigned rename_calls,rename_status,rename_fault;
static void rename_marker(void)
{memcpy(mmio+0x3fe0,"PMLR\001\162\032\064l00nix\000\000",16);}
static void respond_rename(void)
{
 unsigned char *p=mmio+BASE;unsigned op=p[65],pn=p[66],n=p[67];char src[128],name[27],alias[13],dest[128];int obj;
 rename_calls++;assert(drv==20&&pn<=127&&n<=26&&rd16(p+52)==73+pn+n);
 assert(!memcmp(p+60,"PMLR",4)&&p[64]==1&&op<=1);
 memcpy(src,p+73,pn);src[pn]=0;memcpy(name,p+73+pn,n);name[n]=0;
 if(op){obj=rw_find(src);assert(obj>=0&&objects[obj].size==dword(p+69)&&objects[obj].attr==p[68]);}else obj=0;
 memset(p+60,0,52);memcpy(p+60,"PMLR",4);p[64]=1;p[65]=op;
 if(!op){p[67]=26;p[68]=1;}
 else if(rename_status)p[66]=rename_status;
 else {
  strcpy(alias,"RENAM~1.TXT");strcpy(dest,src);strcpy(strrchr(dest,'\\')+1,alias);
  strcpy(objects[obj].path,dest);strcpy(objects[obj].display,name);objects[obj].attr|=32;rw_mutations++;
  p[67]=n;p[68]=objects[obj].attr;le32(p+69,objects[obj].size);strcpy((char*)p+73,alias);memcpy(p+86,name,n);
 }
 if(rename_fault==1)p[64]=2;
 if(rename_fault==2&&op)p[73]='/';
 if(rename_fault==3&&op)p[86]='?';
 if(rename_fault==4&&op)p[69]++;
 if(rename_fault==5&&!op)p[68]=3;
 if(rename_fault==6&&op)strcpy((char*)p+73,"DIR");
 le16(p+52,112);le16(p+58,0);
}

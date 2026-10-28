/* Model the separate v1 creation protocol; all content I/O remains legacy. */
static unsigned create_calls,create_status,create_fault;
static void create_marker(void)
{memcpy(mmio+0x3fd0,"PMLW\001\161\032\064l00nix\000\000",16);}
static void respond_create(void)
{
 unsigned char *p=mmio+BASE;unsigned op=p[65],pn=p[66],n=p[67],j;char parent[128],name[27],alias[13],full[128];
 create_calls++;assert(drv==20&&pn<=127&&n<=26&&rd16(p+52)==68+pn+n);
 assert(!memcmp(p+60,"PMLW",4)&&p[64]==1&&op<=2);
 memcpy(parent,p+68,pn);parent[pn]=0;memcpy(name,p+68+pn,n);name[n]=0;
 memset(p+60,0,52);memcpy(p+60,"PMLW",4);p[64]=1;p[65]=op;
 if(!op){p[67]=26;p[68]=3;}
 else {
  p[66]=create_status;
  if(!create_status) {
   snprintf(alias,sizeof(alias),"NEW%u~1.TXT",rw_total);
   strcpy(full,parent);if(strcmp(parent,"\\"))strcat(full,"\\");strcat(full,alias);
   j=rw_add(full,op==1?32:16);strcpy(objects[j].display,name);rw_mutations++;
   p[67]=n;p[68]=objects[j].attr;strcpy((char*)p+73,alias);memcpy(p+86,name,n);
  }
 }
 if(create_fault==1)p[64]=2;
 if(create_fault==2&&op)p[73]='/';
 if(create_fault==3&&op)p[86]='?';
 if(create_fault==4&&op)p[69]=1;
 if(create_fault==5&&op)strcpy((char*)p+73,"FIXTURE.TXT");
 if(create_fault==6&&!op)p[68]=7;
 le16(p+52,112);le16(p+58,0);
}

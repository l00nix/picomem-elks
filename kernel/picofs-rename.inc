/* SPDX-License-Identifier: GPL-2.0-or-later
 * Same-parent regular-file rename; VFS callout bypasses link/unlink fallback.
 */
static int FARPROC lfn_rename_query(unsigned op,unsigned p,unsigned n)
{
 memcpy(packet+60,"PMLR",4);packet[64]=1;packet[65]=op;packet[66]=p;packet[67]=n;
 return transaction(20,LFN_RENAME_CMD,13+p+n,52);
}
static int FARPROC lfn_rename_cap(void)
{
 unsigned j;int r;
 memset(packet+68,0,5);r=lfn_rename_query(0,0,0);if(r<0)return r;
 if(memcmp(packet+60,"PMLR",4)||packet[64]!=1||packet[65]||packet[66]||packet[67]!=26||packet[68]!=1)return lfn_bad();
 for(j=69;j<112;j++)if(packet[j])return lfn_bad();
 return 0;
}
static int FARPROC rename_inner(struct inode *old_dir,const char *old_name,size_t old_len,
 struct inode *new_dir,const char *new_name,size_t new_len)
{
 struct pm_volume *v=old_dir->i_sb->u.generic_sbp;struct inode *i=NULL;
 unsigned parent=(unsigned)old_dir->i_ino-1,node,target,p,n,j,status;unsigned char attr;
 char old[27],name[27],alias[13];int r;
 if(old_dir->i_sb!=new_dir->i_sb||old_dir->i_ino!=new_dir->i_ino)return -EXDEV;
 r=writable(old_dir);if(r)return r;
 if(!v->can_rename)return -ENOSYS;
 r=lfn_create_name(old,old_name,old_len);if(r)return r;
 r=lfn_create_name(name,new_name,new_len);if(r)return r;
 r=begin_write(old_dir);if(r)return r;
 r=scan(v,parent);if(r)goto done;
 if(v->nodes[parent].scanned!=1){r=-EACCES;goto done;}
 node=find_child(v,parent,old);if(!node){r=-ENOENT;goto done;}
 if(v->nodes[node].attr&16){r=-EISDIR;goto done;}
 if(v->nodes[node].attr&~0x26){r=-EACCES;goto done;}
 target=find_child(v,parent,name);if(target&&target!=node){r=-EEXIST;goto done;}
 i=iget(old_dir->i_sb,node+1);if(!i){r=-ENOMEM;goto done;}
 if(i->i_count>1){r=-EBUSY;goto done;}
 /* Exact same display name is a no-op. Case-only changes use firmware. */
 if(!strcmp(name,v->lfn[node][0]?v->lfn[node]:v->nodes[node].name)){r=0;goto done;}
 r=makepath(v,parent);if(r)goto done;
 p=strlen(path);if(p!=1&&p+13>=PATHMAX){r=-ENAMETOOLONG;goto done;}
 r=makepath(v,node);if(r)goto done;
 p=strlen(path);n=strlen(name);attr=v->nodes[node].attr|32;
 packet[68]=v->nodes[node].attr;putword(packet+69,(unsigned)i->i_size);putword(packet+71,(unsigned)(i->i_size>>16));
 memcpy(packet+73,path,p);memcpy(packet+73+p,name,n);
 r=lfn_rename_query(1,p,n);if(r<0)goto done;
 if(memcmp(packet+60,"PMLR",4)||packet[64]!=1||packet[65]!=1){r=lfn_bad();goto done;}
 status=packet[66];
 if(status) {
  for(j=67;j<112;j++)if(packet[j]){r=lfn_bad();goto done;}
  r=status==1?-ENOSYS:status==2?-EINVAL:status==3?-ENOENT:status==7?-EEXIST:status==8?-EACCES:lfn_bad();goto done;
 }
 if(packet[67]!=n||packet[68]!=attr||dword(packet+69)!=i->i_size||memcmp(packet+86,name,n)){r=lfn_bad();goto done;}
 for(j=86+n;j<112;j++)if(packet[j]){r=lfn_bad();goto done;}
 for(j=0;j<13&&packet[73+j];j++){}
 if(!j||j==13){r=lfn_bad();goto done;}
 /* Validate reply alias as an 8.3 component without copying from user space. */
 {
  unsigned k,base=0,ext=0,dot=0;unsigned char c;
  for(k=0;k<j;k++) {
   c=packet[73+k];
   if(c=='.'){if(dot||!base){r=lfn_bad();goto done;}dot=1;continue;}
   if(c<=32||c>=127||strchr("\\/:*?\"<>|+[],;=",c)){r=lfn_bad();goto done;}
   if(dot){if(++ext>3){r=lfn_bad();goto done;}}else if(++base>8){r=lfn_bad();goto done;}
  }
  if(dot&&!ext){r=lfn_bad();goto done;}
 }
 for(j++;j<13;j++)if(packet[73+j]){r=lfn_bad();goto done;}
 strcpy(alias,(char*)packet+73);for(j=0;alias[j];j++)if(alias[j]>='a'&&alias[j]<='z')alias[j]-=32;
 target=find_child(v,parent,alias);if(target&&target!=node){r=lfn_bad();goto done;}
 /* Verify disappearance only if the short alias changed. */
 if(strcmp(alias,v->nodes[node].name)) {
  r=makepath(v,node);if(r){r=lfn_bad();goto done;}
  r=attr_rw();if(r!=-ENOENT){r=lfn_bad();goto done;}
 }
 r=makepath(v,parent);if(r){r=lfn_bad();goto done;}
 p=strlen(path);if(p!=1)path[p++]='\\';
 strcpy(path+p,alias);
 r=attr_rw();if(r<0||dword(packet+64)!=i->i_size||packet[68]!=attr){r=lfn_bad();goto done;}
 strcpy(v->nodes[node].name,alias);strcpy(v->lfn[node],name);v->nodes[node].attr=attr;r=0;
 done:if(i)iput(i);return end_write(r);
}
int picofs_rename(struct inode *old_dir,const char *old_name,size_t old_len,
 struct inode *new_dir,const char *new_name,size_t new_len)
{return rename_inner(old_dir,old_name,old_len,new_dir,new_name,new_len);}

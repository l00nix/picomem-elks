/* SPDX-License-Identifier: GPL-2.0-or-later
 * Keep the existing ELKS rename behavior for other filesystems.
 * References belong here; the PicoMEM callback does not consume directories.
 */
#ifdef CONFIG_FS_PICOMEM_LFN_RENAME
static int FARPROC rename_picofs_path(char *oldpath,char *newpath,int *handled)
{
 struct inode *old=NULL,*new=NULL;const char *oldname,*newname;
 size_t oldlen,newlen;int r;
 *handled=1;
 r=dir_namei(oldpath,&oldlen,&oldname,NULL,&old);if(r)return r;
 r=dir_namei(newpath,&newlen,&newname,NULL,&new);if(r)goto done;
 if(old->i_sb->s_type->type!=FST_PICOFS&&new->i_sb->s_type->type!=FST_PICOFS){*handled=0;r=0;goto done;}
 if(old->i_sb!=new->i_sb||old->i_ino!=new->i_ino){r=-EXDEV;goto done;}
 if(!oldlen||!newlen){r=-EINVAL;goto done;}
 r=permission(old,MAY_WRITE|MAY_EXEC);if(r)goto done;
 down(&old->i_sem);
 r=picofs_rename(old,oldname,oldlen,new,newname,newlen);
 up(&old->i_sem);
 done:if(new)iput(new);iput(old);return r;
}
#endif

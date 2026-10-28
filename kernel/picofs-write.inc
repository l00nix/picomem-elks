/* SPDX-License-Identifier: GPL-2.0-or-later
 * Optional USB mutations. Included by picofs.c; l00nix, AI coding assistance.
 * Each operation owns busy through close and readback. Never issue SD writes.
 * No sparse writes, nonzero truncate, rename, hard links or open-file unlink.
 */
#define WCHUNK (CHUNK-6)
static unsigned char verify_data[WCHUNK];

static int FARPROC writable(struct inode *i)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    if (i->i_sb->s_dev!=DEV_USB || (i->i_sb->s_flags&MS_RDONLY) || !v->rw)
        return -EROFS;
    if (v->nodes[(unsigned)i->i_ino-1].attr&1) return -EACCES;
    return 0;
}
static int FARPROC begin_write(struct inode *i)
{
    int r=writable(i);
    if (!r) r=enter();
    if (!r) writing=1;
    return r;
}
static int FARPROC end_write(int r)
{ writing=0; busy=0; return r; }
static int FARPROC close_rw(unsigned fid)
{
    int r;
    if (broken) return -EIO;
    putword(packet+60,fid);r=transaction(20,0x06,2,0);
    if (r<0) broken=1;
    return r;
}
static int FARPROC attr_rw(void)
{
    unsigned n=strlen(path);
    memcpy(packet+60,path,n);
    return transaction(20,0x0f,n,9);
}
/* Paths contain only validated, bounded short components. Firmware's legacy
 * OPEN basename buffer is 16 bytes, so never pass a long name to this API. */
static int FARPROC open_rw(unsigned cmd,unsigned mode,unsigned action,
                           const char *name,unsigned long size,unsigned *fid)
{
    char reply[13];
    unsigned n=strlen(path);
    int r, good;
    memset(packet+60,0,6);packet[60]=mode;
    if (cmd==0x2e) {packet[62]=16;packet[64]=2;}
    memcpy(packet+66,path,n);
    r=transaction(20,cmd,n+6,25);
    if (r<0) return r;
    *fid=word(packet+80);
    if (*fid>=16) {broken=1;return -EIO;}
    good=!component(reply,packet+61) && !strcmp(reply,name) &&
        !(packet[60]&0x19) && dword(packet+76)==size &&
        word(packet+82)==action && packet[84]==(cmd==0x17?2:mode);
    if (!good) {close_rw(*fid);broken=1;return -EIO;}
    return 0;
}
/* Canonical 8.3 creation name; reject aliases that FatFs might silently alter. */
static int FARPROC new_name(char *out,const char *src,unsigned len)
{
    unsigned j,base=0,ext=0,dot=0;
    unsigned char c;
    if (!len || len>12) return -ENAMETOOLONG;
    if (verified_memcpy_fromfs(out,(void *)src,len)) return -EFAULT;
    for (j=0;j<len;j++) {
        c=out[j];
        if (c=='.') {if (dot || !base) return -EINVAL;dot=1;continue;}
        if (c<=32 || c>=127 || strchr("\\/:*?\"<>|+[],;=",c)) return -EINVAL;
        if (dot) {if (++ext>3) return -ENAMETOOLONG;}
        else if (++base>8) return -ENAMETOOLONG;
        if (c>='a' && c<='z') out[j]=c-32;
    }
    if (dot && !ext) return -EINVAL;
    out[len]=0;return 0;
}
static unsigned FARPROC find_child(struct pm_volume *v,unsigned parent,char *name)
{
    unsigned j;
    for (j=1;j<v->used;j++)
        if (v->nodes[j].name[0] && v->nodes[j].parent==parent &&
            (!strcmp(v->nodes[j].name,name)
#ifdef CONFIG_FS_PICOMEM_LFN
             || (v->lfn && (lfn_equal(v->nodes[j].name,name) || (v->lfn[j][0] && lfn_equal(v->lfn[j],name))))
#endif
            )) return j;
    return 0;
}
static int FARPROC create_inner(struct inode *dir,const char *name,size_t len,
                                int directory,struct inode **result)
{
    struct pm_volume *v=dir->i_sb->u.generic_sbp;
    struct pm_node *node;
    struct inode *i=NULL;
    unsigned parent=(unsigned)dir->i_ino-1,index,fid;
    char part[27];
    int r, mutation;
    *result=NULL;
#ifdef CONFIG_FS_PICOMEM_LFN_CREATE
    if(v->can_create)r=lfn_create_name(part,name,len);else
#endif
    r=new_name(part,name,len);
    if (r) return r;
    r=begin_write(dir);if (r) return r;
    r=scan(v,parent);if (r) goto done;
    if(v->nodes[parent].scanned!=1) {r=-EACCES;goto done;}
    if (find_child(v,parent,part)) {r=-EEXIST;goto done;}
    if (v->used==NODES) {r=-ENOSPC;goto done;}
    index=v->used++;node=&v->nodes[index];memset(node,0,sizeof(*node));
#ifdef CONFIG_FS_PICOMEM_LFN
    if(v->lfn)v->lfn[index][0]=0;
#endif
    node->parent=parent;node->attr=directory?16:32;
#ifdef CONFIG_FS_PICOMEM_LFN_CREATE
    if(v->can_create) {
        /* All local reservations happen before the one-shot mutation. */
        i=iget(dir->i_sb,index+1);if(!i){r=-ENOMEM;goto rollback;}
        read_inode(i);
        r=lfn_create_new(v,parent,part,directory,node->name);if(r)goto rollback;
        /* A conflicting returned alias makes completion ambiguous. */
        {
            unsigned k;
            for(k=1;k<index;k++)if(v->nodes[k].name[0]&&v->nodes[k].parent==parent&&
               (lfn_equal(v->nodes[k].name,node->name)||
                (v->lfn[k][0]&&lfn_equal(v->lfn[k],node->name)))) {
                r=lfn_bad();goto rollback;
            }
        }
        r=makepath(v,index);if(r){r=lfn_bad();goto rollback;}
        r=attr_rw();
        if(r<0||dword(packet+64)||packet[68]!=(directory?16:32)) {
            r=lfn_bad();goto rollback;
        }
        strcpy(v->lfn[index],part);if(directory)node->scanned=1;
        *result=i;return end_write(0);
    }
#endif
    strcpy(node->name,part);
    r=makepath(v,index);if (r) goto rollback;
    r=attr_rw();
    if (r>=0) {r=-EEXIST;goto rollback;}
    if (r!=-ENOENT) goto rollback;
    /* Reserve an inode before touching media. A reused cache entry is refreshed. */
    i=iget(dir->i_sb,index+1);if (!i) {r=-ENOMEM;goto rollback;}
    read_inode(i);
    if (directory) {
        unsigned n=strlen(path);
        memcpy(packet+60,path,n);
        mutation=transaction(20,0x03,n,0);
        if (broken) {r=mutation;goto rollback;}
        /* Audited firmware discards f_mkdir's return and tests an uninitialized
         * local. Verify the result without repeating the mutation. */
        r=attr_rw();
        if (r<0 || !(packet[68]&16)) {broken=1;r=-EIO;goto rollback;}
        node->scanned=1;
    } else {
        r=open_rw(0x2e,2,2,part,0,&fid);if (r) goto rollback;
        r=close_rw(fid);if (r) goto rollback;
        r=attr_rw();
        if (r<0 || (packet[68]&0x19) || dword(packet+64)) {
            broken=1;r=-EIO;goto rollback;
        }
    }
    *result=i;return end_write(0);
rollback:
    if (i) iput(i);
    memset(node,0,sizeof(*node));--v->used;
done:
    return end_write(r);
}
static int create_pm(struct inode *dir,const char *name,size_t len,mode_t mode,
                     struct inode **result)
{
    int r;
    (void)mode;r=create_inner(dir,name,len,0,result);iput(dir);return r;
}
static int mkdir_pm(struct inode *dir,const char *name,size_t len,mode_t mode)
{
    struct inode *i=NULL;
    int r;
    (void)mode;r=create_inner(dir,name,len,1,&i);if (i) iput(i);iput(dir);return r;
}
static int FARPROC remove_inner(struct inode *dir,const char *name,size_t len,
                                int directory)
{
    struct pm_volume *v=dir->i_sb->u.generic_sbp;
    struct inode *i=NULL;
    char part[27];
    unsigned node,j,n;
    int r;
#ifdef CONFIG_FS_PICOMEM_LFN
    if(v->lfn) {
        if(!len || len>26)return -ENAMETOOLONG;
        if(verified_memcpy_fromfs(part,(void *)name,len))return -EFAULT;
        if(!lfn_valid((unsigned char *)part,len))return -EINVAL;
        part[len]=0;
        for(j=0;j<len;j++)if(part[j]>='a'&&part[j]<='z')part[j]-=32;
    } else
#endif
    {r=new_name(part,name,len);if (r) return r;}
    r=begin_write(dir);if (r) return r;
    r=scan(v,(unsigned)dir->i_ino-1);if (r) goto done;
    if(v->nodes[(unsigned)dir->i_ino-1].scanned!=1) {r=-EACCES;goto done;}
    node=find_child(v,(unsigned)dir->i_ino-1,part);
    if (!node) {r=-ENOENT;goto done;}
    if (!!(v->nodes[node].attr&16)!=directory) {r=directory?-ENOTDIR:-EISDIR;goto done;}
    if (v->nodes[node].attr&1) {r=-EACCES;goto done;}
    i=iget(dir->i_sb,node+1);if (!i) {r=-ENOMEM;goto done;}
    if (i->i_count>1) {r=-EBUSY;goto done;}
    if (directory) {
        r=scan(v,node);if (r) goto done;
        if(v->nodes[node].scanned!=1) {r=-ENOTEMPTY;goto done;}
        for (j=1;j<v->used;j++) if (v->nodes[j].name[0] && v->nodes[j].parent==node) {
            r=-ENOTEMPTY;goto done;
        }
    }
    r=makepath(v,node);if (r) goto done;
    n=strlen(path);memcpy(packet+60,path,n);
    r=transaction(20,directory?0x01:0x13,n,0);
    if (r<0) {broken=1;goto done;}
    r=attr_rw();
    if (r!=-ENOENT) {broken=1;r=-EIO;goto done;}
    v->nodes[node].name[0]=0;i->i_nlink=0;r=0;
#ifdef CONFIG_FS_PICOMEM_LFN
    if(v->lfn)v->lfn[node][0]=0;
#endif
done:
    if (i) iput(i);
    return end_write(r);
}
static int unlink_pm(struct inode *dir,const char *name,size_t len)
{ int r=remove_inner(dir,name,len,0);iput(dir);return r; }
static int rmdir_pm(struct inode *dir,const char *name,size_t len)
{ int r=remove_inner(dir,name,len,1);iput(dir);return r; }

static int FARPROC truncate_inner(struct inode *i,unsigned long length)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    unsigned node=(unsigned)i->i_ino-1,fid;
    int r;
    if (length) return -EINVAL; /* do not silently claim general ftruncate */
    r=begin_write(i);if (r) return r;
    if (v->nodes[node].attr&16) {r=-EISDIR;goto done;}
    r=makepath(v,node);if (r) goto done;
    /* Check that the existing file still has the snapshotted identity/size. */
    r=open_rw(0x16,0,1,v->nodes[node].name,i->i_size,&fid);if (r) goto done;
    r=close_rw(fid);if (r) goto done;
    r=open_rw(0x17,0,2,v->nodes[node].name,0,&fid);if (r) goto done;
    r=close_rw(fid);if (r) goto done;
    r=attr_rw();
    if (r<0 || (packet[68]&0x19) || dword(packet+64)) {broken=1;r=-EIO;goto done;}
    v->nodes[node].size=i->i_size=0;r=0;
done:
    return end_write(r);
}
/* VFS callouts return errors before the generic VFS changes i_size. */
int picofs_truncate(struct inode *i,unsigned long length)
{ return truncate_inner(i,length); }

static size_t FARPROC write_inner(struct inode *i,struct file *f,char *buf,size_t count)
{
    struct pm_volume *v=i->i_sb->u.generic_sbp;
    unsigned node=(unsigned)i->i_ino-1,fid,n,got=0,verified=0,w;
    unsigned long offset,newsize;
    int r,cr;
    if (!(f->f_mode&FMODE_WRITE)) return -EBADF;
    if (f->f_pos<0) return -EINVAL;
    r=begin_write(i);if (r) return r;
    if (!count) return end_write(0);
    offset=(f->f_flags&O_APPEND)?i->i_size:(unsigned long)f->f_pos;
    if (count>1024) count=1024;
    if (offset>i->i_size || offset>0x7fffffffUL-count) return end_write(-EINVAL);
    r=makepath(v,node);if (r) goto done;
    r=open_rw(0x16,2,1,v->nodes[node].name,i->i_size,&fid);if (r) goto done;
    while (got<count) {
        n=count-got;if (n>WCHUNK) n=WCHUNK;
        r=verified_memcpy_fromfs(packet+66,buf+got,n);if (r) break;
        putword(packet+60,(unsigned)(offset+got));
        putword(packet+62,(unsigned)((offset+got)>>16));putword(packet+64,fid);
        r=transaction(20,0x09,n+6,2);
        if (r<0) {broken=1;break;}
        w=word(packet+60);
        if (w>n) {broken=1;r=-EIO;break;}
        got+=w;
        if (w<n) {r=-ENOSPC;break;}
    }
    cr=close_rw(fid);if (cr<0) {r=cr;goto done;}
    if (!got) goto done;
    newsize=i->i_size;if (offset+got>newsize) newsize=offset+got;
    /* Firmware CLOSE masks f_close errors. Reopen and compare size and every
     * accepted byte. This detects observed failures, not power-loss durability. */
    r=open_rw(0x16,0,1,v->nodes[node].name,newsize,&fid);
    if (r) {broken=1;goto done;}
    while (verified<got) {
        n=got-verified;if (n>WCHUNK) n=WCHUNK;
        r=verified_memcpy_fromfs(verify_data,buf+verified,n);if (r) break;
        putword(packet+60,(unsigned)(offset+verified));
        putword(packet+62,(unsigned)((offset+verified)>>16));
        putword(packet+64,fid);putword(packet+66,n);
        r=transaction(20,0x08,8,n);
        if (r!=(int)n || memcmp(verify_data,packet+60,n)) {r=-EIO;break;}
        verified+=n;
    }
    cr=close_rw(fid);
    if (verified!=got || cr<0) {broken=1;r=-EIO;goto done;}
    v->nodes[node].size=i->i_size=newsize;f->f_pos=offset+got;r=got;
done:
    return end_write(r);
}
static size_t write_pm(struct inode *i,struct file *f,char *buf,size_t count)
{ return write_inner(i,f,buf,count); }
static int open_pm(struct inode *i,struct file *f)
{ return (f->f_mode&FMODE_WRITE)?writable(i):0; }

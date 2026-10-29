/* SPDX-License-Identifier: GPL-2.0-or-later
 * Root-only, serialized diagnostics on a mounted USB root directory.
 */
#define PMDIAG_IOCTL 0x5044
extern void FARPROC picostack_read(unsigned char *,int);
static int FARPROC diag_inner(struct inode *i,unsigned cmd,char *arg)
{
 unsigned j,op,selector;int r;
 if(cmd!=PMDIAG_IOCTL)return -EINVAL;
 if(current->euid)return -EPERM;
 if(i->i_ino!=1||((struct pm_volume*)i->i_sb->u.generic_sbp)->drive!=20)return -EINVAL;
 r=enter();if(r)return r;
 /* Copy user request before transaction() clears the frame header. */
 r=verified_memcpy_fromfs(packet+60,arg,8);if(r)goto out;
 if(memcmp(packet+60,"PMDG",4)||packet[64]!=1||packet[65]>1||packet[67]||
    (packet[66]>=128&&packet[66]!=255)||(packet[65]&&packet[66]!=255)){r=-EINVAL;goto out;}
 op=packet[65];selector=packet[66];
 r=transaction(20,0x73,8,52);if(r<0)goto out;
 if(memcmp(packet+60,"PMDG",4)||packet[64]!=1||packet[65]!=op||packet[66]!=selector||
    (packet[67]&~3)||word(packet+86)!=4096||word(packet+84)>4096){broken=1;r=-EIO;goto out;}
 for(j=88;j<112;j++)if(packet[j]){broken=1;r=-EIO;goto out;}
 picostack_read(packet+60,op==1);
 r=verified_memcpy_tofs(arg,packet+60,52);
 out:busy=0;return r<0?r:0;
}
#ifdef PICOCFS_TEST
#define DIAG_ARG uintptr_t
#else
#define DIAG_ARG unsigned
#endif
static int diag_ioctl(struct inode *i,struct file *f,unsigned cmd,DIAG_ARG arg)
{(void)f;return diag_inner(i,cmd,(char*)arg);}
#undef DIAG_ARG

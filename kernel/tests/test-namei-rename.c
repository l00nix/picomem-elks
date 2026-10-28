/* Exercise the actual namei callout's routing, permissions and references. */
#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_FS_PICOMEM_LFN_RENAME 1
#define FARPROC
#define FST_PICOFS 4
#define MAY_WRITE 2
#define MAY_EXEC 1
struct type {int type;};
struct super {struct type *s_type;};
struct inode {struct super *i_sb;unsigned i_ino;int i_sem,refs;};
static struct type pt={4},mt={1};static struct super ps={&pt},ms={&mt},ps2={&pt};
static struct inode a,b;static int calls,resolve_error,deny,ren_error,ren_calls;
static int dir_namei(char *p,size_t *len,const char **name,void *unused,struct inode **out)
{(void)unused;calls++;if(resolve_error==calls)return -ENOENT;*out=calls==1?&a:&b;(*out)->refs++;*name=p;*len=strlen(p);return 0;}
static void iput(struct inode *i){assert(i->refs==1);i->refs--;}
static int permission(struct inode *i,int bits){assert(i==&a&&bits==3);return deny?-EACCES:0;}
static void down(int *s){assert(!*s);(*s)++;}static void up(int *s){assert(*s==1);(*s)--;}
static int picofs_rename(struct inode *old,const char *on,size_t ol,struct inode *new,const char *nn,size_t nl)
{assert(old==&a&&new==&b&&old->i_sem==1);assert(ol==strlen(on)&&nl==strlen(nn));ren_calls++;return ren_error;}
#include "../namei-picomem.inc"
static void reset(void){memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));a.i_sb=b.i_sb=&ps;a.i_ino=b.i_ino=2;calls=resolve_error=deny=ren_error=ren_calls=0;}
static void run(char *old,char *new,int error,int expected_handled,int expected_calls)
{int h=-1;assert(rename_picofs_path(old,new,&h)==error);assert(h==expected_handled&&ren_calls==expected_calls);assert(!a.refs&&!b.refs&&!a.i_sem&&!b.i_sem);}
int main(void)
{
 reset();run("old","new",0,1,1);
 reset();ren_error=-EIO;run("old","new",-EIO,1,1);
 reset();a.i_sb=b.i_sb=&ms;run("old","new",0,0,0);
 reset();b.i_sb=&ms;run("old","new",-EXDEV,1,0);
 reset();a.i_sb=&ms;run("old","new",-EXDEV,1,0);
 reset();b.i_sb=&ps2;run("old","new",-EXDEV,1,0);
 reset();b.i_ino++;run("old","new",-EXDEV,1,0);
 reset();deny=1;run("old","new",-EACCES,1,0);
 reset();run("","new",-EINVAL,1,0);
 reset();run("old","",-EINVAL,1,0);
 reset();resolve_error=1;run("old","new",-ENOENT,1,0);
 reset();resolve_error=2;run("old","new",-ENOENT,1,0);
 puts("PicoMEM namei rename: 12 routing/reference scenarios passed");return 0;
}

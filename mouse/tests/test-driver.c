#define CONFIG_FS_PICOMEM
static int fs_busy;
int picofs_busy(void) { return fs_busy; }
#define PMOUSE_HOST_TEST
#include "../mouse-picomem.c"
static struct tty tty;
static unsigned cases;
static unsigned short step(unsigned short c,unsigned char x){
    unsigned b;c^=(unsigned)x<<8;
    for(b=0;b<8;b++)c=c&0x8000?(c<<1)^0x1021:c<<1;
    return c;
}
static void reset(void){
    unsigned c,a,b;unsigned short crc=0xffff;
    if(tty.inq.base)free(tty.inq.base);
    memset(&tty,0,sizeof tty);memset(memory,0,sizeof memory);
    memset(&pm_stats,0,sizeof pm_stats);
    pm_tty=NULL;pm_claimed=pm_armed=pm_fault=pm_was_unmasked=0;
    irq_enabled=irq_busy=alloc_fail=irq_calls=command_mode=firmware_enabled=0;
    status=low=high=testport=0;port_writes=ack_writes=command=0;
    race_at_ack=race_in_queue=0;iflag=1;task.euid=0;
    fs_busy=0;
    vectors[15]=0xd0002c1eUL;
    memory[0]=0x55;memory[1]=0xaa;memory[2]=0x20;
    memcpy(memory+28,"(Date 2026-06-16)",17);
    /* Synthetic ROM: choose trailing bytes with the expected CRC. */
    for(c=0x2c1e;c<0x2d8f;c++)crc=step(crc,0);
    for(a=0;a<256;a++)for(b=0;b<256;b++)
        if(step(step(crc,a),b)==0x596f){memory[0x2d8f]=a;memory[0x2d90]=b;}
    memory[0x4000]=0x12;memory[0x401b]=11;memory[0x400b]=7;memory[0x400c]=0xa8;
    memory[CFG+210]=1;memory[CFG+215]=5;memory[CFG+217]=3;
    assert(rom_crc()==0x596f);++cases;
}
static void opened(void){assert(pm_open(&tty)==0);assert(!port_writes);assert(!irq_calls);}
static void armed(void){opened();assert(arm_mouse()==0);assert(pm_armed&&pm_claimed&&firmware_enabled&&irq_enabled);}
static void report(unsigned char x,unsigned char y,unsigned char b){memory[MX]=x;memory[MY]=y;memory[MB]=b;memory[IRR+3]=2;memory[PM_IRR]=1;pm_interrupt(7,NULL);assert(iflag==1);}
static void qreport(unsigned char x,unsigned char y,unsigned char b){
    unsigned f=iflag; iflag=0;queue_report(x,y,b);iflag=f;
}
int main(void){
    unsigned x,y,b; int dx,dy;unsigned char*p;
    reset();fs_busy=1;assert(pm_open(&tty)==-EBUSY);assert(!port_writes&&!pm_tty);
    reset();opened();fs_busy=1;assert(arm_mouse()==-EBUSY);assert(!port_writes&&!pm_claimed);
    reset();opened();pm_release(&tty);assert(!pm_tty&&!port_writes);
    reset();task.euid=1;assert(pm_open(&tty)==-EPERM);
    reset();alloc_fail=1;assert(pm_open(&tty)==-ENOMEM);assert(!port_writes);
    reset();memory[28]^=1;assert(pm_open(&tty)==-ENODEV);
    reset();memory[0x2c20]^=1;assert(pm_open(&tty)==-ENODEV);
    reset();status=1;assert(pm_open(&tty)==-EBUSY);
    reset();memory[IRR+2]=10;assert(pm_open(&tty)==-EBUSY);
    reset();opened();assert(pm_open(&tty)==-EBUSY);
    reset();opened();irq_busy=1;assert(arm_mouse()==-EBUSY);assert(!port_writes&&!pm_claimed);
    reset();opened();irq_enabled=1;vectors[15]=0x12340000;assert(arm_mouse()==-EBUSY);assert(!port_writes);
    reset();armed();report(3,4,1);assert(pm_stats.reports==1&&tty.inq.len==3&&irq_idle());assert(tty.inq.base[0]==0x29&&tty.inq.base[1]==3&&tty.inq.base[2]==252);
    assert(stop_mouse()==0);assert(!pm_claimed&&!pm_armed&&!firmware_enabled&&!irq_enabled);assert(vectors[15]==0xd0002c1eUL);pm_release(&tty);assert(!pm_tty);
    reset();armed();for(x=0;x<256;x++)for(y=0;y<256;y++)for(b=0;b<8;b++){
        tty.inq.len=tty.inq.head=0;qreport(x,y,b);p=tty.inq.base;
        dx=(signed char)p[1];dy=-(int)(signed char)p[2];
        if(tty.inq.len==6){dx+=(signed char)p[4];dy-=(signed char)p[5];assert(p[3]==(8|b));}
        assert(dx==(signed char)x&&dy==(signed char)y&&(p[0]&7)==b);
    }
    reset();armed();tty.inq.len=190;tty.inq.head=190;qreport(1,1,0);assert(tty.inq.len==190&&pm_stats.dropped==1);
    reset();armed();tty.inq.len=189;tty.inq.head=189;qreport(1,128,0);assert(tty.inq.len==189&&pm_stats.dropped==1);
    reset();armed();tty.inq.head=191;qreport(1,128,7);assert(tty.inq.len==6&&tty.inq.head==5);
    reset();armed();race_in_queue=1;report(1,2,0);assert(pm_stats.reports==2&&irq_idle());
    reset();armed();race_at_ack=1;report(1,2,0);assert(memory[PM_IRR]==1&&memory[IRR+3]==2);pm_interrupt(7,NULL);assert(pm_stats.reports==2&&irq_idle());
    reset();armed();memory[IRR+2]=10;pm_interrupt(7,NULL);assert(pm_fault&&!irq_enabled&&memory[IRR+2]==10&&!ack_writes);assert(stop_mouse()==-EIO&&pm_claimed&&!firmware_enabled);
    reset();armed();race_in_queue=200;report(1,1,0);assert(pm_fault&&!irq_enabled&&pm_stats.reports==32);
    reset();opened();command_mode=1;assert(arm_mouse()==-EIO&&pm_claimed&&!irq_enabled);assert(stop_mouse()==-EBUSY&&pm_claimed);
    reset();opened();command_mode=2;assert(arm_mouse()==-EIO&&pm_claimed&&!irq_enabled);
    reset();opened();command_mode=3;assert(arm_mouse()==-EIO&&pm_claimed&&!irq_enabled);
    reset();armed();command_mode=1;assert(stop_mouse()==-EIO&&pm_claimed&&pm_fault&&!irq_enabled);pm_release(&tty);assert(pm_tty&&tty.inq.base);
    reset();armed();assert(stop_mouse()==0);assert(arm_mouse()==0);assert(stop_mouse()==0);
    reset();armed();memory[IRR+3]=2;memory[PM_IRR]=1;assert(stop_mouse()==0&&irq_idle());
    reset();opened();irq_enabled=1;assert(arm_mouse()==0);report(1,2,1);
    assert(stop_mouse()==0&&irq_enabled&&vectors[15]==0xd0002c1eUL);
    assert(!firmware_enabled&&!pm_claimed);pm_release(&tty);
    reset();opened();irq_enabled=1;irq_busy=1;assert(arm_mouse()==-EBUSY&&irq_enabled&&!port_writes);
    reset();assert(mouse_command(0x31)==-EINVAL&&!port_writes);
    printf("%u driver scenarios; 524288 movement/button combinations passed\n",cases);
    return 0;
}

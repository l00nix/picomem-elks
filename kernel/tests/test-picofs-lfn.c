#define CONFIG_FS_PICOMEM_LFN 1
#define CONFIG_FS_PICOMEM_WRITE 1
#define PICOLFN_TEST 1
#define main base_main
#include "test-picofs.c"
#undef main
static void begin(void)
{reset();lfn_calls=lfn_fault=lfn_fallback=0;file_display="Long Filename.txt";dir_display="Long Directory";}
int main(void)
{
    struct inode *i,*d;struct file f;char buf[8];unsigned k,start;
    base_main();start=tests;
    begin();assert(read_super_pm(&usb,NULL,0));assert(!lfn_calls);pass();
    for(k=0;k<16;k++) {begin();marker();mmio[0x3fc0+k]^=1;
        assert(read_super_pm(&usb,NULL,0));assert(!lfn_calls);pass();}
    begin();marker();assert(read_super_pm(&usb,NULL,0));assert(lfn_calls==3);
    nn=0;f=(struct file){1,0,0};for(k=0;k<4;k++)assert(!readdir_pm(usb.s_mounted,&f,NULL,fill));
    assert(!strcmp(names[2],file_display)&&!strcmp(names[3],dir_display));
    i=child(usb.s_mounted,"long filename.txt");assert(i==child(usb.s_mounted,"FIXTURE.TXT"));
    f=(struct file){1,0,0};assert(read_pm(i,&f,buf,8)==8);same();
    d=child(usb.s_mounted,"Long Directory");i=child(d,"nested FILE.txt");
    assert(i==child(d,"SUB.TXT"));assert(!makepath(usb.u.generic_sbp,i->i_ino-1));
    assert(!strcmp(path,"\\DIR\\SUB.TXT"));same();pass();
    begin();marker();assert(read_super_pm(&sd,NULL,0));assert(child(sd.s_mounted,file_display));pass();
    begin();marker();usb.s_flags=0;assert(read_super_pm(&usb,NULL,0));assert(lfn_calls==3);
    assert(((struct pm_volume*)usb.u.generic_sbp)->lfn);pass();
    for(k=1;k<=6;k++){begin();marker();lfn_fault=k;assert(!read_super_pm(&usb,NULL,0));
        assert(broken&&!busy&&!mounts);pass();}
    const unsigned fallback[]={1,3,4,5};
    for(k=0;k<4;k++){begin();marker();lfn_fallback=fallback[k];assert(read_super_pm(&usb,NULL,0));
        assert(child(usb.s_mounted,"FIXTURE.TXT"));assert(!((struct pm_volume*)usb.u.generic_sbp)->lfn[1][0]);pass();}
    begin();marker();file_display="12345678901234567890123456";
    assert(read_super_pm(&usb,NULL,0));assert(child(usb.s_mounted,file_display));pass();
    begin();marker();file_display="dir";assert(read_super_pm(&usb,NULL,0));
    assert(!((struct pm_volume*)usb.u.generic_sbp)->lfn[1][0]);assert(child(usb.s_mounted,"DIR")->i_size==0);pass();
    begin();marker();dir_display=file_display;assert(read_super_pm(&usb,NULL,0));
    assert(!((struct pm_volume*)usb.u.generic_sbp)->lfn[1][0]);
    assert(!((struct pm_volume*)usb.u.generic_sbp)->lfn[2][0]);pass();
    begin();marker();test_mouse=1;assert(!read_super_pm(&usb,NULL,0));assert(!calls);pass();
    begin();marker();assert(read_super_pm(&usb,NULL,0));d=child(usb.s_mounted,"DIR");
    mmio[0x3fc0]=0;k=lfn_calls;d->i_count++;assert(lookup(d,"SUB.TXT",7,&i)==-ENODEV);
    assert(lfn_calls==k);pass();
    printf("picofs LFN: %u scenarios passed\n",tests-start);return 0;
}

#!/usr/bin/env python3
"""Host qualification of the extracted production geometry helper and reject gate.
No devices, firmware, mounts, or production writes. Host unsigned long is adapted
explicitly to uint32_t; actual packed BPB structure is extracted and adapted too.
The mount gate test stops at the first superblock assignment (not a full mount).
"""
from pathlib import Path
import hashlib,json,subprocess,re,datetime,sys
P=Path(__file__).resolve().parent
source=Path(sys.argv[1]).resolve()
S=source/'elks/fs/msdos/inode.c'
H=source/'elks/include/linuxmt/msdos_fs.h'
text=S.read_text(); header=H.read_text()
def func(src,name):
 start=src.index('static int '+name+'('); brace=src.index('{',start); n=1; end=brace+1
 while n:
  n+=(src[end]=='{')-(src[end]=='}');end+=1
 return src[start:end]
helper=func(text,'msdos_valid_geometry')
struct=header[header.index('struct msdos_boot_sector {'):header.index('struct msdos_dir_entry {')]
struct=struct.strip()
def adapt(s):
 s=s.replace('unsigned long','uint32_t').replace('unsigned short','uint16_t').replace('unsigned char','uint8_t')
 return re.sub(r'(0x[0-9a-fA-F]+|\d+)UL\b',r'\1U',s)
mount=text[text.index('static struct super_block *msdos_read_super'):]
start=mount.index('\tmap_buffer(bh);');end=mount.index('\tsb->cluster_size = b->cluster_size;')
gate=mount[start:end]
assert gate.index('msdos_valid_geometry')<gate.index('return NULL;')
assert mount.index('msdos_valid_geometry') < mount.index('msdos_prepare_rw')
assert not any(x in mount[:end] for x in ['ll_rw_blk(WRITE','mark_buffer_dirty','msdos_prepare_rw(s)'])
cases=[]
def base(kind=32,bps=512,cluster=1):
 d=dict(ignored11=bps&255,ignored12=bps>>8,cluster_size=cluster,reserved=32 if kind==32 else 1,fats=2,dir_entries=0 if kind==32 else 512,fat_length=0 if kind==32 else (12 if kind==12 else 256),fat32_length=600 if kind==32 else 0,root_cluster=2 if kind==32 else 0,total_sect=0,sectors=0,flags=0,version0=0,version1=0)
 count={12:3000,16:12000,32:70000}[kind]
 data=d['reserved']+d['fats']*(d['fat_length'] or d['fat32_length'])+d['dir_entries']*32//bps
 total=data+count*cluster
 d['total_sect']=total
 if total<65536:d['sectors']=total;d['total_sect']=0
 return d
def add(name,expect,kind=32,bps=512,cluster=1,**updates):
 d=base(kind,bps,cluster);d.update(updates);cases.append(dict(name=name,expected=expect,bps=bps,fields=d))
for kind in [12,16,32]:
 for bps in [512,1024]:
  for cluster in [1,2,4,8,16,32,64,128]:add(f'valid-fat{kind}-{bps}-cluster{cluster}',1,kind,bps,cluster)
for name,updates in [
 ('zero-bps',dict(ignored11=0,ignored12=0)),('bps-1024-on512',dict(ignored12=4)),('bps-4096',dict(ignored12=16)),
 ('zero-reserved',dict(reserved=0)),('zero-fats',dict(fats=0)),('zero-fatlength',dict(fat32_length=0)),
 ('fatlength-over16',dict(fat32_length=65536)),('fatlength-max32',dict(fat32_length=0xffffffff)),
 ('fat-multiply-wrap',dict(fat32_length=0x01010101,fats=255,reserved=1)),
 ('metadata65536',dict(fat32_length=257,fats=255,reserved=1)),('zero-total',dict(sectors=0,total_sect=0)),
 ('metadata-equals-total',dict(total_sect=1232)),('metadata-exceeds-total',dict(total_sect=1231)),
 ('zero-cluster-size',dict(cluster_size=0)),('nonpower3',dict(cluster_size=3)),('nonpower127',dict(cluster_size=127)),
 ('cluster129',dict(cluster_size=129)),('cluster255',dict(cluster_size=255)),
 ('zero-data-clusters',dict(cluster_size=128,total_sect=1233)),
 ('nonmirrored-active0',dict(flags=0x80)),('nonmirrored-active1',dict(flags=0x81)),
 ('version-low',dict(version0=1)),('version-high',dict(version1=1)),('fat32-rootentries',dict(dir_entries=512)),
 ('root0',dict(root_cluster=0)),('root1',dict(root_cluster=1)),('root-data-onepast',dict(root_cluster=70002)),
 ('root-fat-onepast',dict(fat32_length=1,root_cluster=128,total_sect=100034)),
 ('root-max32',dict(root_cluster=0xffffffff))]:add(name,0,**updates)
add('root-last-data',1,root_cluster=70001)
add('root-last-fat',1,fat32_length=1,root_cluster=127,total_sect=100034)
add('metadata65535',1,fats=1,fat32_length=2,reserved=65533,total_sect=66535)
add('max-total-with-safe-root',1,total_sect=0xffffffff)
add('mirrored-activebits-ignored',1,flags=0x0f)
add('total16-precedence',1,sectors=2000,total_sect=0xffffffff)
add('total16-too-small-precedence',0,sectors=1,total_sect=100000)
for kind in [12,16]:
 d=base(kind);data=d['reserved']+d['fats']*d['fat_length']+d['dir_entries']//16
 add(f'fat{kind}-zero-data-clusters',0,kind,cluster=128,sectors=data+1,total_sect=0)
 add(f'fat{kind}-extended-fields-ignored',1,kind,flags=0x80,version0=1,root_cluster=0xffffffff,fat32_length=0xffffffff)
add('bps512-on1024',0,bps=1024,ignored12=2)
add('root-last-fat1024',1,bps=1024,fat32_length=1,root_cluster=255,total_sect=100034)
add('root-onepast-fat1024',0,bps=1024,fat32_length=1,root_cluster=256,total_sect=100034)
# Do not imply arbitrary hostile FAT media is validated: entry chains, bad sector
# signatures and device capacity are outside this geometry helper.
pre='''#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t __u32; typedef uint16_t __u16; typedef uint8_t __u8;
struct super_block { uint16_t sector_size; uint8_t sector_bits; };
#define SECTOR_SIZE_SB(s) ((s)->sector_size)
#define SECTOR_BITS_SB(s) ((s)->sector_bits)
#define MSDOS_DIR_BITS 5
'''
pre+=adapt(struct)+'\n'+adapt(helper)+'\n'
pre+='''struct buffer_head { void *b_data; };
static int maps, releases, logs;
static void map_buffer(struct buffer_head *b) {(void)b;maps++;}
static void unmap_brelse(struct buffer_head *b) {(void)b;releases++;}
#define printk(...) (logs++)
static struct super_block *actual_mount_geometry_gate(struct super_block *s, struct buffer_head *bh) {
 struct msdos_boot_sector *b;
'''+gate+''' return s; /* Stop before first superblock assignment; not a full mount. */
}
int main(void) {
 _Static_assert(sizeof(uint32_t)==4,"32-bit arithmetic");
 _Static_assert(offsetof(struct msdos_boot_sector,total_sect)==32,"BPB layout");
 _Static_assert(offsetof(struct msdos_boot_sector,root_cluster)==44,"BPB layout");
 struct super_block s; struct msdos_boot_sector b; struct buffer_head bh={&b};
 int value, passed=0;
'''
lines=[]
for c in cases:
 lines+=['memset(&b,0,sizeof b);',f's.sector_size={c["bps"]};s.sector_bits={9 if c["bps"]==512 else 10};']
 for k,v in c['fields'].items():
  k={'ignored11':'ignored[11]','ignored12':'ignored[12]','version0':'version[0]','version1':'version[1]'}.get(k,k)
  lines.append(f'b.{k}={v}U;')
 lines += ['maps=releases=logs=0;', 'value=msdos_valid_geometry(&s,&b);',f'if(value!={c["expected"]}){{fprintf(stderr,"FAIL {c["name"]}: %d\\n",value);return 1;}}',f'assert((actual_mount_geometry_gate(&s,&bh)!=NULL)=={c["expected"]});',f'assert(maps==1 && releases=={1-c["expected"]} && logs=={1-c["expected"]});','passed++;']
source=pre+'\n'.join(lines)+'\nprintf("PASS %d geometry cases, helper and actual mount rejection gate\\n",passed);return 0;}\n'
C=P/'geometry-extracted.c';C.write_text(source);exe=P/'geometry-extracted'
cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-O1','-g',str(C),'-o',str(exe)]
compile_result=subprocess.run(cmd,text=True,capture_output=True)
run=subprocess.run([str(exe)],text=True,capture_output=True) if compile_result.returncode==0 else None
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
r=dict(complete=True,passed=bool(run and run.returncode==0),created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),cases=len(cases),case_details=cases,adaptation='Actual helper and packed BPB extracted; unsigned long/UL adapted to uint32_t/U to preserve target32 arithmetic. Host int remains32, unlike ELKS16; target build/emulator remain required.',mount_gate='Actual map/validation/release/return fragment executed for every case. Static source order proves guard precedes first geometry assignment and msdos_prepare_rw. This is not a full host mount emulation.',compile_command=cmd,compile_exit=compile_result.returncode,compile_stdout=compile_result.stdout,compile_stderr=compile_result.stderr,run_exit=run.returncode if run else None,run_stdout=run.stdout if run else None,run_stderr=run.stderr if run else None,source_sha256={str(p):sha(p) for p in [S,H,Path(__file__),C]})
(P/'geometry-host-result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:r[k] for k in ['passed','cases','compile_exit','run_exit','run_stdout','compile_stderr','run_stderr']},indent=2))
raise SystemExit(0 if r['passed'] else 1)

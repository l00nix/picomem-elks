/* Appended to the integrated actual host-control/MSC/writer harness. */
int main(void)
{
 unsigned total=0;
 for(unsigned s=0;s<NSCEN;s++) {
  init(s);int r=pmw_prepare(7);
  if(s==GOOD || s==HEADER || s==DUAL) {
   assert(r==PMW_OK && clears==(s==DUAL?2u:1u));
   unsigned n=clears;uint8_t readback[512];
   for(unsigned lba=0;lba<100;lba+=99) {
    assert(pmw_write(7,lba,payload,512)==PMW_OK);
    assert(pmw_read(7,lba,readback,512)==PMW_OK);
    assert(!memcmp(readback,payload,512) && !memcmp(media[lba],payload,512));
   }
   assert(writes==2 && clears==n && !w.dirty && !w.fault);
   assert(pmw_release(7)==PMW_OK);
   unsigned before=commands;
   assert(pmw_write(7,1,payload,512)==PMW_DENIED && commands==before);
  } else {
   assert(r==(s==RETRY_FAILED?PMW_DENIED:PMW_IO));
   unsigned before=commands;
   assert(pmw_write(7,0,payload,512)!=PMW_OK && commands==before && !writes);
  }
  total++;
 }
 /* A recovered query must not enable retries of ambiguous writes or flushes. */
 for(unsigned opi=0;opi<2;opi++)
 for(unsigned stage=MSC_STAGE_CMD;stage<=MSC_STAGE_STATUS;stage++)
 for(unsigned how=REJECT;how<=BAD_ARG;how++) {
  if(opi && stage==MSC_STAGE_DATA)continue;
  if(how>=SIGNATURE && how<=STATUS_PHASE && stage!=MSC_STAGE_STATUS)continue;
  if((how==BAD_CBW || how==BAD_ARG) && stage!=MSC_STAGE_STATUS)continue;
  init(GOOD);cache_on=true;assert(pmw_prepare(7)==PMW_OK && syncs==1);
  unsigned n=clears;
  fault_op=opi?0x35:0x2a;fault_stage=stage;inject=how;
  assert(pmw_write(7,2,payload,512)==PMW_IO);
  assert(writes==1 && w.fault && w.dirty && clears==n);
  blocked();total++;
 }
 /* Read-only ownership, stale media, bounds and lengths still deny writes. */
 init(GOOD);assert(pmw_claim(7,false)==PMW_OK);unsigned n=commands;
 assert(pmw_write(7,0,payload,512)==PMW_DENIED && commands==n);total++;
 init(GOOD);assert(pmw_prepare(7)==PMW_OK);n=commands;
 assert(pmw_write(6,0,payload,512)==PMW_STALE);
 assert(pmw_write(7,100,payload,512)==PMW_DENIED);
 assert(pmw_write(7,0,NULL,512)==PMW_DENIED);
 for(unsigned len=0;len<514;len++)if(len!=512) {
  assert(pmw_write(7,0,payload,len)==PMW_DENIED);total++;
 }
 assert(commands==n && !writes);total++;
 /* Late completion after a timed-out write never unlocks the device. */
 init(GOOD);assert(pmw_prepare(7)==PMW_OK);
 fault_op=0x2a;fault_stage=MSC_STAGE_DATA;inject=TIMEOUT;
 assert(pmw_write(7,3,payload,512)==PMW_IO && queued.set);
 inject=NONE;while(queued.set)pmw_pump();
 assert(w.fault && w.dirty && writes==1);blocked();total++;
 printf("PASS %u recovered-write scenarios; actual host-control/MSC/writer, simulated media\n",total);
 return 0;
}

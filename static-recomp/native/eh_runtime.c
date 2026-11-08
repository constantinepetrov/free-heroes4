/* GPL-3.0-or-later. Adapted from Heroes V android/host/eh.c; its pcrecomp EH32 origin is MIT, revision 904ac3129d0c9d05efbeec19a1dce6abbe7f5d26. */
#include "h4_host.h"
/* Metadata selection only: never unwind or claim that a catch has run. */
uint32_t h4_eh_select_detail(uint32_t fi,int state,uint32_t info,int32_t *low,int32_t *high,uint32_t *handler){
 h4_need(fi,20,0);h4_need(info,16,0);uint32_t nt=MEM32(fi+12),tb=MEM32(fi+16),arr=MEM32(info+12);h4_need(arr,4,0);uint32_t nc=MEM32(arr);
 if(nt>256||nc>256||arr>0xffffffffu-4u-nc*4u)void_fatal("EH metadata range exceeds limit");if(nt)h4_need(tb,nt*20,0);if(nc)h4_need(arr+4,nc*4,0);
 for(uint32_t t=0;t<nt;t++){uint32_t p=tb+t*20;int lo=(int32_t)MEM32(p),hi=(int32_t)MEM32(p+4);if(state<lo||state>hi)continue;uint32_t n=MEM32(p+12),hs=MEM32(p+16);if(n>256)void_fatal("EH catch count exceeds limit");if(n)h4_need(hs,n*16,0);
  for(uint32_t c=0;c<n;c++){uint32_t h=hs+c*16,type=MEM32(h+4);if(!type){if(low)*low=lo;if(high)*high=hi;if(handler)*handler=h;return MEM32(h+12);}h4_need(type,9,0);const char *name=h4_string(type+8);
   for(uint32_t j=0;j<nc;j++){uint32_t ct=MEM32(arr+4+j*4);h4_need(ct,8,0);uint32_t td=MEM32(ct+4);h4_need(td,9,0);if(!strcmp(name,h4_string(td+8))){if(low)*low=lo;if(high)*high=hi;if(handler)*handler=h;return MEM32(h+12);}}
  }
 }return 0;
}
uint32_t h4_eh_select(uint32_t fi,int state,uint32_t info){return eh_select_detail(fi,state,info,NULL,NULL,NULL);}
/* MSVC6 UnwindMap: validate the entire state path before invoking destructors. */
static void eh_validate_unwind(uint32_t rec,uint32_t fi,int32_t to) {
 h4_need(rec,12,1);h4_need(fi,12,0);uint32_t max=MEM32(fi+4),map=MEM32(fi+8);int32_t state=(int32_t)MEM32(rec+8),check=state;
 /* max is the number of map entries: valid indices end at max-1. */
 if(max>65536||to< -1||to>=(int32_t)max||state< -1||state>=(int32_t)max)void_fatal("invalid EH unwind state");
 if(max)h4_need(map,max*8,0);
 while(check!=to){if(check<0)void_fatal("EH unwind target not reachable");int32_t next=(int32_t)MEM32(map+8*(uint32_t)check);if(next< -1||next>=check)void_fatal("invalid EH unwind transition");check=next;}
}
void h4_eh_unwind(uint32_t rec,uint32_t fi,int32_t to) {
 eh_validate_unwind(rec,fi,to);uint32_t map=MEM32(fi+8);int32_t state=(int32_t)MEM32(rec+8);
 while(state!=to){uint32_t item=map+8*(uint32_t)state;int32_t next=(int32_t)MEM32(item);uint32_t action=MEM32(item+4);MEM32(rec+8)=(uint32_t)next;
  if(action){void_log("EH cleanup %08x frame=%08x",action,rec);uint32_t saved_sp=g_esp,saved_bp=g_ebp,saved_bx=g_ebx,saved_si=g_esi,saved_di=g_edi,saved_cur=g_cur_func;recomp_func_t fn=void_resolve(action);g_ebp=rec+12;g_esp-=4;h4_need(g_esp,4,1);MEM32(g_esp)=0;fn();if(g_esp!=saved_sp)void_fatal("EH cleanup unbalanced guest stack");g_ebp=saved_bp;g_ebx=saved_bx;g_esi=saved_si;g_edi=saved_di;g_cur_func=saved_cur;}
  state=next;
 }
}
/* Only trivial non-virtual catch copies and references are supported. */
struct eh_binding {uint32_t dst,src,size;int reference;};
static struct eh_binding eh_binding_plan(uint32_t handler,uint32_t info,uint32_t obj,uint32_t bp){
 struct eh_binding out={0};uint32_t adj=MEM32(handler),type=MEM32(handler+4);int32_t disp=(int32_t)MEM32(handler+8);
 if(!disp){if(adj)void_fatal("unsupported EH catch qualifiers");return out;}if(!type||(adj&~8u))void_fatal("unsupported EH catch binding");
 uint32_t arr=MEM32(info+12);h4_need(arr,4,0);uint32_t n=MEM32(arr);if(n>256||arr>0xffffffffu-4u-n*4u)void_fatal("invalid EH catchable types");if(n)h4_need(arr+4,n*4,0);
 uint32_t selected=0;for(uint32_t i=0;i<n;i++){uint32_t ct=MEM32(arr+4+4*i);h4_need(ct,28,0);uint32_t td=MEM32(ct+4);h4_need(td,9,0);h4_need(type,9,0);if(!strcmp(h4_string(td+8),h4_string(type+8))){selected=ct;break;}}
 if(!selected)void_fatal("EH catch type unavailable");int32_t mdisp=(int32_t)MEM32(selected+8),pdisp=(int32_t)MEM32(selected+12);uint32_t size=MEM32(selected+20),copy=MEM32(selected+24);
 if(pdisp!=-1||MEM32(selected+16)||!size||size>1048576u||(!(MEM32(selected)&1u)&&copy))void_fatal("unsupported EH catch object layout");
 int64_t src=(int64_t)obj+mdisp,dst=(int64_t)bp+disp;if(src<0||src>0xffffffffLL||dst<0||dst>0xffffffffLL)void_fatal("invalid EH catch object address");
 out.src=(uint32_t)src;out.dst=(uint32_t)dst;out.size=size;out.reference=!!(adj&8);h4_need(out.src,size,0);h4_need(out.dst,out.reference?4:size,1);return out;
}
static void eh_bind(struct eh_binding p){if(!p.dst)return;if(p.reference)MEM32(p.dst)=p.src;else memmove(GPTR(p.dst),GPTR(p.src),p.size);}
/* shortcut: frames without a live landing pad and nontrivial catch copies fail closed. */
/* Guest call entries preserve nonvolatile registers even between SEH owners. */
static struct {uint32_t va,bx,si,di;} eh_calls[4096];
static unsigned eh_call_count;
void h4_eh_call_enter(uint32_t va){if(eh_call_count==4096)void_fatal("EH guest call depth limit");eh_calls[eh_call_count++]=(typeof(eh_calls[0])){va,g_ebx,g_esi,g_edi};}
void h4_eh_call_leave(uint32_t va){if(!eh_call_count||eh_calls[eh_call_count-1].va!=va)void_fatal("EH guest call frame mismatch");--eh_call_count;}
static struct {recomp_ehframe_t *frame;unsigned depth,call_index;uint32_t bx,si,di,owner;} eh_frames[128];
static struct {uint32_t obj,info,bx,si,di;unsigned owner;} eh_contexts[32];
unsigned eh_frame_count,eh_context_count;
void recomp_eh_push(recomp_ehframe_t *f){f->resume=0;if(eh_frame_count==128)void_fatal("EH landing frame limit");f->prev=eh_frame_count?eh_frames[eh_frame_count-1].frame:NULL;eh_frames[eh_frame_count++]=(typeof(eh_frames[0])){f,eh_context_count,eh_call_count?eh_call_count-1:0xffffffffu,g_ebx,g_esi,g_edi,g_cur_func};}
void recomp_eh_pop(recomp_ehframe_t *f){if(!eh_frame_count||eh_frames[eh_frame_count-1].frame!=f)void_fatal("invalid EH landing frame pop");--eh_frame_count;}
void h4_eh_throw(uint32_t obj,uint32_t info){
 if(!info){if(!eh_context_count)void_fatal("EH rethrow outside catch");obj=eh_contexts[eh_context_count-1].obj;info=eh_contexts[eh_context_count-1].info;}
 h4_need(info,16,0);uint32_t rec=MEM32(g_fs_base),chain[64],infos[64];int frames[64];unsigned count=0;int32_t low=0,high=0;uint32_t handler=0,target=0,fi=0;
 while(rec&&rec!=0xffffffffu){
  if(count==64)void_fatal("SEH chain exceeds limit");for(unsigned i=0;i<count;i++)if(chain[i]==rec)void_fatal("SEH chain cycle");h4_need(rec,12,1);uint32_t stub=MEM32(rec+4);h4_need(stub,11,0);
  if(MEM8(stub)!=0xb8||(MEM8(stub+5)!=0xe9&&!(MEM8(stub+5)==0xff&&MEM8(stub+6)==0x25)))void_fatal("unsupported SEH handler stub");
  fi=MEM32(stub+1);h4_need(fi,20,0);if(MEM32(fi)!=0x19930520u)void_fatal("unsupported EH FuncInfo magic");
  chain[count]=rec;infos[count]=fi;frames[count]=-1;for(int i=(int)eh_frame_count-1;i>=0;i--)if(eh_frames[i].frame->entry_esp==rec+16){frames[count]=i;break;}
  target=eh_select_detail(fi,(int32_t)MEM32(rec+8),info,&low,&high,&handler);count++;if(target)break;rec=MEM32(rec);
 }
 if(!target)void_fatal("EH catch not found");h4_need(handler,16,0);struct eh_binding binding=eh_binding_plan(handler,info,obj,rec+12);
 uint32_t max_state=MEM32(fi+4);if(low<0||high<low||(uint32_t)high>=max_state)void_fatal("invalid EH try state range");
 int frame_index=frames[count-1];if(frame_index<0||eh_frames[frame_index].frame->resume==0xffffffffu)void_fatal("EH owner has no landing pad");if(eh_context_count==32)void_fatal("EH context limit");
 for(unsigned i=0;i<count;i++){if(frames[i]<0)void_fatal("EH crossed frame has no live landing pad");if(i&&frames[i]>=frames[i-1])void_fatal("EH frame order mismatch");eh_validate_unwind(chain[i],infos[i],i+1==count?low:-1);}
 unsigned call_index=eh_frames[frame_index].call_index;uint32_t target_bx=g_ebx,target_si=g_esi,target_di=g_edi;
 if(call_index!=0xffffffffu){if(call_index>=eh_call_count||eh_calls[call_index].va!=eh_frames[frame_index].owner)void_fatal("EH owner call frame is not live");if(call_index+1<eh_call_count){target_bx=eh_calls[call_index+1].bx;target_si=eh_calls[call_index+1].si;target_di=eh_calls[call_index+1].di;}}
 while(eh_context_count&&eh_contexts[eh_context_count-1].owner>(unsigned)frame_index)--eh_context_count;
 for(unsigned i=0;i<count;i++){eh_unwind(chain[i],infos[i],i+1==count?low:-1);if(call_index==0xffffffffu&&i+1<count){target_bx=eh_frames[frames[i]].bx;target_si=eh_frames[frames[i]].si;target_di=eh_frames[frames[i]].di;}}
 g_ebx=target_bx;g_esi=target_si;g_edi=target_di;if(call_index!=0xffffffffu)eh_call_count=call_index+1;eh_frame_count=(unsigned)frame_index+1;
 uint32_t guest_bp=rec+12;MEM32(g_fs_base)=rec;MEM32(rec+8)=(uint32_t)(high+1);eh_bind(binding);
 eh_contexts[eh_context_count++]=(typeof(eh_contexts[0])){obj,info,g_ebx,g_esi,g_edi,(unsigned)frame_index};
 uint32_t old_sp=g_esp,old_bp=g_ebp;recomp_func_t fn=void_resolve(target);g_ebp=guest_bp;g_esp-=4;h4_need(g_esp,4,1);MEM32(g_esp)=0;
 void_log("EH invoking catch %08x",target);fn();if(g_esp!=old_sp)void_fatal("EH catch unbalanced guest stack");uint32_t continuation=g_eax;
 if(!h4_eh_resume_allowed(eh_frames[frame_index].owner,continuation)&&!(h4_eh_test_mode()&&!eh_frames[frame_index].owner))void_fatal("EH continuation not owned by landing frame");
 unsigned restore_depth=eh_frames[frame_index].depth;if(restore_depth>=eh_context_count)void_fatal("invalid EH context depth");
 g_ebx=eh_contexts[restore_depth].bx;g_esi=eh_contexts[restore_depth].si;g_edi=eh_contexts[restore_depth].di;eh_context_count=restore_depth;
 (void)old_bp;eh_frame_count=(unsigned)frame_index+1;recomp_ehframe_t *frame=eh_frames[frame_index].frame;
 h4_need(guest_bp-16,4,0);g_esp=MEM32(guest_bp-16);g_ebp=guest_bp;g_eax=continuation;frame->resume=continuation;
 void_log("EH continuing at %08x",continuation);_longjmp(frame->jb,1);
}

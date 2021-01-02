/*
 * (c) UQLX - see COPYRIGHT
 */


/* define memory access fns */
#include "QL68000.h"
#include "memaccess.h"
#include "general.h"
#include "qx_proto.h"
#include "SDL2screen.h"

rw8 ReadByte(aw32 addr)
{
    addr&=ADDR_MASK;

    if (addr > RTOP)
        return 0;

    if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        return ReadHWByte(addr);
    }

    return *((w8*)theROM+addr);
}

rw16 ReadWord(aw32 addr)
{
    addr &=ADDR_MASK;

    if (addr > RTOP)
        return 0;

    if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        return ((w16)ReadHWWord(addr));
    }

    return (w16)RW((w16*)((Ptr)theROM+addr)); /* make sure it is signed */
}

rw32 ReadLong(aw32 addr)
{
    addr &= ADDR_MASK;

    if (addr > RTOP)
        return 0;

    if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        return ((w32)ReadWord(addr)<<16)|(uw16)ReadWord(addr+2);
    }

    return  (w32)RL((Ptr)theROM+addr); /* make sure is is signed */
}

#define ValidateDispByte(_xx)
#ifdef DEBUG
long watchaddr=0;
#endif

void WriteByte(aw32 addr,aw8 d)
{
    addr &= ADDR_MASK;

    if (addr > RTOP)
        return;

    if ((addr >= qlscreen.qm_lo) && (addr <= qlscreen.qm_hi)) {
	    *((w8*)theROM+addr)=d;
        QLSDLUpdateScreenByte(addr-qlscreen.qm_lo, d);
    } else if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        WriteHWByte(addr, d);
    } else if (addr > QL_ROM_SIZE) {
        *((w8*)theROM+addr)=d;
    }
}

void WriteWord(aw32 addr,aw16 d)
{
    addr &= ADDR_MASK;

    if (addr > RTOP)
        return;

    if ((addr >= qlscreen.qm_lo) && (addr <= qlscreen.qm_hi)) {
        WW((Ptr)theROM + addr, d);
        QLSDLUpdateScreenWord(addr-qlscreen.qm_lo, d);
    } else if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        WriteHWWord(addr, d);
    } else if (addr >= QL_ROM_SIZE) {
        WW((Ptr)theROM + addr, d);
    }
}

void WriteLong(aw32 addr,aw32 d)
{
    addr &= ADDR_MASK;

    if (addr > RTOP)
        return;

    if ((addr >= qlscreen.qm_lo) && (addr <= qlscreen.qm_hi)) {
        WL((Ptr)theROM + addr, d);
        QLSDLUpdateScreenLong(addr-qlscreen.qm_lo, d);
    } else if ((addr >= QL_INTERNAL_IO_BASE) &&
                (addr < (QL_INTERNAL_IO_BASE + QL_INTERNAL_IO_SIZE))) {
        WriteHWWord(addr, d >> 16);
        WriteHWWord(addr + 2, d);
    } else if (addr >= QL_ROM_SIZE) {
        WL((Ptr)theROM + addr, d);
    }
}

/*############################################################*/
#ifndef QM_BIG_ENDIAN
int isreg=0;
#endif

rw8 ModifyAtEA_b(ashort mode,ashort r)
{
  shindex displ;
  w32     addr;
  switch(mode) {
  case 0:
#if 1
    mea_acc=0;
#else
#if !defined(VM_SCR)
    isDisplay=false;
#endif
    isHW=false;
#endif
    dest=(Ptr)(&reg[r])+RBO;
    return *((w8*)dest);
  case 2:addr=aReg[r];
    break;
  case 3:addr=aReg[r]++;
    if(r==7) (*m68k_sp)++;
    break;
  case 4:
    if(r==7) (*m68k_sp)--;
    addr=--aReg[r];
    break;
  case 5:addr=aReg[r]+(w16)RW(pc++);
    break;
  case 6:
    displ=(w16)RW(pc++);
    if((displ&2048)!=0) addr=reg[(displ>>12) & 15]+
			  aReg[r]+(w32)((w8)displ);
    else addr=(w32) ((w16)(reg[(displ>>12) & 15]))+
	   aReg[r]+(w32)((w8)displ);
    break;
  case 7:
    switch(r) {
    case 0:addr=(w16)RW(pc++);
      break;
    case 1:addr=RL((w32*)pc);
      pc+=2;
      break;
    default:
      exception=4;
      extraFlag=true;
      nInst2=nInst;
      nInst=0;

      mea_acc=0;

      dest=(Ptr)(&dummy);
      return 0;
    }
    break;
  default:
    exception=4;
    extraFlag=true;
    nInst2=nInst;
    nInst=0;

    mea_acc=0;

    dest=(Ptr)(&dummy);
    return 0;
  }
#if 1
  addr&=ADDR_MASK;
#endif
  switch(RamMap[addr>>RM_SHIFT]) {
  case 1:

    mea_acc=0;

    dest=(Ptr)(&dummy);
    return *((w8*)theROM+addr);
  case 3:

    mea_acc=0;

    dest=(Ptr)theROM+addr;
    return *((w8*)dest);
  case 7:

    mea_acc=0;

    dest=(Ptr)theROM+addr;
    return *((w8*)dest);
  case 23:

    mea_acc=MEA_DISP;

    dest=(Ptr)theROM+addr;
    return *((w8*)dest);
  case 8:

    mea_acc=MEA_HW;

    lastAddr=addr;
    dest=(Ptr)(&dummy);
    return ReadByte(addr);
  }

  mea_acc=0;

  dest=(Ptr)(&dummy);
  return 0;
}

rw16 ModifyAtEA_w(ashort mode,ashort r)
{
  /*w16*/ shindex displ;
  w32     addr=0;

#ifndef QM_BIG_ENDIAN
  isreg=0;
#endif

  switch(mode) {
  case 0:
// mea_acc is not set *if* little endian?????
#ifdef QM_BIG_ENDIAN
#if 1
    mea_acc=0;
#else
#if !defined(VM_SCR)
    isDisplay=false;
#endif
    isHW=false;
#endif
#else
    isreg=1;
#endif
    dest=(Ptr)(&reg[r])+RWO;
    return *((w16*)dest);
  case 1:
#ifdef QM_BIG_ENDIAN
#if 1
    mea_acc=0;
#else
#if !defined(VM_SCR)
    isDisplay=false;
#endif
    isHW=false;
#endif
#else
    isreg=1;
#endif
    dest=(Ptr)(&aReg[r])+RWO;
    return *((w16*)dest);
  case 2:addr=aReg[r];
    break;
  case 3:addr=aReg[r];
    aReg[r]+=2;
    break;
  case 4:addr=(aReg[r]-=2);
    break;
  case 5:addr=aReg[r]+(w16)RW(pc++);
    break;
  case 6:
    displ=(w16)RW(pc++);
    if((displ&2048)!=0) addr=reg[(displ>>12) & 15]+
			  aReg[r]+(w32)((w8)displ);
    else addr=(w32)((w16)(reg[(displ>>12) & 15]))+
	   aReg[r]+(w32)((w8)displ);
    break;
  case 7:
    switch(r) {
    case 0:addr=(w16)RW(pc++);
      break;
    case 1:addr=RL((w32*)pc);
      pc+=2;
      break;
    default:
      exception=4;
      extraFlag=true;
      nInst2=nInst;
      nInst=0;
#if 1
      mea_acc=0;
#else
      isHW=false;
#if !defined(VM_SCR)
      isDisplay=false;
#endif
#endif
      dest=(Ptr)(&dummy);
      return 0;
    }
    break;
  }
  addr &=ADDR_MASK;

  switch(RamMap[addr>>RM_SHIFT]) {
  case 1:   /* ROM */
#if 1
    mea_acc=0;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    dest=(Ptr)(&dummy);
    return (w16)RW((w16*)((Ptr)theROM+addr));
  case 3:
#if 1
    mea_acc=0;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    dest=(Ptr)theROM+addr;
    return (w16)RW((w16*)dest);

  case 7:   /* screen access */
  case 23:
#if 1
    mea_acc=MEA_DISP;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=true;
#endif
#endif
    dest=(Ptr)theROM+addr;
    return (w16)RW((w16*)dest);

#if 0  /* delete that ...*/
  case 23:
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=true;
#endif
    dest=(Ptr)theROM+addr;
    return (w16)RW(dest);
#endif

  case 8:
#if 1
    mea_acc=MEA_HW;
#else
    isHW=true;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    lastAddr=addr;
    dest=(Ptr)(&dummy);
    return ReadWord(addr);
  }
#if 1
  mea_acc=0;
#else
  isHW=false;
#if !defined(VM_SCR)
  isDisplay=false;
#endif
#endif
  dest=(Ptr)(&dummy);
  return 0;
}

rw32 ModifyAtEA_l(ashort mode,ashort r)
{
  /*w16*/ shindex displ;
  w32     addr=0;

#ifndef QM_BIG_ENDIAN
  isreg=0;
#endif

  switch(mode) {
  case 0:
#ifdef QM_BIG_ENDIAN
#if 1
    mea_acc=0;
#else
#if !defined(VM_SCR)
    isDisplay=false;
#endif
    isHW=false;
#endif
#else
    isreg=1;
#endif
    dest=(Ptr)(&reg[r]);
    return *((w32*)dest);
  case 1:
#ifdef QM_BIG_ENDIAN
#if 1
    mea_acc=0;
#else
#if !defined(VM_SCR)
    isDisplay=false;
#endif
    isHW=false;
#endif
#else
    isreg=1;
#endif
    dest=(Ptr)(&aReg[r]);
    return *((w32*)dest);
  case 2:addr=aReg[r];
    break;
  case 3:addr=aReg[r];
    aReg[r]+=4;
    break;
  case 4:addr=(aReg[r]-=4);
    break;
  case 5:addr=aReg[r]+(w16)RW(pc++);
    break;
  case 6:
    displ=(w16)RW(pc++);
    if((displ&2048)!=0) addr=reg[(displ>>12) & 15]+
			  aReg[r]+(w32)((w8)displ);
    else addr=(w32)((w16)(reg[(displ>>12) & 15]))+
	   aReg[r]+(w32)((w8)displ);
    break;
  case 7:
    switch(r) {
    case 0:addr=(w16)RW(pc++);
      break;
    case 1:addr=RL((w32*)pc);
      pc+=2;
      break;
    default:
      exception=4;
      extraFlag=true;
      nInst2=nInst;
      nInst=0;
#if 1
      mea_acc=0;
#else
      isHW=false;
#if !defined(VM_SCR)
      isDisplay=false;
#endif
#endif
      dest=(Ptr)(&dummy);
      return 0;
    }
    break;
  }
  addr &= ADDR_MASK;

  switch(RamMap[addr>>RM_SHIFT]) {
  case 1:
#if 1
    mea_acc=0;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    dest=(Ptr)(&dummy);
    return (w32)RL((w32*)((Ptr)theROM+addr));
  case 3:
#if 1
    mea_acc=0;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    dest=(Ptr)theROM+addr;
    return (w32)RL((w32*)dest);
  case 7:
  case 23:
#if 1
    mea_acc=MEA_DISP;
#else
    isHW=false;
#if !defined(VM_SCR)
    isDisplay=true;
#endif
#endif
    dest=(Ptr)theROM+addr;
    return (w32)RL((w32*)dest);

#if 0  /* delete that */
  case 23:isHW=false;
#if !defined(VM_SCR)
    isDisplay=true;
#endif
    dest=(Ptr)theROM+addr;
    /*tmp=ReadDispLong(addr);*/
    return (w32)RL(dest);
#endif

  case 8:
#if 1
    mea_acc=MEA_HW;
#else
    isHW=true;
#if !defined(VM_SCR)
    isDisplay=false;
#endif
#endif
    lastAddr=addr;
    dest=(Ptr)(&dummy);
    return ReadLong(addr);
  }
#if 1
  mea_acc=0;
#else
  isHW=false;
#if !defined(VM_SCR)
  isDisplay=false;
#endif
#endif
  dest=(Ptr)(&dummy);
  return 0;
}


void RewriteEA_b(aw8 d)
{
#if 1
  *((w8*)dest)=d;
  if (!mea_acc) return;
  else
#ifdef EVM_SCR
    if (mea_acc==MEA_DISP) vmMarkScreen((char*)dest-(char*)theROM);
    else
#endif
      WriteByte(lastAddr,d);
#else
  if(isHW) WriteByte(lastAddr,d);
  else
    {
#if defined(EVM_SCR)
      if(isDisplay)
	vmMarkScreen((char*)dest-(char*)theROM);
#endif
      *((w8*)dest)=d;
    }
#endif
}

void RewriteEA_w(aw16 d)
{
#ifndef QM_BIG_ENDIAN
      if (isreg)
	*((w16*)dest)=d;
      else
	{
	  WW((Ptr)dest,d);
#else
	  WW((Ptr)dest,d);
#endif /* QM_BIG_ENDIAN */
	  if (!mea_acc) return;
#if defined(EVM_SCR)
	  if(mea_acc==MEA_DISP)
	    vmMarkScreen((char*)dest-(char*)theROM);
	  else
#endif /* EVM_SCR */
	    WriteWord(lastAddr,d);
#ifndef QM_BIG_ENDIAN
	}
#endif
}

#if 1
void rww_acc(w16 d)
{
#if defined(EVM_SCR)
	  if(mea_acc==MEA_DISP)
	    vmMarkScreen((char*)dest-(char*)theROM);
	  else
#endif /* EVM_SCR */
	    WriteWord(lastAddr,d);
}
#ifdef QM_BIG_ENDIAN
#define RewriteEA_w(_d_)       do{    \
	  WW((Ptr)dest,_d_);          \
          if (mea_acc) rww_acc(_d_); } while(0)
#else
#define RewriteEA_w(_d_)       do{    \
          if (isreg) *((w16*)dest)=_d_;       \
          else {                              \
            	  WW((Ptr)dest,_d_);          \
                  if (mea_acc) rww_acc(_d_); }} while(0)
#endif
#endif


void RewriteEA_l(aw32 d)
{
#ifndef QM_BIG_ENDIAN
      if (isreg) *((w32*)dest)=d;
      else
	{
	  WL((Ptr)dest,d);
#else
	  WL((Ptr)dest,d);
#endif /* QM_BIG_ENDIAN */
	  if (!mea_acc) return;
#if defined(EVM_SCR)
	  if(mea_acc==MEA_DISP)
	    vmMarkScreen((char*)dest-(char*)theROM);
	  else
#endif /* EVM_SCR */
	    WriteLong(lastAddr,d);
#ifndef QM_BIG_ENDIAN
	}
#endif
}

#if 1
void rwl_acc(w32 d)
{
#if defined(EVM_SCR)
	  if(mea_acc==MEA_DISP)
	    vmMarkScreen((char*)dest-(char*)theROM);
	  else
#endif /* EVM_SCR */
	    WriteLong(lastAddr,d);
}
#ifdef QM_BIG_ENDIAN
#define RewriteEA_l(_d_)       do{    \
	  WL((Ptr)dest,_d_);          \
          if (mea_acc) rwl_acc(_d_); } while(0)
#else
#define RewriteEA_l(_d_)       do{    \
          if (isreg) *((w32*)dest)=_d_;       \
          else {                              \
            	  WL((Ptr)dest,_d_);          \
                  if (mea_acc) rwl_acc(_d_); }} while(0)
#endif
#endif

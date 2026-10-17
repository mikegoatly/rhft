import sys
from capstone import *
path=sys.argv[1]; base=int(sys.argv[2],16)
d=open(path,'rb').read()
md=Cs(CS_ARCH_ARM, CS_MODE_ARM); md.skipdata=True
SWIS={0x00:"OS_WriteC",0x01:"OS_WriteS",0x02:"OS_Write0",0x03:"OS_NewLine",0x04:"OS_ReadC",0x05:"OS_CLI",0x06:"OS_Byte",0x07:"OS_Word",0x08:"OS_File",0x09:"OS_Args",0x0a:"OS_BGet",0x0b:"OS_BPut",0x0c:"OS_GBPB",0x0d:"OS_Find",0x0e:"OS_ReadLine",0x0f:"OS_Control",0x10:"OS_GetEnv",0x11:"OS_Exit",0x12:"OS_SetEnv",0x13:"OS_IntOn",0x14:"OS_IntOff",0x15:"OS_CallBack",0x16:"OS_EnterOS",0x17:"OS_BreakPt",0x18:"OS_BreakCtrl",0x19:"OS_UnusedSWI",0x1a:"OS_UpdateMEMC",0x1b:"OS_SetCallBack",0x1c:"OS_Mouse",0x1d:"OS_Heap",0x1e:"OS_Module",0x1f:"OS_Claim",0x20:"OS_Release",0x21:"OS_ReadUnsigned",0x22:"OS_GenerateEvent",0x23:"OS_ReadVarVal",0x24:"OS_SetVarVal",0x25:"OS_GSInit",0x26:"OS_GSRead",0x27:"OS_GSTrans",0x28:"OS_BinaryToDecimal",0x29:"OS_FSControl",0x2a:"OS_ChangeDynamicArea",0x2b:"OS_GenerateError",0x2c:"OS_ReadEscapeState",0x2d:"OS_EvaluateExpression",0x2e:"OS_SpriteOp",0x2f:"OS_ReadPalette",0x30:"OS_ServiceCall",0x31:"OS_ReadVduVariables",0x32:"OS_ReadPoint",0x33:"OS_UpCall",0x34:"OS_CallAVector",0x35:"OS_ReadModeVariable",0x36:"OS_RemoveCursors",0x37:"OS_RestoreCursors",0x38:"OS_SWINumberToString",0x39:"OS_SWINumberFromString",0x3a:"OS_ValidateAddress",0x3b:"OS_CallAfter",0x3c:"OS_CallEvery",0x3d:"OS_RemoveTickerEvent",0x3e:"OS_InstallKeyHandler",0x3f:"OS_CheckModeValid",0x40:"OS_ChangeEnvironment",0x41:"OS_ClaimScreenMemory",0x42:"OS_ReadMonotonicTime",0x43:"OS_SubstituteArgs",0x44:"OS_PrettyPrint",0x45:"OS_Plot",0x46:"OS_WriteN",0x47:"OS_AddToVector",0x48:"OS_WriteEnv",0x49:"OS_ReadArgs",0x4a:"OS_ReadRAMFsLimits",0x4b:"OS_ClaimProcessorVector",0x4c:"OS_Reset",0x4d:"OS_MMUControl",0x100:"OS_WriteI"}
SWIS.update({0x40240+i:n for i,n in enumerate(["Sound_Configure","Sound_Enable","Sound_Stereo","Sound_Speaker"])})
SWIS.update({0x40140+i:n for i,n in enumerate(["Sound_Configure","Sound_Enable","Sound_Stereo","Sound_Speaker","Sound_Mode","Sound_LinearHandler","Sound_SampleRate"])})
SWIS.update({0x40180+i:n for i,n in enumerate(["Sound_Volume","Sound_SoundLog","Sound_LogScale","Sound_InstallVoice","Sound_RemoveVoice","Sound_AttachVoice","Sound_ControlPacked","Sound_Tuning","Sound_Pitch","Sound_Control","Sound_AttachNamedVoice","Sound_ReadControlBlock","Sound_WriteControlBlock"])})
SWIS.update({0x401c0+i:n for i,n in enumerate(["Sound_QInit","Sound_QSchedule","Sound_QRemove","Sound_QFree","Sound_QSDispatch","Sound_QTempo","Sound_QBeat","Sound_QInterface"])})
SWIS.update({0x47d00+i:"FastSpr_"+n for i,n in enumerate("Plot ClearWindow SetClipWindow Load GetAddress SetBackdrop ScreenBank SpritesAreAt ReadSize GetPointer SetFlash".split())})
SWIS.update({0x4a140+i:"Amnesia_"+n for i,n in enumerate("Init ClaimBlock ReleaseBlock ReadBlock ResizeBlock FindBlock TableInit TableInfo ObjectAdd ObjectRemove SelectTable ProcessTable PlotTable CollisionCheck TableSave TableLoad ObjectFind ObjectList SetGameWindow SetPlotWindow SetKillWindow TableClear ReadCollisions ObjectInfo CollideTables ObjectKill FindFree".split())})
def swiname(n):
    x=n&~0x20000
    if x in SWIS: return ("X" if n&0x20000 else "")+SWIS[x]
    return "SWI_%X"%n
import struct
for i in md.disasm(d, base):
    extra=""
    if i.mnemonic=="svc":
        n=int(i.op_str.lstrip('#'),0); extra="  ; "+swiname(n)
    elif i.mnemonic.startswith("ldr") and "[pc" in i.op_str:
        try:
            off=int(i.op_str.split('#')[1].rstrip(']'),0)
            addr=i.address+8+off
            v=struct.unpack_from('<I',d,addr-base)[0]; extra="  ; =0x%X (%d)"%(v,v)
        except Exception as e: pass
    elif i.mnemonic==".byte":
        off=i.address-base; w=struct.unpack_from('<I',d,off)[0] if off+4<=len(d) else 0
        s=d[off:off+4]
        extra="  ; %08X %s"%(w, ''.join(chr(c) if 32<=c<127 else '.' for c in s))
    print("%06X  %s %s%s"%(i.address,i.mnemonic,i.op_str,extra))

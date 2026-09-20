#include "OTFFrameLowering.h"
#include "TargetInfo/OTFTargetInfo.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/CodeGenCWrappers.h"

#define GET_SUBTARGETINFO_HEADER
#define GET_SUBTARGETINFO_CTOR
#include "OTFGenSubtargetInfo.inc"

#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_HEADER
#define GET_INSTRINFO_CTOR_DTOR
#include "OTFGenInstrInfo.inc"

#define GET_REGINFO_ENUM
#define GET_REGINFO_HEADER
#define GET_REGINFO_TARGET_DESC
#include "OTFGenRegisterInfo.inc"

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "OTFGenCallingConv.inc"

class OTFMachineFunctionInfo : public MachineFunctionInfo {
public:
  size_t num_args;
};

class OTFRegisterInfo : public OTFGenRegisterInfo {
  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override {
    reportFatalInternalError("eliminateFrameIndex not implemented yet");
  }
  const MCPhysReg *
  getCalleeSavedRegs(const MachineFunction *MF) const override {
    static MCPhysReg regs[] = {NULL};
    return regs;
  }
  BitVector getReservedRegs(const MachineFunction &MF) const override {
    return BitVector(getNumRegs());
  }
  Register getFrameRegister(const MachineFunction &MF) const override {
    return OTF::NoRegister;
  }

public:
  OTFRegisterInfo() : OTFGenRegisterInfo(OTF::NoRegister) {}
};

class OTFInstrInfo : public OTFGenInstrInfo {
  const OTFRegisterInfo RI;

public:
  OTFInstrInfo(const TargetSubtargetInfo &STI) : OTFGenInstrInfo(STI, RI) {}
};

class OTFSelectionDAG : public SelectionDAGTargetInfo {};

class OTFTargetLowering : public TargetLowering {
  SDValue
  LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                       const SmallVectorImpl<ISD::InputArg> &Ins,
                       const SDLoc &DL, SelectionDAG &DAG,
                       SmallVectorImpl<SDValue> &InVals) const override {
    MachineFunction &MF = DAG.getMachineFunction();
    OTFMachineFunctionInfo *MFI = MF.getInfo<OTFMachineFunctionInfo>();
    SmallVector<CCValAssign, 16> ArgLocs;
    CCState CCInfo(CallConv, isVarArg, MF, ArgLocs, *DAG.getContext());
    CCInfo.AnalyzeFormalArguments(Ins, CC_OTF);

    MFI->num_args = ArgLocs.size();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();
    for (CCValAssign &VA : ArgLocs) {
      if (VA.isRegLoc()) {
        Register VR = RegInfo.createVirtualRegister(&OTF::GPR8RegClass);
        MCPhysReg PR = VA.getLocReg();
        RegInfo.addLiveIn(PR, VR);
        InVals.push_back(DAG.getCopyFromReg(Chain, DL, VR, MVT::i8));
      } else {
        reportFatalInternalError("Stack arguments not implemented yet");
      }
    }
    return Chain;
  }
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override {
    MachineFunction &MF = DAG.getMachineFunction();
    SmallVector<CCValAssign, 16> RetLocs;
    CCState CCInfo(CallConv, isVarArg, MF, RetLocs, *DAG.getContext());
    CCInfo.AnalyzeReturn(Outs, CC_OTF);
    SmallVector<SDValue> Ops;
    for (unsigned i = 0; i < RetLocs.size(); i++) {
      CCValAssign VA = RetLocs[i];
      if (VA.isRegLoc()) {
        Register reg = VA.getLocReg();
        Chain = DAG.getCopyToReg(Chain, DL, reg, OutVals[i]);
        Ops.push_back(DAG.getRegister(reg, MVT::i8));
      } else {
        reportFatalInternalError("Stack returns not implemented yet");
      }
    }
    Ops.push_back(Chain);
    return SDValue(DAG.getMachineNode(OTF::RET, DL, MVT::Other, Ops), 0);
  }

public:
  OTFTargetLowering(TargetMachine &TM, TargetSubtargetInfo &STI)
      : TargetLowering(TM, STI) {
    addRegisterClass(MVT::i8, &OTF::GPR8RegClass);
    computeRegisterProperties(STI.getRegisterInfo());
  }
};

class OTFSubtarget : public OTFGenSubtargetInfo {
  OTFFrameLowering FrameLowering;
  OTFInstrInfo InstrInfo;
  OTFSelectionDAG TSInfo;
  OTFTargetLowering TLInfo;
  const OTFFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const TargetRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const OTFInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const OTFSelectionDAG *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  const OTFTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }

public:
  OTFSubtarget(const Triple &TT, StringRef CPU, StringRef FS, TargetMachine &TM)
      : OTFGenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this),
        TLInfo(TM, *this) {}
};

class OTFISel : public SelectionDAGISel {
#include "OTFGenDAGISel.inc"
  void Select(SDNode *Node) override {
    if (Node->isMachineOpcode()) {
      Node->setNodeId(-1);
      return;
    }
    SelectCode(Node);
  }

public:
  OTFISel(TargetMachine &TM, CodeGenOptLevel OL) : SelectionDAGISel(TM, OL) {}
};

class OTFISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  OTFISelLegacy(TargetMachine &TM, CodeGenOptLevel OL)
      : SelectionDAGISelLegacy(ID, std::make_unique<OTFISel>(TM, OL)) {}
};
char OTFISelLegacy::ID = 0;
namespace llvm {
void initializeOTFISelLegacyPass(PassRegistry &Registry);
} // namespace llvm
INITIALIZE_PASS(OTFISelLegacy, "OTF-isel", "OTF instruction select", false,
                false);

class OTFRegAlloc : public MachineFunctionPass {
  bool runOnMachineFunction(MachineFunction &MF) {
    MCInstrDesc CALL = MF.getSubtarget().getInstrInfo()->get(OTF::CALL);
    MachineBasicBlock &front = MF.front();
    for (MachineInstr &MI : make_early_inc_range(front)) {
      switch (MI.getOpcode()) {
      case OTF::RET:
        break;
      case OTF::MOV: {
        std::string lookup_name =
            (Twine("__push_") + Twine(MI.getOperand(1).getImm())).str();
        const char *stored_name =
            MF.getContext().allocateString(lookup_name).data();
        BuildMI(front, MI, MI.getDebugLoc(), CALL)
            .addImm(0)
            .addExternalSymbol(stored_name);
        break;
      }
      default:
        llvm_unreachable("An instruction was not processed correctly");
      }
      MI.eraseFromParent();
    }

    auto iter_pos = front.begin();
    DebugLoc DL = iter_pos->getDebugLoc();
    size_t num_args = MF.getInfo<OTFMachineFunctionInfo>()->num_args;
    bool ret = num_args > 0;
    for (size_t i = 0; i < num_args; i++) {
      BuildMI(front, iter_pos, DL, CALL).addImm(1).addExternalSymbol("__set_0");
      BuildMI(front, iter_pos, DL, CALL).addImm(0).addExternalSymbol("__del_1");
    }
    return ret;
  }

public:
  static char ID;
  OTFRegAlloc() : MachineFunctionPass(ID) {}
};
char OTFRegAlloc::ID = 0;
namespace llvm {
void initializeOTFRegAllocPass(PassRegistry &Registry);
} // namespace llvm
INITIALIZE_PASS(OTFRegAlloc, "OTF-reg-alloc", "OTF register allocation", false,
                false);

class OTFPassConfig : public TargetPassConfig {
  bool addInstSelector() override {
    addPass(new OTFISelLegacy(getTM<TargetMachine>(), getOptLevel()));
    return false;
  }
  void addPreEmitPass2() { addPass(new OTFRegAlloc()); }

public:
  OTFPassConfig(TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}
};

class OTFTargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFileELF> TLOF;
  OTFSubtarget Subtarget;

  OTFMachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const {
    return new OTFMachineFunctionInfo();
  }
  OTFPassConfig *createPassConfig(PassManagerBase &PM) override {
    return new OTFPassConfig(*this, PM);
  }
  TargetLoweringObjectFileELF *getObjFileLowering() const override {
    return TLOF.get();
  }
  const OTFSubtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }

public:
  OTFTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                   StringRef FS, const TargetOptions &Options,
                   std::optional<Reloc::Model> RM,
                   std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                   bool JIT)
      : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS,
                                 Options, Reloc::Static, CodeModel::Tiny, OL),
        TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
        Subtarget(TT, CPU, FS, *this) {
    initAsmInfo();
  }
};

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeOTFTarget() {
  RegisterTargetMachine<OTFTargetMachine> X(getTheOTFTarget());
  PassRegistry *PR = PassRegistry::getPassRegistry();
  initializeOTFISelLegacyPass(*PR);
  initializeOTFRegAllocPass(*PR);
}

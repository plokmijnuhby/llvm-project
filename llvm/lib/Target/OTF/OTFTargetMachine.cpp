#include "OTFFrameLowering.h"
#include "OTFRegAlloc.h"
#include "OTFTargetLowering.h"
#include "OTFRegisterInfo.h"
#include "TargetInfo/OTFTargetInfo.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
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

#define GET_REGINFO_TARGET_DESC
#include "OTFGenRegisterInfo.inc"

#define GET_SDNODE_ENUM
#include "OTFGenSDNodeInfo.inc"

using namespace llvm;

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
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify = false) const override {
    if (!MBB.empty() && MBB.instr_back().getOpcode() == OTF::BR) {
      TBB = MBB.instr_back().getOperand(0).getMBB();
      return false;
    }
    return true;
  }

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override {
    MBB.instr_back().removeFromParent();
    return 1;
  }

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override {
    BuildMI(MBB, MBB.end(), DL,
            MBB.getParent()->getSubtarget().getInstrInfo()->get(OTF::BR))
        .addMBB(TBB);
    return 1;
  }

public:
  OTFInstrInfo(const TargetSubtargetInfo &STI) : OTFGenInstrInfo(STI, RI) {}
};

class OTFSelectionDAG : public SelectionDAGTargetInfo {};

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

class OTFPassConfig : public TargetPassConfig {
  bool addInstSelector() override {
    addPass(new OTFISelLegacy(getTM<TargetMachine>(), getOptLevel()));
    return false;
  }
  void addPreEmitPass2() override { addPass(new OTFRegAlloc()); }

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

#ifndef LLVM_LIB_TARGET_OTF_OTFTARGETLOWERING_H
#define LLVM_LIB_TARGET_OTF_OTFTARGETLOWERING_H

#include "OTFRegisterInfo.h"
#include "llvm/CodeGen/TargetLowering.h"

using namespace llvm;

class OTFTargetLowering : public TargetLowering {
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;
  EVT getSetCCResultType(const DataLayout &, LLVMContext &,
                         EVT) const override {
    return MVT::i8;
  }
  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;

public:
  OTFTargetLowering(TargetMachine &TM, TargetSubtargetInfo &STI)
      : TargetLowering(TM, STI) {
    addRegisterClass(MVT::i8, &OTF::GPR8RegClass);
    computeRegisterProperties(STI.getRegisterInfo());
  }
};

#endif
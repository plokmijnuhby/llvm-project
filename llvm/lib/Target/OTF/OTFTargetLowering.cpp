#include "OTFTargetLowering.h"
#include "OTFMachineFunctionInfo.h"
#include "OTFRegisterInfo.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

#define GET_SDNODE_ENUM
#include "OTFGenSDNodeInfo.inc"

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "OTFGenCallingConv.inc"

SDValue OTFTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
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
SDValue
OTFTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               const SmallVectorImpl<SDValue> &OutVals,
                               const SDLoc &DL, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> RetLocs;
  CCState CCInfo(CallConv, isVarArg, MF, RetLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, CC_OTF);
  SmallVector<SDValue> Ops;
  SDValue Glue;
  for (unsigned i = 0; i < RetLocs.size(); i++) {
    CCValAssign VA = RetLocs[i];
    if (VA.isRegLoc()) {
      Register reg = VA.getLocReg();
      Chain = DAG.getCopyToReg(Chain, DL, reg, OutVals[i], Glue);
      Glue = Chain.getValue(1);
      Ops.push_back(DAG.getRegister(reg, MVT::i8));
    } else {
      reportFatalInternalError("Stack returns not implemented yet");
    }
  }
  Ops.push_back(Chain);
  if (Glue.getNode()) {
    Ops.push_back(Glue);
  }
  return SDValue(DAG.getMachineNode(OTF::RET, DL, MVT::Other, Ops), 0);
}
SDValue OTFTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                     SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc DL = CLI.DL;
  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(CLI.Outs, CC_OTF);
  assert(ArgLocs.size() == 0);
  SDValue hi, lo;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(CLI.Callee)) {
    hi = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i8, 0, 0);
    lo = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i8, 0, 1);
  } else {
    reportFatalInternalError("Non global address call not implemented yet");
  }
  SDValue Chain = DAG.getNode(OTFISD::CALL, DL, MVT::Other, CLI.Chain, hi, lo);

  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CLI.CallConv, CLI.IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(CLI.Ins, CC_OTF);
  assert(RVLocs.size() == 0);
  return Chain;
}
MachineBasicBlock *
OTFTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                               MachineBasicBlock *MBB) const {
  MachineFunction *MF = MBB->getParent();
  switch (MI.getOpcode()) {
  case OTF::CALL_SPLIT:
    BuildMI(*MBB, MI, MI.getDebugLoc(),
            MF->getSubtarget().getInstrInfo()->get(OTF::CALL))
        .addImm(0)
        .addGlobalAddress(MI.getOperand(0).getGlobal());
    MI.eraseFromParent();
    return MBB;
  case OTF::BR_EQ: {
    uint8_t value = MI.getOperand(1).getImm();
    MachineBasicBlock *true_path = MI.getOperand(2).getMBB();
    MachineBasicBlock *false_path =
        MF->CreateMachineBasicBlock(MBB->getBasicBlock());
    MF->insert(++MachineFunction::iterator(MBB), false_path);
    false_path->splice(false_path->begin(), MBB,
                       MachineBasicBlock::iterator(&MI), MBB->end());
    for (MachineBasicBlock *succ : MBB->successors()) {
      MBB->removeSuccessor(succ);
    }
    MBB->addSuccessor(true_path);
    MBB->addSuccessor(false_path);
    auto *MFI = MF->getInfo<OTFMachineFunctionInfo>();
    MachineJumpTableInfo *JTI =
        MF->getOrCreateJumpTableInfo(MachineJumpTableInfo::EK_BlockAddress);
    unsigned table_number = JTI->createJumpTableIndex({true_path, false_path});
    BuildMI(*MBB, MBB->end(), MI.getDebugLoc(),
            MF->getSubtarget().getInstrInfo()->get(OTF::BR_COND_PSEUDO))
        .add(MI.getOperand(0))
        .addImm(MFI->lookups.size())
        .addJumpTableIndex(table_number);
    MI.eraseFromParent();
    Lookup lookup = {table_number};
    if (value != 0) {
      lookup.ranges.push_back({0, (uint8_t)(value - 1), 1});
    }
    lookup.ranges.push_back({value, value, 0});
    if (value != 255) {
      lookup.ranges.push_back({(uint8_t)(value + 1), 255, 1});
    }
    MFI->lookups.push_back(lookup);
    return false_path;
  }
  case OTF::BR: {
    MBB->addSuccessor(MI.getOperand(0).getMBB());
    return MBB;
  }
  default:
    llvm_unreachable("No custom inserter for this node");
  }
}

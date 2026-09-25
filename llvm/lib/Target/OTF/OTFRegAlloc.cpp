#include "OTFRegAlloc.h"
#include "OTFMachineFunctionInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCContext.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

using namespace llvm;

void call(MachineBasicBlock &MBB, MachineBasicBlock::iterator &MI, DebugLoc DL,
          size_t index, const char *name) {
  BuildMI(MBB, MI, DL,
          MBB.getParent()->getSubtarget().getInstrInfo()->get(OTF::CALL))
      .addImm(index)
      .addExternalSymbol(name);
}

bool OTFRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  MCContext &context = MF.getContext();
  MachineBasicBlock &front = MF.front();
  SmallVector<Register> in_use;
  for (MachineInstr &MI : make_early_inc_range(reverse(front))) {
    DebugLoc DL = MI.getDebugLoc();
    switch (MI.getOpcode()) {
    case OTF::MOV:
      for (size_t i = 0; i < in_use.size(); i++) {
        if (in_use[i] == MI.getOperand(0).getReg()) {
          std::string lookup_name =
              (Twine("__push_") + Twine(MI.getOperand(1).getImm())).str();
          const char *stored_name = context.allocateString(lookup_name).data();
          call(front, MachineBasicBlock::iterator(MI), DL, i, stored_name);
          in_use.erase(in_use.begin() + i);
          break;
        }
      }
      break;
    case OTF::ADD:
      for (size_t i = 0; i < in_use.size(); i++) {
        if (in_use[i] == MI.getOperand(0).getReg()) {
          std::string lookup_name =
              (Twine("__add_") + Twine(MI.getOperand(2).getImm())).str();
          const char *stored_name = context.allocateString(lookup_name).data();
          call(front, MachineBasicBlock::iterator(MI), DL, i + 1, stored_name);
          call(front, MachineBasicBlock::iterator(MI), DL, i + 1,
               "__unset_magic");
          in_use[i] == MI.getOperand(1).getReg();
          break;
        }
      }
      break;
    case OTF::RET:
      for (MachineOperand &MO : MI.operands())
        in_use.push_back(MO.getReg());
      break;
    default:
      llvm_unreachable("An instruction was not processed correctly");
    }
    MI.eraseFromParent();
  }

  auto iter_pos = front.begin();
  DebugLoc DL;
  if (iter_pos != front.end()) {
    DL = iter_pos->getDebugLoc();
  }
  size_t num_args = MF.getInfo<OTFMachineFunctionInfo>()->num_args;
  bool ret = num_args > 0;
  for (size_t i = 0; i < num_args; i++) {
    bool used = false;
    for (Register reg : in_use) {
      if (i == context.getRegisterInfo()->getEncodingValue(reg)) {
        used = true;
        break;
      }
    }
    if (!used) {
      call(front, iter_pos, DL, 1, "__set_0");
      call(front, iter_pos, DL, 0, "__del_1");
    }
  }
  return ret;
}

INITIALIZE_PASS(OTFRegAlloc, "OTF-reg-alloc", "OTF register allocation", false,
                false);
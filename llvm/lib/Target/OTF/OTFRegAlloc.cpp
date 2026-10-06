#include "OTFRegAlloc.h"
#include "OTFMachineFunctionInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCContext.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

using namespace llvm;

SmallVector<Register> OTFRegAlloc::processBlock(MachineBasicBlock &MBB) {
  MachineFunction &MF = *MBB.getParent();
  MCContext &context = MF.getContext();
  SmallVector<Register> in_use;
  auto call = [&](MachineInstr &I, DebugLoc DL, size_t index,
                  const char *name) {
    BuildMI(MBB, I, DL, MF.getSubtarget().getInstrInfo()->get(OTF::CALL))
        .addImm(index)
        .addExternalSymbol(name);
  };
  for (MachineInstr &MI : make_early_inc_range(reverse(MBB))) {
    DebugLoc DL = MI.getDebugLoc();
    switch (MI.getOpcode()) {
    case OTF::MOV:
      for (size_t i = 0; i < in_use.size(); i++) {
        if (in_use[i] == MI.getOperand(0).getReg()) {
          std::string lookup_name =
              (Twine("__push_") + Twine(MI.getOperand(1).getImm())).str();
          const char *stored_name = context.allocateString(lookup_name).data();
          call(MI, DL, i, stored_name);
          in_use.erase(in_use.begin() + i);
          break;
        }
      }
      MI.eraseFromParent();
      break;
    case OTF::ADD:
      for (size_t i = 0; i < in_use.size(); i++) {
        if (in_use[i] == MI.getOperand(0).getReg()) {
          std::string lookup_name =
              (Twine("__add_") + Twine(MI.getOperand(2).getImm())).str();
          const char *stored_name = context.allocateString(lookup_name).data();
          call(MI, DL, i + 1, stored_name);
          call(MI, DL, i + 1, "__unset_magic");
          in_use[i] == MI.getOperand(1).getReg();
          break;
        }
      }
      MI.eraseFromParent();
      break;
    case OTF::AND:
      for (size_t i = 0; i < in_use.size(); i++) {
        if (in_use[i] == MI.getOperand(0).getReg()) {
          std::string lookup_name =
              (Twine("__and_") + Twine(MI.getOperand(2).getImm())).str();
          const char *stored_name = context.allocateString(lookup_name).data();
          call(MI, DL, i + 1, stored_name);
          in_use[i] == MI.getOperand(1).getReg();
          break;
        }
      }
      MI.eraseFromParent();
      break;
    case OTF::RET:
      for (MachineOperand &MO : MI.operands())
        in_use.push_back(MO.getReg());
      MI.eraseFromParent();
      break;
    case OTF::BR_EQ_FALLTHROUGH: {
      MachineBasicBlock *tail = MBB.splitAt(MI);
      assert(tail->getLiveIns().empty());
      assert(MI.getOperand(2).getMBB()->getLiveIns().empty());
      processBlock(*tail);
      processBlock(*MI.getOperand(2).getMBB());
      in_use.push_back(MI.getOperand(0).getReg());
      in_use.push_back(MI.getOperand(1).getReg());
      BuildMI(MBB, MI, DL, MF.getSubtarget().getInstrInfo()->get(OTF::BR_EQ))
          .addImm(1)
          .addImm(2)
          .add(MI.getOperand(2))
          .addMBB(tail);
      MI.eraseFromParent();
      break;
    }
    case OTF::BR:
      processBlock(*MI.getOperand(0).getMBB());
      for (auto &reg_pair : MI.getOperand(0).getMBB()->getLiveIns())
        in_use.push_back(reg_pair.PhysReg);
      break;
    case OTF::CALL:
      break;
    default:
      llvm_unreachable("An instruction was not processed correctly");
    }
  }
  return in_use;
}

bool OTFRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  MCContext &context = MF.getContext();
  MachineBasicBlock &front = MF.front();
  SmallVector<Register> in_use = processBlock(front);

  auto iter_pos = front.begin();
  DebugLoc DL;
  if (iter_pos != front.end()) {
    DL = iter_pos->getDebugLoc();
  }
  size_t num_args = MF.getInfo<OTFMachineFunctionInfo>()->num_args;
  for (size_t i = 0; i < num_args; i++) {
    bool used = false;
    for (Register reg : in_use) {
      if (i == context.getRegisterInfo()->getEncodingValue(reg)) {
        used = true;
        break;
      }
    }
    if (!used) {
      const MCInstrDesc &call =
          MF.getSubtarget().getInstrInfo()->get(OTF::CALL);
      BuildMI(front, iter_pos, DL, call).addImm(1).addExternalSymbol("__set_0");
      BuildMI(front, iter_pos, DL, call).addImm(0).addExternalSymbol("__del_1");
    }
  }
  return true;
}

INITIALIZE_PASS(OTFRegAlloc, "OTF-reg-alloc", "OTF register allocation", false,
                false);
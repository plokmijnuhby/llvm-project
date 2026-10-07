#include "OTFRegAlloc.h"
#include "OTFMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCContext.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

#define GET_REGINFO_HEADER
#include "OTFGenRegisterInfo.inc"

using namespace llvm;

void OTFRegAlloc::processBlock(MachineBasicBlock &MBB,
                               SmallVector<unsigned> inputs) {
  MachineFunction &MF = *MBB.getParent();
  MCContext &context = MF.getContext();
  const MCInstrDesc &callInstr =
      MF.getSubtarget().getInstrInfo()->get(OTF::CALL);
  SmallVector<unsigned> in_use;
  auto call = [&](MachineInstr &I, DebugLoc DL, size_t index,
                  const char *name) {
    BuildMI(MBB, I, DL, callInstr).addImm(index).addExternalSymbol(name);
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
          in_use[i] = MI.getOperand(1).getReg();
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
          in_use[i] = MI.getOperand(1).getReg();
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
    case OTF::BR_COND_PSEUDO: {
      in_use.push_back(MI.getOperand(0).getReg());
      for (MachineBasicBlock *block :
           MF.getJumpTableInfo()
               ->getJumpTables()[MI.getOperand(2).getIndex()]
               .MBBs) {
        assert(block->getLiveIns().empty());
        processBlock(*block, in_use);
      }
      BuildMI(MBB, MI, DL, MF.getSubtarget().getInstrInfo()->get(OTF::BR_COND))
          .addImm(1)
          .add(MI.getOperand(1));
      MI.eraseFromParent();
      break;
    }
    case OTF::BR:
      processBlock(*MI.getOperand(0).getMBB(), in_use);
      for (auto &reg_pair : MI.getOperand(0).getMBB()->getLiveIns())
        in_use.push_back(reg_pair.PhysReg);
      break;
    case OTF::CALL:
      break;
    default:
      llvm_unreachable("An instruction was not processed correctly");
    }
  }
  auto iter_pos = MBB.begin();
  DebugLoc DL;
  if (iter_pos != MBB.end()) {
    DL = iter_pos->getDebugLoc();
  }
  for (Register input : inputs) {
    bool used = false;
    for (Register reg : in_use) {
      if (reg == input) {
        used = true;
        break;
      }
    }
    if (!used) {
      BuildMI(MBB, iter_pos, DL, callInstr)
          .addImm(1)
          .addExternalSymbol("__set_0");
      BuildMI(MBB, iter_pos, DL, callInstr)
          .addImm(0)
          .addExternalSymbol("__del_1");
    }
  }
}

bool OTFRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  MachineBasicBlock &front = MF.front();
  size_t num_args = MF.getInfo<OTFMachineFunctionInfo>()->num_args;
  auto *inputs = new SmallVector<unsigned>(
      OTF::GPR8RegClass.getRegisters().take_front(num_args));
  processBlock(front, *inputs);
  return true;
}

INITIALIZE_PASS(OTFRegAlloc, "OTF-reg-alloc", "OTF register allocation", false,
                false);
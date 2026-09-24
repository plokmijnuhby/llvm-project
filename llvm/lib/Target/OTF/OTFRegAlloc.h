#ifndef LLVM_LIB_TARGET_OTF_OTFREGALLOC_H
#define LLVM_LIB_TARGET_OTF_OTFREGALLOC_H

#include "OTFMachineFunctionInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCContext.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

using namespace llvm;

class OTFRegAlloc : public MachineFunctionPass {
  bool runOnMachineFunction(MachineFunction &MF) {
    MCContext &context = MF.getContext();
    MCInstrDesc CALL = MF.getSubtarget().getInstrInfo()->get(OTF::CALL);
    MachineBasicBlock &front = MF.front();
    SmallVector<Register> in_use;
    for (MachineInstr &MI : make_early_inc_range(reverse(front))) {
      switch (MI.getOpcode()) {
      case OTF::MOV: {
        for (size_t i = 0; i < in_use.size(); i++) {
          if (in_use[i] == MI.getOperand(0).getReg()) {
            std::string lookup_name =
                (Twine("__push_") + Twine(MI.getOperand(1).getImm())).str();
            const char *stored_name =
                context.allocateString(lookup_name).data();
            BuildMI(front, MI, MI.getDebugLoc(), CALL)
                .addImm(i)
                .addExternalSymbol(stored_name);
            in_use.erase(in_use.begin() + i);
            break;
          }
        }
        break;
      }
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
        BuildMI(front, iter_pos, DL, CALL)
            .addImm(1)
            .addExternalSymbol("__set_0");
        BuildMI(front, iter_pos, DL, CALL)
            .addImm(0)
            .addExternalSymbol("__del_1");
      }
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
#endif

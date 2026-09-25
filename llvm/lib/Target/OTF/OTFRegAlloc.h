#ifndef LLVM_LIB_TARGET_OTF_OTFREGALLOC_H
#define LLVM_LIB_TARGET_OTF_OTFREGALLOC_H

#include "OTFMachineFunctionInfo.h"

class OTFRegAlloc : public MachineFunctionPass {
  bool runOnMachineFunction(MachineFunction &MF) override;

public:
  static char ID;
  OTFRegAlloc() : MachineFunctionPass(ID) {}
};
char OTFRegAlloc::ID = 0;
namespace llvm {
void initializeOTFRegAllocPass(PassRegistry &Registry);
} // namespace llvm

#endif

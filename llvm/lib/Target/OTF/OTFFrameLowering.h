#ifndef LLVM_LIB_TARGET_OTF_OTFFRAMELOWERING_H
#define LLVM_LIB_TARGET_OTF_OTFFRAMELOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {
class OTFFrameLowering : public TargetFrameLowering {
  void emitPrologue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override {}
  void emitEpilogue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override {}
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }

public:
  OTFFrameLowering()
      : TargetFrameLowering(StackDirection::StackGrowsDown, Align(), 0) {}
};
} // namespace llvm

#endif
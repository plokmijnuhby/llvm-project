#include "TargetInfo/OTFTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheOTFTarget() {
  static Target TheOTFTarget;
  return TheOTFTarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeOTFTargetInfo() {
  RegisterTarget<Triple::otf> X(getTheOTFTarget(), "otf", "OTF", "OTF");
}

#ifndef LLVM_LIB_TARGET_OTF_OTFMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_OTF_OTFMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/SelectionDAGISel.h"

using namespace llvm;

class OTFMachineFunctionInfo : public MachineFunctionInfo {
public:
  size_t num_args;
};

#endif
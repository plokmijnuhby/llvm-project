#ifndef LLVM_LIB_TARGET_OTF_OTFMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_OTF_OTFMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"

using namespace llvm;

class Range {
public:
  uint8_t start;
  uint8_t end;
  uint16_t table_index;
};

class Lookup {
public:
  unsigned table_number;
  SmallVector<Range> ranges;
};

class OTFMachineFunctionInfo : public MachineFunctionInfo {
public:
  size_t num_args;
  SmallVector<Lookup> lookups;
};

#endif
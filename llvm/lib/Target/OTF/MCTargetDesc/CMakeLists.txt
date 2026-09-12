add_llvm_component_library(LLVMOTFDesc
  OTFMCTargetDesc.cpp

  LINK_COMPONENTS
  AsmPrinter

  ADD_TO_COMPONENT
  OTF
)
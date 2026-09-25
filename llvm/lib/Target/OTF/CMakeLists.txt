add_llvm_component_group(OTF)

set(LLVM_TARGET_DEFINITIONS OTF.td)

tablegen(LLVM OTFGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM OTFGenCallingConv.inc -gen-callingconv)
tablegen(LLVM OTFGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM OTFGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM OTFGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM OTFGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(OTFCommonTableGen)

add_llvm_target(OTFCodeGen
  OTFAsmPrinter.cpp
  OTFRegAlloc.cpp
  OTFTargetMachine.cpp

  ADD_TO_COMPONENT
  OTF
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

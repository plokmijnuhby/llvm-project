#include "TargetInfo/OTFTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_ENUM
#include "OTFGenInstrInfo.inc"

using namespace llvm;

class OTFMCExpr : public MCTargetExpr {
  unsigned int flags;
  const MCSymbol *sym;

  bool evaluateAsRelocatableImpl(MCValue &,
                                 const MCAssembler *) const override {
    reportFatalInternalError("evaluateAsRelocatableImpl not implemented yet");
  }
  void visitUsedExpr(MCStreamer &Streamer) const override {
    reportFatalInternalError("visitUsedExpr not implemented yet");
  }
  MCFragment *findAssociatedFragment() const override {
    reportFatalInternalError("findAssociatedFragment not implemented yet");
  }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override {
    sym->print(OS, MAI);
    OS << ":" << flags;
  }

public:
  OTFMCExpr(unsigned int flags, MCSymbol *sym)
      : MCTargetExpr(), flags(flags), sym(sym) {}
};

class OTFAsmPrinter : public AsmPrinter {
  StringRef getPassName() const override { return "OTF Assembly Printer"; }
  void emitInstruction(const MachineInstr *MI) override {
    if (MI->getOpcode() == OTF::CALL) {
      auto instruction = MCInstBuilder(OTF::CALL);
      for (const MachineOperand MO : MI->operands()) {
        switch (MO.getType()) {
        case MachineOperand::MO_Immediate:
          instruction.addImm(MO.getImm());
          break;
        case MachineOperand::MO_GlobalAddress:
          instruction.addExpr(
              new OTFMCExpr(MO.getTargetFlags(), getSymbol(MO.getGlobal())));
          break;
        case MachineOperand::MO_ExternalSymbol:
          instruction.addExpr(
              new OTFMCExpr(MO.getTargetFlags(),
                            GetExternalSymbolSymbol(MO.getSymbolName())));
          break;
        default:
          llvm_unreachable("Unknown type");
        }
      }
      OutStreamer.get()->emitInstruction(instruction, getSubtargetInfo());
    }
  }

public:
  static char ID;
  OTFAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}
};
char OTFAsmPrinter::ID = 0;

namespace llvm {
void initializeOTFAsmPrinterPass(PassRegistry &Registry);
} // namespace llvm
INITIALIZE_PASS(OTFAsmPrinter, "otf-asm-printer", "OTF Assembly Printer", false,
                false);

// Force static initialization.
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeOTFAsmPrinter() {
  RegisterAsmPrinter<OTFAsmPrinter> X(getTheOTFTarget());
}

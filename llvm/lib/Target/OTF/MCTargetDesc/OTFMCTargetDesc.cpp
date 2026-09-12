#include "TargetInfo/OTFTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSchedule.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

#define GET_REGINFO_ENUM
#define GET_REGINFO_MC_DESC
#include "OTFGenRegisterInfo.inc"

#define GET_INSTRINFO_MC_DESC
#include "OTFGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_MC_DESC
#include "OTFGenSubtargetInfo.inc"

class OTFInstPrinter : public MCInstPrinter {
  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &O);
  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &O) override {
    printInstruction(MI, Address, O);
    printAnnotation(O, Annot);
  }
  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O) {
    const MCOperand &MO = MI->getOperand(OpNo);
    if (MO.isImm()) {
      O << MO.getImm();
    } else if (MO.isExpr()) {
      MAI.printExpr(O, *MO.getExpr());
    } else {
      llvm_unreachable("Unknown type");
    }
  }
  std::pair<const char *, uint64_t> getMnemonic(const MCInst &MI) const;
  const char *getRegisterName(MCRegister Reg);

public:
  OTFInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                 const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}
};

#include "OTFGenAsmWriter.inc"

class OTFMCAsmInfo : public MCAsmInfo {
public:
  explicit OTFMCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
      : MCAsmInfo(Options) {}
};

static MCInstrInfo *createOTFMCInstrInfo() {
  auto *X = new MCInstrInfo();
  InitOTFMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createOTFMCRegisterInfo(const Triple &Triple) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitOTFMCRegisterInfo(X, OTF::NoRegister);
  return X;
}

static MCSubtargetInfo *createOTFMCSubtargetInfo(const Triple &TT,
                                                 StringRef CPU, StringRef FS) {
  return createOTFMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

class OTFAsmPrinter : public AsmPrinter {
public:
  OTFAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}
};

class OTFObjectWriter : public MCObjectTargetWriter {
  Triple::ObjectFormatType getFormat() const override { return Triple::ELF; }
};

class OTFAsmBackend : public MCAsmBackend {
  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return std::make_unique<OTFObjectWriter>();
  }
  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &Target,
                  uint8_t *Data, uint64_t Value, bool IsResolved) override {
    reportFatalInternalError("applyFixup not implemented yet");
  }
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    llvm_unreachable("Nop is not relevant for OTF");
  }

public:
  OTFAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                const MCRegisterInfo &MRI)
      : MCAsmBackend(llvm::endianness::big) {}
};

static MCInstPrinter *createOTFMCInstPrinter(const Triple &T,
                                             unsigned SyntaxVariant,
                                             const MCAsmInfo &MAI,
                                             const MCInstrInfo &MII,
                                             const MCRegisterInfo &MRI) {
  return new OTFInstPrinter(MAI, MII, MRI);
}

class OTFMCCodeEmitter : public MCCodeEmitter {
  ~OTFMCCodeEmitter() override = default;
  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override {
    reportFatalInternalError("encodeInstruction not implemented yet");
  }
};

class OTFELFStreamer : public MCELFStreamer {
public:
  OTFELFStreamer(MCContext &Context, std::unique_ptr<MCAsmBackend> TAB,
                 std::unique_ptr<MCObjectWriter> OW,
                 std::unique_ptr<MCCodeEmitter> Emitter)
      : MCELFStreamer(Context, std::move(TAB), std::move(OW),
                      std::move(Emitter)) {}
};

MCStreamer *createOTFELFStreamer(const Triple &, MCContext &Context,
                                 std::unique_ptr<MCAsmBackend> &&TAB,
                                 std::unique_ptr<MCObjectWriter> &&OW,
                                 std::unique_ptr<MCCodeEmitter> &&Emitter) {
  return new OTFELFStreamer(Context, std::move(TAB), std::move(OW),
                            std::move(Emitter));
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeOTFTargetMC() {
  Target &t = getTheOTFTarget();
  RegisterMCAsmInfo<OTFMCAsmInfo> A(t);
  TargetRegistry::RegisterMCInstrInfo(t, createOTFMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(t, createOTFMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(t, createOTFMCSubtargetInfo);
  RegisterAsmPrinter<OTFAsmPrinter> B(t);
  RegisterMCAsmBackend<OTFAsmBackend> C(t);
  TargetRegistry::RegisterMCInstPrinter(t, createOTFMCInstPrinter);
  RegisterMCCodeEmitter<OTFMCCodeEmitter> D(t);
  TargetRegistry::RegisterELFStreamer(t, createOTFELFStreamer);
}

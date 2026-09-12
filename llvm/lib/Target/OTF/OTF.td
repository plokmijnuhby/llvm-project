include "llvm/Target/Target.td"

foreach i = 1-63 in {
  def R#i : Register<"R"#i> {
    let HWEncoding = i;
    let Namespace = "OTF";
  }
}
def GPR11 : RegisterClass<"OTF", [i16], 1, (sequence "R%u", 1, 63)>;

def CC_OTF : CallingConv<[
  CCIfType<[i16], CCAssignToReg<!foreach(i, !range(1, 63), !cast<Register>("R"#i))>>,
  CCIfType<[i16], CCAssignToStack<2, 2>>
]>;

def imm16 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]>;

def RetNode : SDNode<"OTFISD::RET", SDTypeProfile<0, 0, []>, [SDNPHasChain]>;
def CallNode : SDNode<"OTFISD::CALL", SDTypeProfile<0, 2, [
    SDTCisVT<0, i16>, SDTCisVT<1, i16>
]>, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def RET : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs);
  dag InOperandList = (ins);
  list<dag> Pattern = [(RetNode)];
  bit isPseudo = true;
  bit isReturn = true;
  bit isBarrier = true;
  bit isTerminator = true;
}
def CALL : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs);
  dag InOperandList = (ins imm16:$sequenceIndex, imm16:$lookupListIndex);
  list<dag> Pattern = [(CallNode imm16:$sequenceIndex, imm16:$lookupListIndex)];
  let AsmString = "CALL $sequenceIndex $lookupListIndex";
  bit isCall = true;
  bit hasSideEffects = true;
}

def otf_ptr_rc : RegisterClassLike;
defm : RemapAllTargetPseudoPointerOperands<otf_ptr_rc>;

def Feature : SubtargetFeature<"feature", "feature", "true", "feature">;

def Proc: Processor<"generic", ProcessorItineraries<[], [], []>, []>;

def OTFInstrInfo : InstrInfo;
def OTF : Target {
  let InstructionSet = OTFInstrInfo;
}

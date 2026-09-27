include "llvm/Target/Target.td"

foreach i = 0-63 in {
  def R#i : Register<"R"#i> {
    let HWEncoding = i;
    let Namespace = "OTF";
  }
}
def GPR8 : RegisterClass<"OTF", [i8, i16], 1, (sequence "R%u", 0, 63)>;

def callNode : SDNode<"OTFISD::CALL", SDTypeProfile<0, 3, [
    SDTCisVT<0, i16>, SDTCisVT<1, i8>, SDTCisVT<2, i8>
]>, [SDNPHasChain]>;

def CC_OTF : CallingConv<[
  CCIfType<[i8], CCAssignToReg<!foreach(i, !range(0, 63), !cast<Register>("R"#i))>>,
  CCIfType<[i8], CCAssignToStack<0, 0>>
]>;

def imm8 : Operand<i8>, ImmLeaf<i8, [{ return isUInt<8>(Imm); }]>;
def imm16 : Operand<i16>, TImmLeaf<i16, [{ return isUInt<16>(Imm); }]>;

class OTFInst<dag ins, dag outs> : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = outs;
  dag InOperandList = ins;
}

def RET : OTFInst<(ins variable_ops), (outs)> {
  bit isPseudo = true;
  bit isReturn = true;
  bit isBarrier = true;
  bit isTerminator = true;
}
def MOV : OTFInst<(ins imm8:$src), (outs GPR8:$dst)> {
  list<dag> Pattern = [(set GPR8:$dst, imm8:$src)];
  bit isPseudo = true;
  bit isMoveImm = true;
}
def ADD : OTFInst<(ins GPR8:$a, imm8:$b), (outs GPR8:$out)> {
  list<dag> Pattern = [(set GPR8:$out, (add i8:$a, imm8:$b))];
  bit isPseudo = true;
  bit isMoveImm = true;
}
def CALL : OTFInst<(ins imm16:$sequenceIndex, imm8:$lookupHi, imm8:$lookupLo), (outs)> {
  list<dag> Pattern = [
    (callNode imm16:$sequenceIndex, tglobaladdr:$lookupHi, tglobaladdr:$lookupLo)
  ];
  let AsmString = "CALL $sequenceIndex $lookupHi $lookupLo";
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

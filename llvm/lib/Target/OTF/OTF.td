include "llvm/Target/Target.td"

foreach i = 0-63 in {
  def R#i : Register<"R"#i> {
    let HWEncoding = i;
    let Namespace = "OTF";
  }
}
def GPR8 : RegisterClass<"OTF", [i1, i8, i16], 1, (sequence "R%u", 0, 63)>;

def CC_OTF : CallingConv<[
  CCIfType<[i8], CCAssignToReg<!foreach(i, !range(0, 63), !cast<Register>("R"#i))>>,
  CCIfType<[i8], CCAssignToStack<0, 0>>
]>;

def imm8 : Operand<i8>, ImmLeaf<i8, [{ return isUInt<8>(Imm); }]>;
def imm16 : Operand<i16>, ImmLeaf<i16, [{ return isUInt<16>(Imm); }]>;

def RET : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs);
  dag InOperandList = (ins variable_ops);
  bit isPseudo = true;
  bit isReturn = true;
  bit isBarrier = true;
  bit isTerminator = true;
}
def MOV : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs GPR8:$dst);
  dag InOperandList = (ins imm8:$src);
  list<dag> Pattern = [(set GPR8:$dst, imm8:$src)];
  bit isPseudo = true;
  bit isMoveImm = true;
}
def ADD : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs GPR8:$out);
  dag InOperandList = (ins GPR8:$a, imm8:$b);
  list<dag> Pattern = [(set GPR8:$out, (add i8:$a, imm8:$b))];
  bit isPseudo = true;
  bit isMoveImm = true;
}
def CALL : Instruction {
  let Namespace = "OTF";
  dag OutOperandList = (outs);
  dag InOperandList = (ins imm16:$sequenceIndex, imm16:$lookupListIndex);
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

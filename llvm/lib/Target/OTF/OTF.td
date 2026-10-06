include "llvm/Target/Target.td"

foreach i = 0-63 in {
  def R#i : Register<"R"#i> {
    let HWEncoding = i;
    let Namespace = "OTF";
  }
}
def GPR8 : RegisterClass<"OTF", [i8, i16], 1, (sequence "R%u", 0, 63)>;

def callNode : SDNode<"OTFISD::CALL", SDTypeProfile<0, 2, [
    SDTCisVT<0, i8>, SDTCisVT<1, i8>
]>, [SDNPHasChain]>;

def CC_OTF : CallingConv<[
  CCIfType<[i8], CCAssignToReg<!foreach(i, !range(0, 63), !cast<Register>("R"#i))>>,
  CCIfType<[i8], CCAssignToStack<0, 0>>
]>;

def imm8 : Operand<i8>, ImmLeaf<i8, [{ return isUInt<8>(Imm); }]>;
def imm16 : Operand<i16>;

class OTFInst<dag ins, dag outs> : Instruction {
  let Namespace = "OTF";
  let OutOperandList = outs;
  let InOperandList = ins;
}

def RET : OTFInst<(ins variable_ops), (outs)> {
  let isPseudo = true;
  let isReturn = true;
  let isBarrier = true;
  let isTerminator = true;
}
def MOV : OTFInst<(ins imm8:$src), (outs GPR8:$dst)> {
  let Pattern = [(set GPR8:$dst, imm8:$src)];
  let isPseudo = true;
  let isMoveImm = true;
}
def ADD : OTFInst<(ins GPR8:$a, imm8:$b), (outs GPR8:$out)> {
  let Pattern = [(set GPR8:$out, (add i8:$a, imm8:$b))];
  let isPseudo = true;
  let isAdd = true;
}
def AND : OTFInst<(ins GPR8:$a, imm8:$b), (outs GPR8:$out)> {
  let Pattern = [(set GPR8:$out, (and i8:$a, imm8:$b))];
  let isPseudo = true;
}

def other : Operand<OtherVT>;
def BR_EQ_FALLTHROUGH : OTFInst<(ins GPR8:$left, GPR8:$right, other:$target), (outs)> {
  let Pattern = [(brcc SETEQ, i8:$left, i8:$right, bb:$target)];
  let isPseudo = true;
  let isBranch = true;
  let isCompare = true;
  let isTerminator = true;
  let usesCustomInserter = true;
}
def BR_EQ_PSEUDO : OTFInst<(ins GPR8:$left, GPR8:$right, other:$true, other:$false), (outs)> {
  let isPseudo = true;
  let isBranch = true;
  let isCompare = true;
  let isTerminator = true;
}
def BR_EQ : OTFInst<(ins imm8:$left, imm8:$right, other:$true, other:$false), (outs)> {
  let AsmString = "BR_EQ $left $right $true $false";
  let isBranch = true;
  let isCompare = true;
  let isTerminator = true;
}
def BR : OTFInst<(ins other:$target), (outs)> {
  let Pattern = [(br bb:$target)];
  let AsmString = "BR $target";
  let isBranch = true;
  let isBarrier = true;
  let isTerminator = true;
  let usesCustomInserter = true;
}
def CALL_SPLIT : OTFInst<(ins GPR8:$lookupHi, GPR8:$lookupLo), (outs)> {
  let Pattern = [(callNode i8:$lookupHi, i8:$lookupLo)];
  let isPseudo = true;
  let isCall = true;
  let hasSideEffects = true;
  let usesCustomInserter = true;
}
def CALL : OTFInst<(ins imm16:$sequenceIndex, imm16:$lookup), (outs)> {
  let AsmString = "CALL $sequenceIndex $lookup";
  let isCall = true;
  let hasSideEffects = true;
}

def otf_ptr_rc : RegisterClassLike;
defm : RemapAllTargetPseudoPointerOperands<otf_ptr_rc>;

def Feature : SubtargetFeature<"feature", "feature", "true", "feature">;

def Proc: Processor<"generic", ProcessorItineraries<[], [], []>, []>;

def OTFInstrInfo : InstrInfo;
def OTF : Target {
  let InstructionSet = OTFInstrInfo;
}

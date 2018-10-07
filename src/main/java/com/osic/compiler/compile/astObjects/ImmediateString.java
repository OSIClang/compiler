package com.osic.compiler.compile.astObjects;

import com.osic.compiler.compile.stackir.Instruction;
import com.osic.compiler.compile.stackir.InstructionSequence;
import com.osic.compiler.compile.stackir.Opcode;

public final class ImmediateString extends StringExpression {

    private final String value;

    public ImmediateString(String value) {
        this.value = value;
    }

    public String getValue() {
        return value;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        ImmediateString that = (ImmediateString) o;

        if (!value.equals(that.value)) return false;

        return true;
    }

    @Override
    public int hashCode() {
        return value.hashCode();
    }

    @Override
    public String toString() {
        return "\"" + value + "\"";
    }

    @Override
    public void compile(InstructionSequence seq) {
        seq.append(new Instruction(Opcode.PUSHS, value));
    }

}

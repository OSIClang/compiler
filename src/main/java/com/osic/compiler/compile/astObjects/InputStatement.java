package com.osic.compiler.compile.astObjects;

import com.osic.compiler.compile.stackir.Instruction;
import com.osic.compiler.compile.stackir.InstructionSequence;
import com.osic.compiler.compile.stackir.Opcode;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public final class InputStatement extends Statement {

    private final List<String> names;

    public InputStatement(String... names) {
        this.names = Collections.unmodifiableList(Arrays.asList(names));
    }

    public InputStatement(List<String> names) {
        this.names = Collections.unmodifiableList(new ArrayList<>(names));
    }

    public List<String> getNames() {
        return names;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        InputStatement that = (InputStatement) o;

        if (!names.equals(that.names)) return false;

        return true;
    }

    @Override
    public int hashCode() {
        return names.hashCode();
    }

    @Override
    public String toString() {
        StringBuilder buf = new StringBuilder("INPUT ");
        for (int i = 0; i < names.size(); i++) {
            buf.append(names.get(i));
            if (i != (names.size() - 1))
                buf.append(", ");
        }
        return buf.toString();
    }

    @Override
    public void compile(InstructionSequence seq) {
        for (String name : names) {
            seq.append(
                new Instruction(Opcode.IN),
                new Instruction(Opcode.STORE, name)
            );
        }
    }

}

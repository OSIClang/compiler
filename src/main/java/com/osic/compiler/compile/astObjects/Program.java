package com.osic.compiler.compile.astObjects;

import com.osic.compiler.compile.stackir.InstructionSequence;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public final class Program {

    private final List<Line> lines;

    public Program(Line... lines) {
        this.lines = Collections.unmodifiableList(Arrays.asList(lines));
    }

    public Program(List<Line> lines) {
        this.lines = Collections.unmodifiableList(new ArrayList<>(lines));
    }

    public List<Line> getLines() {
        return lines;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        Program program = (Program) o;

        if (!lines.equals(program.lines)) return false;

        return true;
    }

    @Override
    public int hashCode() {
        return lines.hashCode();
    }

    @Override
    public String toString() {
        StringBuilder buf = new StringBuilder();
        for (Line line : lines) {
            buf.append(line).append("\n");
        }
        return buf.toString();
    }

    public InstructionSequence compile() {
        InstructionSequence seq = new InstructionSequence();
        for (Line line : lines) {
            line.compile(seq);
        }
        return seq;
    }

}

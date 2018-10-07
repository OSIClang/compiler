package com.osic.compiler.compile.astObjects;

import com.osic.compiler.compile.stackir.Instruction;
import com.osic.compiler.compile.stackir.InstructionSequence;
import com.osic.compiler.compile.stackir.Opcode;

import java.util.Objects;

public final class UnaryExpression extends Expression {

    private final UnaryOperator operator;
    private final Expression expression;

    public UnaryExpression(UnaryOperator operator, Expression expression) {
        this.operator = operator;
        this.expression = expression;
    }

    public UnaryOperator getOperator() {
        return operator;
    }

    public Expression getExpression() {
        return expression;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        UnaryExpression that = (UnaryExpression) o;

        if (!expression.equals(that.expression)) return false;
        if (operator != that.operator) return false;

        return true;
    }

    @Override
    public int hashCode() {
        return Objects.hash(operator, expression);
    }

    @Override
    public String toString() {
        return "(" + operator + expression + ")";
    }

    @Override
    public void compile(InstructionSequence seq) {
        switch (operator) {
            case PLUS:
                expression.compile(seq);
                break;
            case MINUS:
                seq.append(new Instruction(Opcode.PUSHI, 0));
                expression.compile(seq);
                seq.append(new Instruction(Opcode.SUB));
                break;
            default:
                throw new AssertionError();
        }

    }

}

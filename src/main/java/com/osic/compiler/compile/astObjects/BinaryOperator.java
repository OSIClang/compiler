package com.osic.compiler.compile.astObjects;

public enum BinaryOperator {
    PLUS('+'), MINUS('-'), MULT('*'), DIV('/');

    private final char character;

    private BinaryOperator(char character) {
        this.character = character;
    }

    public String toString() {
        return String.valueOf(character);
    }
}

package org.santech.calc;

public class CalculatorUiState {

    private final String expression;
    private final String display;

    public CalculatorUiState(String expression, String display) {
        this.expression = expression;
        this.display = display;
    }

    public String getExpression() {
        return expression;
    }

    public String getDisplay() {
        return display;
    }
}


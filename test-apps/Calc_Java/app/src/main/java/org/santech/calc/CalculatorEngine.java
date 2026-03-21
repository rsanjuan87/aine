package org.santech.calc;

import java.math.BigDecimal;
import java.math.RoundingMode;

public class CalculatorEngine {

    private String displayValue = "0";
    private BigDecimal storedValue = null;
    private CalculatorOperator pendingOperator = null;
    private boolean startNewNumber = false;
    private boolean hasError = false;

    public CalculatorUiState getUiState() {
        String expression;
        if (storedValue != null && pendingOperator != null) {
            expression = format(storedValue) + " " + pendingOperator.getSymbol();
        } else {
            expression = "";
        }

        return new CalculatorUiState(expression, displayValue);
    }

    public void inputDigit(int digit) {
        if (digit < 0 || digit > 9) {
            throw new IllegalArgumentException("Digit must be between 0 and 9");
        }

        prepareForInput();

        String digitText = Integer.toString(digit);
        if (startNewNumber) {
            displayValue = digitText;
        } else if ("0".equals(displayValue)) {
            displayValue = digitText;
        } else if ("-0".equals(displayValue)) {
            displayValue = "-" + digitText;
        } else {
            displayValue = displayValue + digitText;
        }

        startNewNumber = false;
    }

    public void inputDecimal() {
        prepareForInput();

        if (startNewNumber) {
            displayValue = "0.";
            startNewNumber = false;
            return;
        }

        if (!displayValue.contains(".")) {
            displayValue = displayValue + ".";
        }
    }

    public void setOperator(CalculatorOperator operator) {
        if (hasError) {
            clearAll();
        }

        BigDecimal currentValue = currentNumber();

        if (storedValue != null && pendingOperator != null && !startNewNumber) {
            BigDecimal result = applyOperation(storedValue, currentValue, pendingOperator);
            if (hasError) {
                return;
            }
            storedValue = result;
            displayValue = format(result);
        } else if (storedValue == null) {
            storedValue = currentValue;
        }

        pendingOperator = operator;
        startNewNumber = true;
    }

    public void evaluate() {
        if (hasError || storedValue == null || pendingOperator == null || startNewNumber) {
            return;
        }

        BigDecimal result = applyOperation(storedValue, currentNumber(), pendingOperator);
        if (hasError) {
            return;
        }

        displayValue = format(result);
        storedValue = null;
        pendingOperator = null;
        startNewNumber = true;
    }

    public void clearAll() {
        displayValue = "0";
        storedValue = null;
        pendingOperator = null;
        startNewNumber = false;
        hasError = false;
    }

    public void backspace() {
        if (hasError) {
            clearAll();
            return;
        }

        if (startNewNumber) {
            displayValue = "0";
            return;
        }

        displayValue = displayValue.substring(0, Math.max(0, displayValue.length() - 1));
        if (displayValue.isEmpty() || "-".equals(displayValue)) {
            displayValue = "0";
        }
    }

    public void toggleSign() {
        if (hasError) {
            clearAll();
            return;
        }

        if (startNewNumber) {
            displayValue = "-0";
            startNewNumber = false;
            return;
        }

        if ("0".equals(displayValue)) {
            return;
        }

        if (displayValue.startsWith("-")) {
            displayValue = displayValue.substring(1);
        } else {
            displayValue = "-" + displayValue;
        }
    }

    public void percent() {
        if (hasError) {
            clearAll();
            return;
        }

        BigDecimal result = currentNumber().divide(BigDecimal.valueOf(100));
        displayValue = format(result);
        startNewNumber = false;
    }

    private void prepareForInput() {
        if (hasError) {
            clearAll();
        }
    }

    private BigDecimal currentNumber() {
        if ("-0".equals(displayValue)) {
            return BigDecimal.ZERO;
        }
        return new BigDecimal(displayValue);
    }

    private BigDecimal applyOperation(BigDecimal left, BigDecimal right, CalculatorOperator operator) {
        try {
            switch (operator) {
                case ADD:
                    return left.add(right);
                case SUBTRACT:
                    return left.subtract(right);
                case MULTIPLY:
                    return left.multiply(right);
                case DIVIDE:
                    if (right.compareTo(BigDecimal.ZERO) == 0) {
                        throw new ArithmeticException("Division by zero");
                    }
                    return left.divide(right, 12, RoundingMode.HALF_UP);
                default:
                    throw new IllegalStateException("Unsupported operator");
            }
        } catch (ArithmeticException ignored) {
            hasError = true;
            storedValue = null;
            pendingOperator = null;
            startNewNumber = true;
            displayValue = "Error";
            return BigDecimal.ZERO;
        }
    }

    private String format(BigDecimal value) {
        if (value.compareTo(BigDecimal.ZERO) == 0) {
            return "0";
        }

        return value.stripTrailingZeros().toPlainString();
    }
}


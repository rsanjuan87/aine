package org.santech.calc;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class CalculatorEngineTest {

    @Test
    public void performsBasicAddition() {
        CalculatorEngine calculator = new CalculatorEngine();

        calculator.inputDigit(1);
        calculator.inputDigit(2);
        calculator.setOperator(CalculatorOperator.ADD);
        calculator.inputDigit(7);
        calculator.evaluate();

        assertEquals("19", calculator.getUiState().getDisplay());
    }

    @Test
    public void supportsDecimalOperations() {
        CalculatorEngine calculator = new CalculatorEngine();

        calculator.inputDigit(1);
        calculator.inputDecimal();
        calculator.inputDigit(5);
        calculator.setOperator(CalculatorOperator.ADD);
        calculator.inputDigit(2);
        calculator.inputDecimal();
        calculator.inputDigit(2);
        calculator.inputDigit(5);
        calculator.evaluate();

        assertEquals("3.75", calculator.getUiState().getDisplay());
    }

    @Test
    public void handlesDivisionByZero() {
        CalculatorEngine calculator = new CalculatorEngine();

        calculator.inputDigit(7);
        calculator.setOperator(CalculatorOperator.DIVIDE);
        calculator.inputDigit(0);
        calculator.evaluate();

        assertEquals("Error", calculator.getUiState().getDisplay());
    }

    @Test
    public void supportsPercentAndSignChange() {
        CalculatorEngine calculator = new CalculatorEngine();

        calculator.inputDigit(5);
        calculator.inputDigit(0);
        calculator.percent();
        calculator.toggleSign();

        assertEquals("-0.5", calculator.getUiState().getDisplay());
    }

    @Test
    public void removesLastDigitWithBackspace() {
        CalculatorEngine calculator = new CalculatorEngine();

        calculator.inputDigit(1);
        calculator.inputDigit(2);
        calculator.inputDigit(3);
        calculator.backspace();

        assertEquals("12", calculator.getUiState().getDisplay());
    }
}


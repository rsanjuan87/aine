package org.santech.calc

import org.junit.Assert.assertEquals
import org.junit.Test

class CalculatorEngineTest {

	@Test
	fun performs_basic_addition() {
		val calculator = CalculatorEngine()

		calculator.inputDigit(1)
		calculator.inputDigit(2)
		calculator.setOperator(CalculatorOperator.ADD)
		calculator.inputDigit(7)
		calculator.evaluate()

		assertEquals("19", calculator.getUiState().display)
	}

	@Test
	fun supports_decimal_operations() {
		val calculator = CalculatorEngine()

		calculator.inputDigit(1)
		calculator.inputDecimal()
		calculator.inputDigit(5)
		calculator.setOperator(CalculatorOperator.ADD)
		calculator.inputDigit(2)
		calculator.inputDecimal()
		calculator.inputDigit(2)
		calculator.inputDigit(5)
		calculator.evaluate()

		assertEquals("3.75", calculator.getUiState().display)
	}

	@Test
	fun handles_division_by_zero() {
		val calculator = CalculatorEngine()

		calculator.inputDigit(7)
		calculator.setOperator(CalculatorOperator.DIVIDE)
		calculator.inputDigit(0)
		calculator.evaluate()

		assertEquals("Error", calculator.getUiState().display)
	}

	@Test
	fun supports_percent_and_sign_change() {
		val calculator = CalculatorEngine()

		calculator.inputDigit(5)
		calculator.inputDigit(0)
		calculator.percent()
		calculator.toggleSign()

		assertEquals("-0.5", calculator.getUiState().display)
	}

	@Test
	fun removes_last_digit_with_backspace() {
		val calculator = CalculatorEngine()

		calculator.inputDigit(1)
		calculator.inputDigit(2)
		calculator.inputDigit(3)
		calculator.backspace()

		assertEquals("12", calculator.getUiState().display)
	}
}


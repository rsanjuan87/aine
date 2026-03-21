package org.santech.calc

import java.math.BigDecimal
import java.math.RoundingMode

data class CalculatorUiState(
    val expression: String,
    val display: String
)

enum class CalculatorOperator(val symbol: String) {
    ADD("+"),
    SUBTRACT("−"),
    MULTIPLY("×"),
    DIVIDE("÷")
}

class CalculatorEngine {

    private var displayValue = "0"
    private var storedValue: BigDecimal? = null
    private var pendingOperator: CalculatorOperator? = null
    private var startNewNumber = false
    private var hasError = false

    fun getUiState(): CalculatorUiState {
        val expression = if (storedValue != null && pendingOperator != null) {
            "${format(storedValue!!)} ${pendingOperator!!.symbol}"
        } else {
            ""
        }

        return CalculatorUiState(
            expression = expression,
            display = displayValue
        )
    }

    fun inputDigit(digit: Int) {
        require(digit in 0..9) { "Digit must be between 0 and 9" }
        prepareForInput()

        val digitText = digit.toString()
        displayValue = when {
            startNewNumber -> digitText
            displayValue == "0" -> digitText
            displayValue == "-0" -> "-$digitText"
            else -> displayValue + digitText
        }
        startNewNumber = false
    }

    fun inputDecimal() {
        prepareForInput()

        if (startNewNumber) {
            displayValue = "0."
            startNewNumber = false
            return
        }

        if (!displayValue.contains('.')) {
            displayValue += "."
        }
    }

    fun setOperator(operator: CalculatorOperator) {
        if (hasError) {
            clearAll()
        }

        val currentValue = currentNumber()

        if (storedValue != null && pendingOperator != null && !startNewNumber) {
            val result = applyOperation(storedValue!!, currentValue, pendingOperator!!)
            if (hasError) {
                return
            }
            storedValue = result
            displayValue = format(result)
        } else if (storedValue == null) {
            storedValue = currentValue
        }

        pendingOperator = operator
        startNewNumber = true
    }

    fun evaluate() {
        if (hasError || storedValue == null || pendingOperator == null || startNewNumber) {
            return
        }

        val result = applyOperation(storedValue!!, currentNumber(), pendingOperator!!)
        if (hasError) {
            return
        }

        displayValue = format(result)
        storedValue = null
        pendingOperator = null
        startNewNumber = true
    }

    fun clearAll() {
        displayValue = "0"
        storedValue = null
        pendingOperator = null
        startNewNumber = false
        hasError = false
    }

    fun backspace() {
        if (hasError) {
            clearAll()
            return
        }

        if (startNewNumber) {
            displayValue = "0"
            return
        }

        displayValue = displayValue.dropLast(1)
        if (displayValue.isEmpty() || displayValue == "-") {
            displayValue = "0"
        }
    }

    fun toggleSign() {
        if (hasError) {
            clearAll()
            return
        }

        if (startNewNumber) {
            displayValue = "-0"
            startNewNumber = false
            return
        }

        displayValue = when {
            displayValue == "0" -> displayValue
            displayValue.startsWith('-') -> displayValue.removePrefix("-")
            else -> "-$displayValue"
        }
    }

    fun percent() {
        if (hasError) {
            clearAll()
            return
        }

        val result = currentNumber().divide(BigDecimal(100))
        displayValue = format(result)
        startNewNumber = false
    }

    private fun prepareForInput() {
        if (hasError) {
            clearAll()
        }
    }

    private fun currentNumber(): BigDecimal {
        return when (displayValue) {
            "-0" -> BigDecimal.ZERO
            else -> displayValue.toBigDecimal()
        }
    }

    private fun applyOperation(
        left: BigDecimal,
        right: BigDecimal,
        operator: CalculatorOperator
    ): BigDecimal {
        return try {
            when (operator) {
                CalculatorOperator.ADD -> left + right
                CalculatorOperator.SUBTRACT -> left - right
                CalculatorOperator.MULTIPLY -> left * right
                CalculatorOperator.DIVIDE -> {
                    if (right.compareTo(BigDecimal.ZERO) == 0) {
                        throw ArithmeticException("Division by zero")
                    }
                    left.divide(right, 12, RoundingMode.HALF_UP)
                }
            }
        } catch (_: ArithmeticException) {
            hasError = true
            storedValue = null
            pendingOperator = null
            startNewNumber = true
            displayValue = "Error"
            BigDecimal.ZERO
        }
    }

    private fun format(value: BigDecimal): String {
        if (value.compareTo(BigDecimal.ZERO) == 0) {
            return "0"
        }

        return value.stripTrailingZeros().toPlainString()
    }
}


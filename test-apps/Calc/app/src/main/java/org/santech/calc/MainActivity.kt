package org.santech.calc

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import org.santech.calc.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val calculator = CalculatorEngine()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupClickListeners()
        render()
    }

    private fun setupClickListeners() {
        listOf(
            binding.button0 to 0,
            binding.button1 to 1,
            binding.button2 to 2,
            binding.button3 to 3,
            binding.button4 to 4,
            binding.button5 to 5,
            binding.button6 to 6,
            binding.button7 to 7,
            binding.button8 to 8,
            binding.button9 to 9
        ).forEach { (button, digit) ->
            button.setOnClickListener {
                calculator.inputDigit(digit)
                render()
            }
        }

        binding.buttonDecimal.setOnClickListener {
            calculator.inputDecimal()
            render()
        }
        binding.buttonClear.setOnClickListener {
            calculator.clearAll()
            render()
        }
        binding.buttonBackspace.setOnClickListener {
            calculator.backspace()
            render()
        }
        binding.buttonSign.setOnClickListener {
            calculator.toggleSign()
            render()
        }
        binding.buttonPercent.setOnClickListener {
            calculator.percent()
            render()
        }
        binding.buttonAdd.setOnClickListener {
            calculator.setOperator(CalculatorOperator.ADD)
            render()
        }
        binding.buttonSubtract.setOnClickListener {
            calculator.setOperator(CalculatorOperator.SUBTRACT)
            render()
        }
        binding.buttonMultiply.setOnClickListener {
            calculator.setOperator(CalculatorOperator.MULTIPLY)
            render()
        }
        binding.buttonDivide.setOnClickListener {
            calculator.setOperator(CalculatorOperator.DIVIDE)
            render()
        }
        binding.buttonEquals.setOnClickListener {
            calculator.evaluate()
            render()
        }
    }

    private fun render() {
        val state = calculator.getUiState()
        binding.textExpression.text = state.expression
        binding.textDisplay.text = state.display
    }
}

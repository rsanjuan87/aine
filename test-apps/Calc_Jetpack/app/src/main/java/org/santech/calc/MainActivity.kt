package org.santech.calc

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

class MainActivity : ComponentActivity() {

    private val calculator = CalculatorEngine()
    private var uiState by mutableStateOf(calculator.getUiState())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme {
                CalculatorScreen(
                    uiState = uiState,
                    onDigit = { digit ->
                        calculator.inputDigit(digit)
                        render()
                    },
                    onDecimal = {
                        calculator.inputDecimal()
                        render()
                    },
                    onClear = {
                        calculator.clearAll()
                        render()
                    },
                    onBackspace = {
                        calculator.backspace()
                        render()
                    },
                    onToggleSign = {
                        calculator.toggleSign()
                        render()
                    },
                    onPercent = {
                        calculator.percent()
                        render()
                    },
                    onOperator = { operator ->
                        calculator.setOperator(operator)
                        render()
                    },
                    onEquals = {
                        calculator.evaluate()
                        render()
                    }
                )
            }
        }
    }

    private fun render() {
        uiState = calculator.getUiState()
    }
}

private enum class CalcButtonStyle {
    FILLED,
    TONAL,
    OUTLINED
}

@Composable
private fun CalculatorScreen(
    uiState: CalculatorUiState,
    onDigit: (Int) -> Unit,
    onDecimal: () -> Unit,
    onClear: () -> Unit,
    onBackspace: () -> Unit,
    onToggleSign: () -> Unit,
    onPercent: () -> Unit,
    onOperator: (CalculatorOperator) -> Unit,
    onEquals: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Column(
            modifier = Modifier
                .weight(2f)
                .fillMaxWidth(),
            verticalArrangement = Arrangement.Bottom,
            horizontalAlignment = Alignment.End
        ) {
            Text(
                text = uiState.expression,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.End,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 24.sp,
                maxLines = 1
            )
            Text(
                text = uiState.display,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.End,
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 48.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1
            )
        }

        ButtonRow(modifier = Modifier.weight(1f)) {
            CalcButton(stringResource(R.string.button_clear), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight(), onClear)
            CalcButton(stringResource(R.string.button_backspace), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight(), onBackspace)
            CalcButton(stringResource(R.string.button_percent), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight(), onPercent)
            CalcButton(stringResource(R.string.button_divide), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight()) {
                onOperator(CalculatorOperator.DIVIDE)
            }
        }
        ButtonRow(modifier = Modifier.weight(1f)) {
            CalcButton(stringResource(R.string.button_7), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(7) }
            CalcButton(stringResource(R.string.button_8), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(8) }
            CalcButton(stringResource(R.string.button_9), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(9) }
            CalcButton(stringResource(R.string.button_multiply), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight()) {
                onOperator(CalculatorOperator.MULTIPLY)
            }
        }
        ButtonRow(modifier = Modifier.weight(1f)) {
            CalcButton(stringResource(R.string.button_4), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(4) }
            CalcButton(stringResource(R.string.button_5), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(5) }
            CalcButton(stringResource(R.string.button_6), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(6) }
            CalcButton(stringResource(R.string.button_subtract), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight()) {
                onOperator(CalculatorOperator.SUBTRACT)
            }
        }
        ButtonRow(modifier = Modifier.weight(1f)) {
            CalcButton(stringResource(R.string.button_1), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(1) }
            CalcButton(stringResource(R.string.button_2), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(2) }
            CalcButton(stringResource(R.string.button_3), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(3) }
            CalcButton(stringResource(R.string.button_add), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight()) {
                onOperator(CalculatorOperator.ADD)
            }
        }
        ButtonRow(modifier = Modifier.weight(1f)) {
            CalcButton(stringResource(R.string.button_sign), CalcButtonStyle.OUTLINED, Modifier.weight(1f).fillMaxHeight(), onToggleSign)
            CalcButton(stringResource(R.string.button_0), CalcButtonStyle.TONAL, Modifier.weight(1f).fillMaxHeight()) { onDigit(0) }
            CalcButton(stringResource(R.string.button_decimal), CalcButtonStyle.OUTLINED, Modifier.weight(1f).fillMaxHeight(), onDecimal)
            CalcButton(stringResource(R.string.button_equals), CalcButtonStyle.FILLED, Modifier.weight(1f).fillMaxHeight(), onEquals)
        }
    }
}

@Composable
private fun ButtonRow(
    modifier: Modifier = Modifier,
    content: @Composable RowScope.() -> Unit
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        content = content
    )
}

@Composable
private fun CalcButton(
    text: String,
    style: CalcButtonStyle,
    modifier: Modifier,
    onClick: () -> Unit
) {
    when (style) {
        CalcButtonStyle.FILLED -> {
            Button(
                onClick = onClick,
                modifier = modifier
            ) {
                Text(text = text, fontSize = 24.sp)
            }
        }

        CalcButtonStyle.TONAL -> {
            FilledTonalButton(
                onClick = onClick,
                modifier = modifier
            ) {
                Text(text = text, fontSize = 24.sp)
            }
        }

        CalcButtonStyle.OUTLINED -> {
            OutlinedButton(
                onClick = onClick,
                modifier = modifier
            ) {
                Text(text = text, fontSize = 24.sp)
            }
        }
    }
}

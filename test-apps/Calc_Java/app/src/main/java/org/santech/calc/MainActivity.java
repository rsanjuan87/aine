package org.santech.calc;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends Activity {

    private final CalculatorEngine calculator = new CalculatorEngine();

    private TextView textExpression;
    private TextView textDisplay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        textExpression = findViewById(R.id.textExpression);
        textDisplay = findViewById(R.id.textDisplay);

        setupClickListeners();
        render();
    }

    private void setupClickListeners() {
        setDigitClick(R.id.button0, 0);
        setDigitClick(R.id.button1, 1);
        setDigitClick(R.id.button2, 2);
        setDigitClick(R.id.button3, 3);
        setDigitClick(R.id.button4, 4);
        setDigitClick(R.id.button5, 5);
        setDigitClick(R.id.button6, 6);
        setDigitClick(R.id.button7, 7);
        setDigitClick(R.id.button8, 8);
        setDigitClick(R.id.button9, 9);

        setActionClick(R.id.buttonDecimal, v -> calculator.inputDecimal());
        setActionClick(R.id.buttonClear, v -> calculator.clearAll());
        setActionClick(R.id.buttonBackspace, v -> calculator.backspace());
        setActionClick(R.id.buttonSign, v -> calculator.toggleSign());
        setActionClick(R.id.buttonPercent, v -> calculator.percent());
        setActionClick(R.id.buttonAdd, v -> calculator.setOperator(CalculatorOperator.ADD));
        setActionClick(R.id.buttonSubtract, v -> calculator.setOperator(CalculatorOperator.SUBTRACT));
        setActionClick(R.id.buttonMultiply, v -> calculator.setOperator(CalculatorOperator.MULTIPLY));
        setActionClick(R.id.buttonDivide, v -> calculator.setOperator(CalculatorOperator.DIVIDE));
        setActionClick(R.id.buttonEquals, v -> calculator.evaluate());
    }

    private void setDigitClick(int buttonId, int digit) {
        setActionClick(buttonId, v -> calculator.inputDigit(digit));
    }

    private void setActionClick(int buttonId, View.OnClickListener listener) {
        Button button = findViewById(buttonId);
        button.setOnClickListener(v -> {
            listener.onClick(v);
            render();
        });
    }

    private void render() {
        CalculatorUiState state = calculator.getUiState();
        textExpression.setText(state.getExpression());
        textDisplay.setText(state.getDisplay());
    }
}


# Calc

Aplicación Android de calculadora básica con una sola `Activity`.

## Funciones

- Suma, resta, multiplicación y división
- Decimales
- Porcentaje
- Cambio de signo (`±`)
- Borrado completo (`AC`)
- Borrado del último dígito (`⌫`)
- Manejo de división por cero mostrando `Error`

## Estructura principal

- `app/src/main/java/org/santech/calc/MainActivity.java`: UI y eventos con `Activity` + `findViewById`
- `app/src/main/java/org/santech/calc/CalculatorEngine.java`: lógica de cálculo
- `app/src/main/res/layout/activity_main.xml`: pantalla única con widgets Android nativos (`Button`)
- `app/src/test/java/org/santech/calc/CalculatorEngineTest.java`: pruebas unitarias del motor

## Validación rápida

```zsh
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
```


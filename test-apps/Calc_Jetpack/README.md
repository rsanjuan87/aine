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

- `app/src/main/java/org/santech/calc/MainActivity.kt`: UI y eventos con Jetpack Compose
- `app/src/main/java/org/santech/calc/CalculatorEngine.kt`: lógica de cálculo
- `app/src/test/java/org/santech/calc/CalculatorEngineTest.kt`: pruebas unitarias del motor

## Validación rápida

```zsh
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
```


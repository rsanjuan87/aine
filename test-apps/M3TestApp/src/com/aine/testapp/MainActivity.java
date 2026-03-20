package com.aine.testapp;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * AINE M3 TestApp — demuestra el ciclo de vida completo de una Activity.
 *
 * Secuencia de ejecución:
 *   onCreate  → onStart → onResume → (3 segundos) → finish()
 *                                                    → onPause → onStop → onDestroy
 *
 * El proceso termina con código 0 (sin crash).
 */
public class MainActivity extends Activity {

    static final String TAG = "AINE-M3";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "onCreate — AINE M3 funcional");

        // Auto-terminar tras 3 s para completar el ciclo sin interacción
        new Handler(Looper.getMainLooper()).postDelayed(this::finish, 3000);
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.i(TAG, "onStart");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.i(TAG, "onStop");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy — ciclo de vida completo");
    }
}

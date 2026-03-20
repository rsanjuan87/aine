# M3 — Primera app sin gráficos ejecuta

**Target:** Meses 5–7
**Criterio de éxito:** Una app Android de utilidad/consola completa su ciclo de vida sin crash.
**Prerequisito:** M2 completado (Binder funcional)

---

## Qué significa "sin gráficos"

Para M3 usamos apps que no usen `android.view` ni OpenGL. Existen apps reales con este perfil:
- Apps de terminal
- Servicios en background (sync, workers)
- Apps de prueba del AOSP (CTS tests)

El objetivo no es UI — es demostrar que `onCreate → onResume → onPause → onDestroy` funciona.

## Componentes a implementar

### PackageManager mínimo

```java
// Funcionalidad mínima para M3:
// - Parsear AndroidManifest.xml de un APK
// - Registrar el package name y la Activity principal
// - Resolver Intents básicos (ACTION_MAIN)

// El PackageManager de AOSP es enorme — para M3 solo necesitamos:
PackageInfo getPackageInfo(String packageName, int flags);
ActivityInfo[] getActivities(String packageName);
ResolveInfo resolveActivity(Intent intent, int flags);
```

### ActivityManagerService mínimo

```
Para M3, AMS gestiona:
- Un proceso por app (posix_spawn con aine-shim inyectado)
- El Intent de arranque: ACTION_MAIN con la Activity principal
- Los callbacks de ciclo de vida: CREATE, START, RESUME, PAUSE, STOP, DESTROY
- Terminación limpia cuando se cierra la ventana (o SIGTERM)

No necesario para M3:
- Back stack de Activities
- Múltiples tareas (tasks)
- Fragments
- Services en segundo plano (para apps más complejas, M5+)
```

### App de prueba: AINE TestApp

Para M3, crear una app Android mínima de prueba que:
1. Imprime "AINE M3: onCreate" en Logcat cuando arranca
2. Espera 2 segundos
3. Imprime "AINE M3: onResume"
4. Responde a un Intent de cierre
5. Imprime "AINE M3: onDestroy"

```java
// test-apps/M3TestApp/src/MainActivity.java
package com.aine.testapp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends Activity {
    private static final String TAG = "AINE-M3";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "onCreate — AINE M3 funcional");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy — ciclo de vida completo");
    }
}
```

## Proceso de arranque de una app en M3

```
1. ./scripts/run-app.sh M3TestApp.apk
        │
        ▼
2. aine-launcher invoca PackageManager
   → parsea AndroidManifest.xml
   → obtiene: package=com.aine.testapp, mainActivity=MainActivity
        │
        ▼
3. ActivityManagerService lanza el proceso:
   posix_spawn(
     argv: [dalvikvm, -cp, /path/to/app.dex, android.app.ActivityThread],
     env:  [DYLD_INSERT_LIBRARIES=libaine-shim.dylib,
            AINE_PACKAGE=com.aine.testapp, ...]
   )
        │
        ▼
4. ActivityThread arranca en el nuevo proceso
   → se conecta a system_server via Binder
   → recibe Intent de AMS: {action: MAIN, component: MainActivity}
        │
        ▼
5. ART instancia MainActivity
   → llama onCreate(), onStart(), onResume()
        │
        ▼
6. App corre hasta que se envía SIGTERM o Intent de cierre
   → onPause(), onStop(), onDestroy()
   → proceso termina con código 0
```

## Definition of Done — M3

- [x] `M3TestApp.apk` se compila con `./test-apps/M3TestApp/build.sh`
- [x] Al ejecutar, se ven los logs: `onCreate`, `onStart`, `onResume`
- [x] Al cerrar (finish() auto tras 3s), se ven: `onPause`, `onStop`, `onDestroy`
- [x] El proceso termina con código 0 (sin crash)
- [x] Logcat de AINE captura y muestra los logs correctamente (`aine-launcher lifecycle`)
- [x] `./scripts/run-app.sh --m3` ejecuta el ciclo completo en ~6 segundos
- [ ] Una app real de AOSP (ej: el test runner de AOSP) también funciona → M4

**Estado: COMPLETADO** (marzo 2026)
- Implementación via adb bridge (emulador arm64-v8a Android 36.1)
- `aine-launcher`: binario C++ que parsea APK, instala, lanza y verifica lifecycle via logcat
- M3TestApp.java: Activity con Handler.postDelayed(finish, 3000ms) para ciclo autónomo
- Ciclo verificado: onCreate → onStart → onResume → onPause → onStop → onDestroy ✓

## Siguiente: M4

Con el ciclo de vida funcionando, M4 añade gráficos.
Ver: `docs/milestones/M4-graphics.md` (pendiente de crear)

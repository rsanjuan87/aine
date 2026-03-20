// M3LifecycleTest.java — Native M3 Activity lifecycle test for AINE
//
// Demonstrates Android Activity lifecycle WITHOUT the Android emulator.
// Runs with aine-dalvik (native macOS ARM64 DEX interpreter).
//
// This is the proper AINE approach:
//   DYLD_INSERT_LIBRARIES=libaine-shim.dylib dalvikvm -cp classes.dex M3LifecycleTest
//
// When ART is ported to macOS (B6 resolved), this will be replaced by a
// real android.app.Activity subclass running with the actual Android framework.

public class M3LifecycleTest {

    // ── Lifecycle callbacks (mirror android.app.Activity) ────────────────────
    public void onCreate()  { System.out.println("AINE-M3: onCreate");  }
    public void onStart()   { System.out.println("AINE-M3: onStart");   }
    public void onResume()  { System.out.println("AINE-M3: onResume");  }
    public void onPause()   { System.out.println("AINE-M3: onPause");   }
    public void onStop()    { System.out.println("AINE-M3: onStop");    }
    public void onDestroy() { System.out.println("AINE-M3: onDestroy"); }

    // ── Driver: simulates ActivityManagerService driving the lifecycle ────────
    public static void main(String[] args) {
        System.out.println("AINE M3: iniciando ciclo de vida (sin emulador)");
        System.out.println("Runtime: " + System.getProperty("java.vendor"));
        System.out.println("Arch: "    + System.getProperty("os.arch"));
        System.out.println("---");

        M3LifecycleTest activity = new M3LifecycleTest();

        // Forward lifecycle (AMS normally sends these Binder messages)
        activity.onCreate();
        activity.onStart();
        activity.onResume();

        // Simulate app doing work
        System.out.println("AINE-M3: [app running...]");

        // Reverse lifecycle (user presses back / app finishes)
        activity.onPause();
        activity.onStop();
        activity.onDestroy();

        System.out.println("---");
        System.out.println("AINE M3: ciclo de vida completado OK");
    }
}

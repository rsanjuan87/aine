/**
 * G5WindowActivity — Activity lifecycle test for --window mode (G5).
 *
 * This class has no main() method, so dalvikvm runs it in Activity mode:
 *   onCreate → onStart → onResume → [handler_drain] → onPause → onStop → onDestroy
 *
 * With --window, NSApplication + NSWindow are created first, and the lifecycle
 * runs on a background thread while the main thread pumps the NSRunLoop.
 * Without a display (headless CI), the window is silently skipped but the
 * lifecycle still runs — so the CTest still passes.
 *
 * Expected output (to stderr):
 *   [aine-dalvik] Activity mode: LG5WindowActivity;
 *   g5-window: onCreate
 *   g5-window: onStart
 *   g5-window: onResume
 *   g5-window: onPause
 *   g5-window: onStop
 *   g5-window: onDestroy
 *   g5-window: done
 */
public class G5WindowActivity {

    private String mName = "G5WindowActivity";

    public void onCreate(android.os.Bundle savedInstanceState) {
        System.err.println("g5-window: onCreate");
        // Simulate saving state via instance field
        mName = "G5Active";
    }

    public void onStart() {
        System.err.println("g5-window: onStart");
    }

    public void onResume() {
        System.err.println("g5-window: onResume");
        // Verify StringBuilder works in window mode
        StringBuilder sb = new StringBuilder();
        sb.append("g5-");
        sb.append("window");
        sb.append(": name=");
        sb.append(mName);
        System.err.println(sb.toString());
    }

    public void onPause() {
        System.err.println("g5-window: onPause");
    }

    public void onStop() {
        System.err.println("g5-window: onStop");
    }

    public void onDestroy() {
        System.err.println("g5-window: onDestroy");
        System.err.println("g5-window: done");
    }
}

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * HandlerTest — verifies Handler.postDelayed fires in AINE Activity mode.
 *
 * Flow:
 *   onCreate  -> schedules a 100ms callback
 *   onResume  -> logs "onResume"
 *   [handler fires after 100ms -> logs "handler-fired"]
 *   onPause   -> logs "onPause"
 *   onDestroy -> logs "onDestroy"
 */
public class HandlerTest extends Activity {
    static final String TAG = "AINE-G1";

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        Log.i(TAG, "onCreate");
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            @Override public void run() {
                Log.i(TAG, "handler-fired");
            }
        }, 100);
    }

    @Override protected void onStart()   { super.onStart();   Log.i(TAG, "onStart"); }
    @Override protected void onResume()  { super.onResume();  Log.i(TAG, "onResume"); }
    @Override protected void onPause()   { super.onPause();   Log.i(TAG, "onPause"); }
    @Override protected void onStop()    { super.onStop();    Log.i(TAG, "onStop"); }
    @Override protected void onDestroy() { super.onDestroy(); Log.i(TAG, "onDestroy"); }
}

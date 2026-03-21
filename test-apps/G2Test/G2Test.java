import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * G2Test — verifies stdlib extensions in AINE Activity mode.
 *
 * Checks:
 *   T1: System.currentTimeMillis() > 0                   → "time-ok"
 *   T2: ArrayList add/get/size                           → "list-size:3" / "list-get:beta"
 *   T3: HashMap put/get/size                             → "map-get:val1" / "map-size:2"
 *   T4: Math.max(int, int)                               → "math-max:20"
 *   T5: String.format basic %d/%s                        → "fmt-42-hello"
 */
public class G2Test extends Activity {
    static final String TAG = "AINE-G2";

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);

        // T1: System time
        long now = System.currentTimeMillis();
        Log.i(TAG, now > 0 ? "time-ok" : "time-fail");

        // T2: ArrayList
        ArrayList<String> list = new ArrayList<String>();
        list.add("alpha");
        list.add("beta");
        list.add("gamma");
        Log.i(TAG, "list-size:" + list.size());
        Log.i(TAG, "list-get:" + list.get(1));

        // T3: HashMap (String->String; no Integer autoboxing)
        HashMap<String, String> map = new HashMap<String, String>();
        map.put("key1", "val1");
        map.put("key2", "val2");
        Log.i(TAG, "map-get:" + map.get("key1"));
        Log.i(TAG, "map-size:" + map.size());

        // T4: Math.max
        int mx = Math.max(10, 20);
        Log.i(TAG, "math-max:" + mx);

        // T5: String.format
        String s = String.format("fmt-%d-%s", 42, "hello");
        Log.i(TAG, s);

        Log.i(TAG, "g2-done");
    }

    @Override protected void onResume()  { super.onResume(); }
    @Override protected void onPause()   { super.onPause(); }
    @Override protected void onDestroy() { super.onDestroy(); }
}

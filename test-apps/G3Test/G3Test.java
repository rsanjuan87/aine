import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;

/**
 * G3Test — verifies G3 runtime extensions in AINE Activity mode.
 *
 * Checks:
 *   T1: try/catch                        → "exc-caught"
 *   T2: Thread.sleep (≥0 ms)             → "sleep-ok"
 *   T3: ArrayList for-each (iterator)    → "iter:foo,bar,baz,"
 *   T4: String.split(",")                → "split-len:4" / "split-2:c"
 *   T5: String.replace                   → "replace:hello_world"
 *   T6: Collections.sort                 → "sort-0:apple"
 */
public class G3Test extends Activity {
    static final String TAG = "AINE-G3";

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);

        // T1: try/catch
        try {
            throw new IllegalStateException("test-exc");
        } catch (Exception e) {
            Log.i(TAG, "exc-caught");
        }

        // T2: Thread.sleep
        long before = System.currentTimeMillis();
        try {
            Thread.sleep(20);
        } catch (Exception e) { /* ignored */ }
        long after = System.currentTimeMillis();
        Log.i(TAG, after >= before ? "sleep-ok" : "sleep-fail");

        // T3: ArrayList for-each (iterator)
        ArrayList<String> names = new ArrayList<String>();
        names.add("foo");
        names.add("bar");
        names.add("baz");
        StringBuilder sb = new StringBuilder();
        Iterator<String> it = names.iterator();
        while (it.hasNext()) {
            sb.append(it.next());
            sb.append(",");
        }
        Log.i(TAG, "iter:" + sb.toString());

        // T4: String.split
        String csv = "a,b,c,d";
        String[] parts = csv.split(",");
        Log.i(TAG, "split-len:" + parts.length);
        Log.i(TAG, "split-2:" + parts[2]);

        // T5: String.replace
        String replaced = "hello world".replace(" ", "_");
        Log.i(TAG, "replace:" + replaced);

        // T6: Collections.sort
        ArrayList<String> fruits = new ArrayList<String>();
        fruits.add("cherry");
        fruits.add("apple");
        fruits.add("banana");
        Collections.sort(fruits);
        Log.i(TAG, "sort-0:" + fruits.get(0));

        Log.i(TAG, "g3-done");
    }

    @Override protected void onResume()  { super.onResume(); }
    @Override protected void onPause()   { super.onPause(); }
    @Override protected void onDestroy() { super.onDestroy(); }
}

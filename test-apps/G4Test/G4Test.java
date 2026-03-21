import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/**
 * G4Test — verifies G4 advanced runtime extensions in AINE Activity mode.
 *
 * Checks:
 *   T1: Arrays.asList                    → "asList-size:3"
 *   T2: Collections.sort + reverse       → "sorted:apple" / "reversed:cherry"
 *   T3: String.replaceAll                → "replaceAll:hello-world-test"
 *   T4: Integer.MAX_VALUE                → "max-val:2147483647"
 *   T5: String.contains/startsWith       → "contains:true" / "starts:true"
 *   T6: String.split with array access   → "parts-1:B"
 *   T7: Math operations                  → "abs:-5->5"
 *   T8: StringBuilder operations         → "sb:Hello World"
 */
public class G4Test extends Activity {
    static final String TAG = "AINE-G4";

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);

        // T1: Arrays.asList
        String[] arr = {"x", "y", "z"};
        ArrayList<String> fromArr = new ArrayList<String>(Arrays.asList(arr));
        Log.i(TAG, "asList-size:" + fromArr.size());

        // T2: Collections.sort + reverse
        ArrayList<String> fruits = new ArrayList<String>();
        fruits.add("cherry");
        fruits.add("apple");
        fruits.add("banana");
        Collections.sort(fruits);
        Log.i(TAG, "sorted:" + fruits.get(0));
        Collections.reverse(fruits);
        Log.i(TAG, "reversed:" + fruits.get(0));

        // T3: String.replaceAll
        String r = "hello world test".replaceAll(" ", "-");
        Log.i(TAG, "replaceAll:" + r);

        // T4: Integer.MAX_VALUE static field
        int maxVal = Integer.MAX_VALUE;
        Log.i(TAG, "max-val:" + maxVal);

        // T5: String.contains / startsWith
        String s = "Hello World";
        Log.i(TAG, "contains:" + (s.contains("World") ? "true" : "false"));
        Log.i(TAG, "starts:" + (s.startsWith("Hello") ? "true" : "false"));

        // T6: String.split + array index
        String csv = "A,B,C";
        String[] parts = csv.split(",");
        Log.i(TAG, "parts-1:" + parts[1]);

        // T7: Math.abs
        int neg = -5;
        int abs = Math.abs(neg);
        Log.i(TAG, "abs:" + neg + "->" + abs);

        // T8: StringBuilder chaining
        String result = new StringBuilder()
            .append("Hello")
            .append(" ")
            .append("World")
            .toString();
        Log.i(TAG, "sb:" + result);

        Log.i(TAG, "g4-done");
    }

    @Override protected void onResume()  { super.onResume(); }
    @Override protected void onPause()   { super.onPause(); }
    @Override protected void onDestroy() { super.onDestroy(); }
}

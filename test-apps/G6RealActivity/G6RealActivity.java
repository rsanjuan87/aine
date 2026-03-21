import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.LinearLayout;
import android.graphics.Color;

/**
 * G6RealActivity — tests the View/widget/graphics stubs added in G6.
 *
 * Exercises:
 *   T1: setContentView (no-op, must not crash)
 *   T2: TextView.setText / getText
 *   T3: Button.setOnClickListener (store listener, must not crash)
 *   T4: Toast.makeText / show → stderr
 *   T5: Activity.getWindow (returns stub)
 *   T6: Activity.finish() → logs, continues lifecycle teardown
 *   T7: Color.rgb static
 *   T8: LinearLayout stub
 *   T9: Handler.postDelayed from Activity
 *  T10: ResourceId-based ops (getString returns empty, no crash)
 *
 * Expected key outputs in stderr:
 *   g6-onCreate, g6-textview:Hello AINE, g6-color:ok, g6-toast-shown,
 *   g6-layout:ok, g6-finish-called, g6-onDestroy, g6-done
 */
public class G6RealActivity extends Activity {

    static final String TAG = "AINE-G6";
    private TextView mDisplay;
    private int mClickCount = 0;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // T1: setContentView stub (no actual layout inflation, just no crash)
        setContentView(0x7f040001); // R.layout.activity_main
        System.err.println("g6-onCreate");

        // T2: TextView stub
        mDisplay = new TextView(this);
        mDisplay.setText("Hello AINE");
        String text = mDisplay.getText().toString();
        System.err.println("g6-textview:" + text);

        // T3: Button + OnClickListener stub
        Button btn = new Button(this);
        btn.setText("Click me");
        btn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                mClickCount++;
                System.err.println("g6-clicked:" + mClickCount);
            }
        });

        // T4: Toast stub
        Toast.makeText(this, "AINE toast test", Toast.LENGTH_SHORT).show();
        System.err.println("g6-toast-shown");

        // T5: getWindow stub
        android.view.Window win = getWindow();
        if (win != null) {
            win.addFlags(0x80000000); // FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS
        }

        // T6: Activity.finish() stub  
        System.err.println("g6-finish-called");
        finish();

        // T7: Color.rgb static
        int red = Color.rgb(255, 0, 0);
        System.err.println("g6-color:" + (red != 0 ? "ok" : "fail"));

        // T8: LinearLayout stub
        LinearLayout ll = new LinearLayout(this);
        ll.setOrientation(LinearLayout.VERTICAL);
        ll.addView(mDisplay);
        ll.addView(btn);
        System.err.println("g6-layout:ok");

        // T9: Handler from Activity context
        new Handler(Looper.getMainLooper()).postDelayed(new Runnable() {
            public void run() {
                System.err.println("g6-handler-fired");
            }
        }, 50);
    }

    @Override
    public void onResume() {
        super.onResume();
        // T10: Resources.getString stub (returns empty string, no crash)
        String s = getString(0x7f050001); // R.string.app_name
        System.err.println("g6-resources-ok");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.err.println("g6-onDestroy");
        System.err.println("g6-done");
    }
}

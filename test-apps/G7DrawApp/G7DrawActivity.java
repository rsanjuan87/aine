/**
 * G7DrawActivity — Prueba final: renderizado Canvas en ventana macOS nativa.
 *
 * Demuestra ciclo completo:
 *  1. setContentView(View) → registra drawView para dispatch de onDraw
 *  2. Handler.postDelayed → actualiza contador cada 200 ms
 *  3. view.invalidate() → dispara onDraw en cada tick
 *  4. onDraw: drawColor (fondo), drawText (título, contador), drawRect (botón),
 *             drawCircle (indicador) — todos mapeados a CoreGraphics
 *  5. Tras 5 ticks → finish() → onDestroy
 *
 * Salida esperada en stderr:
 *   [G7] onCreate — AINE Draw Test
 *   [G7] frame:1 … [G7] frame:5
 *   [G7] done — all frames rendered
 *   [G7] onDestroy
 *   [G7] draw-complete
 */

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

public class G7DrawActivity extends Activity {

    private int     mFrameCount = 0;
    private Handler mHandler;
    private DrawView mView;

    /* Inner custom View with onDraw override */
    static class DrawView extends View {
        private int mFrames = 0;

        public DrawView(Context ctx) {
            super(ctx);
        }

        public void setFrameCount(int n) { mFrames = n; }

        @Override
        protected void onDraw(Canvas canvas) {
            /* Background: dark blue */
            canvas.drawColor(Color.rgb(20, 20, 50));

            /* Title */
            Paint titlePaint = new Paint();
            titlePaint.setColor(Color.WHITE);
            titlePaint.setTextSize(32);
            canvas.drawText("AINE Android Runtime", 50, 80, titlePaint);

            /* Frame counter */
            Paint counterPaint = new Paint();
            counterPaint.setColor(Color.rgb(0, 220, 100));
            counterPaint.setTextSize(24);
            canvas.drawText("Frame: " + mFrames, 50, 130, counterPaint);

            /* Blue button rectangle */
            Paint btnPaint = new Paint();
            btnPaint.setColor(Color.rgb(70, 130, 220));
            canvas.drawRect(50, 160, 280, 210, btnPaint);

            /* Button label */
            Paint btnTextPaint = new Paint();
            btnTextPaint.setColor(Color.WHITE);
            btnTextPaint.setTextSize(20);
            canvas.drawText("Running...", 85, 192, btnTextPaint);

            /* Status circle — red dot top-right */
            Paint dotPaint = new Paint();
            dotPaint.setColor(Color.rgb(230, 50, 50));
            canvas.drawCircle(720, 60, 25, dotPaint);

            /* Status text */
            Paint statusPaint = new Paint();
            statusPaint.setColor(Color.rgb(200, 200, 200));
            statusPaint.setTextSize(16);
            canvas.drawText("macOS ARM64", 50, 260, statusPaint);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        System.err.println("[G7] onCreate — AINE Draw Test");

        mView = new DrawView(this);
        setContentView(mView);          /* triggers initial onDraw */

        mHandler = new Handler(Looper.getMainLooper());
        scheduleFrame();
    }

    private void scheduleFrame() {
        mHandler.postDelayed(new Runnable() {
            public void run() {
                mFrameCount++;
                mView.setFrameCount(mFrameCount);
                System.err.println("[G7] frame:" + mFrameCount);
                mView.invalidate();     /* triggers onDraw via event loop */

                if (mFrameCount < 5) {
                    scheduleFrame();
                } else {
                    System.err.println("[G7] done — all frames rendered");
                    finish();
                }
            }
        }, 200);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        System.err.println("[G7] onDestroy");
        System.err.println("[G7] draw-complete");
    }
}

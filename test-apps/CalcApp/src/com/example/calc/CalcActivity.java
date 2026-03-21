/*
 * CalcActivity.java — Visual integer calculator for AINE T9.4
 *
 * A basic 4-operation calculator rendered via Canvas on an 800×600 NSWindow.
 * Uses onDraw() for rendering and onTouchEvent() via the Activity for input.
 *
 * Licensed under Apache 2.0 (original work for AINE project).
 */
package com.example.calc;

import android.app.Activity;
import android.os.Bundle;
import android.content.Context;
import android.view.View;
import android.view.MotionEvent;
import android.graphics.Canvas;
import android.graphics.Paint;

public class CalcActivity extends Activity {

    private CalcView mView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mView = new CalcView(this);
        setContentView(mView);
        System.out.println("[calc] ready");
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // ACTION_DOWN = 0
        if (mView != null && event.getAction() == 0) {
            mView.onTap((int) event.getX(), (int) event.getY());
        }
        return true;
    }

    // ── Calculator View ───────────────────────────────────────────────────
    static class CalcView extends View {

        // ── State ────────────────────────────────────────────────────────
        private String  mDisplay;    // what the display shows
        private int     mLeft;       // left-hand operand
        private int     mOp;         // 0=none 1=+ 2=- 3=* 4=/
        private boolean mReset;      // next digit clears display

        // ── Colors (ARGB) ────────────────────────────────────────────────
        private static final int C_BG    = 0xFF1C1C1E;
        private static final int C_DISP  = 0xFF2C2C2E;
        private static final int C_NUM   = 0xFF3A3A3C;
        private static final int C_OP    = 0xFFFF9500;   // orange for operators
        private static final int C_EQ    = 0xFF30D158;   // green for =
        private static final int C_CTRL  = 0xFF636366;   // grey for C
        private static final int C_WHITE = 0xFFFFFFFF;

        // ── Layout constants (800×600 window) ────────────────────────────
        private static final float DISP_H  = 150.0f;
        private static final float BTN_W   = 200.0f;
        private static final float BTN_H   = 112.0f;
        private static final float GAP     = 2.0f;

        // Button labels — instance array to avoid static-initializer issues
        private final String[] mLabels;

        private final Paint mPaint = new Paint();

        public CalcView(Context ctx) {
            super(ctx);
            mDisplay = "0";
            mLeft    = 0;
            mOp      = 0;
            mReset   = true;
            mLabels = new String[16];
            mLabels[0]  = "7"; mLabels[1]  = "8"; mLabels[2]  = "9"; mLabels[3]  = "/";
            mLabels[4]  = "4"; mLabels[5]  = "5"; mLabels[6]  = "6"; mLabels[7]  = "*";
            mLabels[8]  = "1"; mLabels[9]  = "2"; mLabels[10] = "3"; mLabels[11] = "-";
            mLabels[12] = "C"; mLabels[13] = "0"; mLabels[14] = "="; mLabels[15] = "+";
        }

        // ── Touch handling ───────────────────────────────────────────────
        public void onTap(int x, int y) {
            if (y < (int) DISP_H) return;
            int col = x / (int) BTN_W;
            int row = (y - (int) DISP_H) / (int) BTN_H;
            if (col < 0 || col >= 4 || row < 0 || row >= 4) return;
            press(mLabels[row * 4 + col]);
            invalidate();
        }

        private void press(String btn) {
            if (btn.equals("C")) {
                mDisplay = "0";
                mLeft  = 0;
                mOp    = 0;
                mReset = true;
            } else if (btn.equals("=")) {
                if (mOp != 0) {
                    int right  = Integer.parseInt(mDisplay);
                    int result = 0;
                    if      (mOp == 1) result = mLeft + right;
                    else if (mOp == 2) result = mLeft - right;
                    else if (mOp == 3) result = mLeft * right;
                    else if (mOp == 4) result = (right != 0) ? mLeft / right : 0;
                    mDisplay = "" + result;
                    mOp    = 0;
                    mReset = true;
                    System.out.println("[calc] result:" + mDisplay);
                }
            } else if (btn.equals("+") || btn.equals("-") ||
                       btn.equals("*") || btn.equals("/")) {
                mLeft  = Integer.parseInt(mDisplay);
                mOp    = btn.equals("+") ? 1
                       : btn.equals("-") ? 2
                       : btn.equals("*") ? 3 : 4;
                mReset = true;
            } else {
                if (mReset) {
                    mDisplay = btn;
                    mReset   = false;
                } else if (mDisplay.length() < 10) {
                    if (mDisplay.equals("0")) mDisplay = btn;
                    else mDisplay = mDisplay + btn;
                }
            }
        }

        // ── Drawing ──────────────────────────────────────────────────────
        @Override
        protected void onDraw(Canvas canvas) {
            // Background
            canvas.drawColor(C_BG);

            // Display area background
            mPaint.setColor(C_DISP);
            canvas.drawRect(0.0f, 0.0f, 800.0f, DISP_H, mPaint);

            // Display text: right-justified, textSize 72
            mPaint.setColor(C_WHITE);
            mPaint.setTextSize(72.0f);
            int len = mDisplay.length();
            float tw = len * 42.0f;
            float tx = 780.0f - tw;
            if (tx < 10.0f) tx = 10.0f;
            canvas.drawText(mDisplay, tx, 116.0f, mPaint);

            // Op indicator (small, top-left)
            if (mOp != 0) {
                mPaint.setColor(C_OP);
                mPaint.setTextSize(30.0f);
                String opStr = mOp == 1 ? "+" : mOp == 2 ? "-" : mOp == 3 ? "*" : "/";
                canvas.drawText(opStr, 20.0f, 40.0f, mPaint);
            }

            // Button grid
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                    String lbl = mLabels[row * 4 + col];
                    float bx = col * BTN_W;
                    float by = DISP_H + row * BTN_H;

                    int btnColor;
                    if (lbl.equals("C")) {
                        btnColor = C_CTRL;
                    } else if (lbl.equals("=")) {
                        btnColor = C_EQ;
                    } else if (lbl.equals("+") || lbl.equals("-") ||
                               lbl.equals("*") || lbl.equals("/")) {
                        btnColor = C_OP;
                    } else {
                        btnColor = C_NUM;
                    }

                    mPaint.setColor(btnColor);
                    canvas.drawRect(bx + GAP, by + GAP,
                                    bx + BTN_W - GAP, by + BTN_H - GAP,
                                    mPaint);

                    // Centered label
                    mPaint.setColor(C_WHITE);
                    mPaint.setTextSize(38.0f);
                    float lx = bx + BTN_W / 2.0f - 11.0f;
                    float ly = by + BTN_H / 2.0f + 13.0f;
                    canvas.drawText(lbl, lx, ly, mPaint);
                }
            }
        }
    }
}

 *
 * A basic 4-operation calculator rendered via Canvas on an 800×600 NSWindow.
 * Uses onDraw() for rendering and onTouchEvent() via the Activity for input.
 *
 * Licensed under Apache 2.0 (original work for AINE project).
 */
package com.example.calc;

import android.app.Activity;
import android.os.Bundle;
import android.content.Context;
import android.view.View;
import android.view.MotionEvent;
import android.graphics.Canvas;
import android.graphics.Paint;

public class CalcActivity extends Activity {

    private CalcView mView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mView = new CalcView(this);
        setContentView(mView);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // ACTION_DOWN = 0
        if (mView != null && event.getAction() == 0) {
            mView.onTap((int) event.getX(), (int) event.getY());
        }
        return true;
    }

    // ── Calculator View ───────────────────────────────────────────────────
    static class CalcView extends View {

        // ── State ────────────────────────────────────────────────────────
        private String  mDisplay = "0";   // what the display shows
        private int     mLeft    = 0;     // left-hand operand
        private int     mOp      = 0;     // 0=none 1=+ 2=- 3=* 4=/
        private boolean mReset   = true;  // next digit clears display

        // ── Colors (ARGB) ────────────────────────────────────────────────
        private static final int C_BG    = 0xFF1C1C1E;
        private static final int C_DISP  = 0xFF2C2C2E;
        private static final int C_NUM   = 0xFF3A3A3C;
        private static final int C_OP    = 0xFFFF9500;   // orange for operators
        private static final int C_EQ    = 0xFF30D158;   // green for =
        private static final int C_CTRL  = 0xFF636366;   // grey for C
        private static final int C_WHITE = 0xFFFFFFFF;
        private static final int C_DISP_TXT = 0xFFFFFFFF;

        // ── Layout constants (800×600 window) ────────────────────────────
        // Display area: top 150px
        private static final float DISP_H  = 150.0f;
        // Button grid: 4 cols × 4 rows filling remaining 450px
        private static final float BTN_W   = 200.0f;   // 800/4
        private static final float BTN_H   = 112.0f;   // 450/4 ≈ 112
        private static final float GAP     = 2.0f;     // inset per side

        // Button labels in row-major order
        private static final String[] LABELS = {
            "7", "8", "9", "/",
            "4", "5", "6", "*",
            "1", "2", "3", "-",
            "C", "0", "=", "+"
        };

        private final Paint mPaint = new Paint();

        public CalcView(Context ctx) {
            super(ctx);
        }

        // ── Touch handling ───────────────────────────────────────────────
        public void onTap(int x, int y) {
            if (y < (int) DISP_H) return;   // tap in display area — ignore
            int col = x / (int) BTN_W;
            int row = (y - (int) DISP_H) / (int) BTN_H;
            if (col < 0 || col >= 4 || row < 0 || row >= 4) return;
            press(LABELS[row * 4 + col]);
            invalidate();
        }

        private void press(String btn) {
            if (btn.equals("C")) {
                mDisplay = "0";
                mLeft  = 0;
                mOp    = 0;
                mReset = true;
            } else if (btn.equals("=")) {
                if (mOp != 0) {
                    int right = Integer.parseInt(mDisplay);
                    int result = 0;
                    if      (mOp == 1) result = mLeft + right;
                    else if (mOp == 2) result = mLeft - right;
                    else if (mOp == 3) result = mLeft * right;
                    else if (mOp == 4) result = (right != 0) ? mLeft / right : 0;
                    mDisplay = "" + result;
                    mOp    = 0;
                    mReset = true;
                }
            } else if (btn.equals("+") || btn.equals("-") ||
                       btn.equals("*") || btn.equals("/")) {
                mLeft  = Integer.parseInt(mDisplay);
                mOp    = btn.equals("+") ? 1
                       : btn.equals("-") ? 2
                       : btn.equals("*") ? 3 : 4;
                mReset = true;
            } else {
                // Digit
                if (mReset) {
                    mDisplay = btn;
                    mReset   = false;
                } else if (mDisplay.length() < 10) {
                    // Avoid leading zero
                    if (mDisplay.equals("0")) mDisplay = btn;
                    else mDisplay = mDisplay + btn;
                }
            }
        }

        // ── Drawing ──────────────────────────────────────────────────────
        @Override
        protected void onDraw(Canvas canvas) {
            // Background
            canvas.drawColor(C_BG);

            // Display area background
            mPaint.setColor(C_DISP);
            canvas.drawRect(0.0f, 0.0f, 800.0f, DISP_H, mPaint);

            // Display text: right-justified estimate
            // Approximate width: each character ≈ 42px at text size 72
            mPaint.setColor(C_DISP_TXT);
            mPaint.setTextSize(72.0f);
            int len = mDisplay.length();
            float tw = len * 42.0f;
            float tx = 780.0f - tw;
            if (tx < 10.0f) tx = 10.0f;
            canvas.drawText(mDisplay, tx, 112.0f, mPaint);

            // Op indicator (small, top-left of display)
            if (mOp != 0) {
                mPaint.setColor(C_OP);
                mPaint.setTextSize(30.0f);
                String opStr = mOp == 1 ? "+" : mOp == 2 ? "-" : mOp == 3 ? "*" : "/";
                canvas.drawText(opStr, 20.0f, 40.0f, mPaint);
            }

            // Button grid
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 4; col++) {
                    String lbl = LABELS[row * 4 + col];

                    // Button top-left pixel (using float arithmetic)
                    float bx = col * BTN_W;
                    float by = DISP_H + row * BTN_H;

                    // Button color
                    int btnColor;
                    if (lbl.equals("C")) {
                        btnColor = C_CTRL;
                    } else if (lbl.equals("=")) {
                        btnColor = C_EQ;
                    } else if (lbl.equals("+") || lbl.equals("-") ||
                               lbl.equals("*") || lbl.equals("/")) {
                        btnColor = C_OP;
                    } else {
                        btnColor = C_NUM;
                    }
                    mPaint.setColor(btnColor);
                    canvas.drawRect(bx + GAP, by + GAP,
                                    bx + BTN_W - GAP, by + BTN_H - GAP,
                                    mPaint);

                    // Button label: centered (1-char labels, 38px text)
                    // Center: cx = bx + BTN_W/2, cy = by + BTN_H/2
                    // Text baseline: cy + textSize*0.35 ≈ cy + 13
                    mPaint.setColor(C_WHITE);
                    mPaint.setTextSize(38.0f);
                    float lx = bx + BTN_W / 2.0f - 11.0f;
                    float ly = by + BTN_H / 2.0f + 13.0f;
                    canvas.drawText(lbl, lx, ly, mPaint);
                }
            }
        }
    }
}

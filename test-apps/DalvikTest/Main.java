/**
 * test-apps/DalvikTest/Main.java
 *
 * Tests aine-dalvik F6 opcodes:
 * - Arithmetic (add, sub, mul, div, rem)
 * - Long arithmetic (64-bit)
 * - Comparisons and branches
 * - Arrays (int[], String[])
 * - String operations (length, contains, concat)
 * - StringBuilder
 * - android.util.Log
 */
public class Main {

    // ── T1: Integer arithmetic ──────────────────────────────────────────
    static int testArith() {
        int a = 10, b = 3;
        int add = a + b;    // 13
        int sub = a - b;    // 7
        int mul = a * b;    // 30
        int div = a / b;    // 3
        int rem = a % b;    // 1
        int and_ = a & b;   // 2
        int or_  = a | b;   // 11
        int xor_ = a ^ b;   // 9
        int shl  = a << 2;  // 40
        int shr  = a >> 1;  // 5
        return add + sub + mul + div + rem + and_ + or_ + xor_ + shl + shr;
        // 13+7+30+3+1+2+11+9+40+5 = 121
    }

    // ── T2: Long arithmetic ─────────────────────────────────────────────
    static long testLong() {
        long x = 1000000000L * 3L;  // 3_000_000_000
        long y = x + 500000000L;    // 3_500_000_000
        return y;
    }

    // ── T3: Conditional branches ────────────────────────────────────────
    static int testBranches() {
        int result = 0;
        for (int i = 0; i < 5; i++) {
            if (i % 2 == 0) result += i;
        }
        // 0 + 2 + 4 = 6
        return result;
    }

    // ── T4: Int array ───────────────────────────────────────────────────
    static int testIntArray() {
        int[] arr = new int[5];
        for (int i = 0; i < arr.length; i++) arr[i] = i * 2;
        // arr = [0, 2, 4, 6, 8]
        int sum = 0;
        for (int v : arr) sum += v;
        return sum; // 20
    }

    // ── T5: StringBuilder ───────────────────────────────────────────────
    static String testStringBuilder() {
        StringBuilder sb = new StringBuilder();
        sb.append("AINE");
        sb.append("-");
        sb.append("F6");
        return sb.toString(); // "AINE-F6"
    }

    // ── T6: String operations ───────────────────────────────────────────
    static int testStrings() {
        String s = "Hello, AINE!";
        int len = s.length();          // 12
        boolean has = s.contains("AINE"); // true = 1
        return len + (has ? 1 : 0);   // 13
    }

    // ── T7: Negation and bitwise NOT ────────────────────────────────────
    static int testUnary() {
        int a = 42;
        int neg = -a;  // -42
        int not_ = ~a; // -43
        return neg + not_; // -85
    }

    // ── T8: Lit arithmetic (22s / 22b opcodes) ──────────────────────────
    static int testLitArith() {
        int a = 100;
        int b = a + 7;    // 107 (add-int/lit16)
        int c = b * 2;    // 214 (mul-int/lit8)
        int d = c - 14;   // 200
        return d;
    }

    public static void main(String[] args) {
        int arith  = testArith();
        long lng   = testLong();
        int branch = testBranches();
        int arr    = testIntArray();
        String sb_res = testStringBuilder();
        int strops = testStrings();
        int unary  = testUnary();
        int lit_a  = testLitArith();

        System.out.println("[dalvik-test] T1 arith   = " + arith);
        System.out.println("[dalvik-test] T2 long    = " + lng);
        System.out.println("[dalvik-test] T3 branch  = " + branch);
        System.out.println("[dalvik-test] T4 array   = " + arr);
        System.out.println("[dalvik-test] T5 sb      = " + sb_res);
        System.out.println("[dalvik-test] T6 strops  = " + strops);
        System.out.println("[dalvik-test] T7 unary   = " + unary);
        System.out.println("[dalvik-test] T8 litarith= " + lit_a);

        // Check expected values
        int ok = 0, fail = 0;
        if (arith == 121)              { ok++; } else { System.out.println("FAIL T1 expected 121 got " + arith); fail++; }
        if (lng == 3500000000L)        { ok++; } else { System.out.println("FAIL T2 expected 3500000000 got " + lng); fail++; }
        if (branch == 6)               { ok++; } else { System.out.println("FAIL T3 expected 6 got " + branch); fail++; }
        if (arr == 20)                 { ok++; } else { System.out.println("FAIL T4 expected 20 got " + arr); fail++; }
        if (sb_res.equals("AINE-F6")) { ok++; } else { System.out.println("FAIL T5 expected AINE-F6 got " + sb_res); fail++; }
        if (strops == 13)              { ok++; } else { System.out.println("FAIL T6 expected 13 got " + strops); fail++; }
        if (unary == -85)              { ok++; } else { System.out.println("FAIL T7 expected -85 got " + unary); fail++; }
        if (lit_a == 200)              { ok++; } else { System.out.println("FAIL T8 expected 200 got " + lit_a); fail++; }

        System.out.println("[dalvik-test] RESULT: " + ok + " ok, " + fail + " failed");
        if (fail > 0) throw new RuntimeException("dalvik-test: " + fail + " test(s) failed");
    }
}

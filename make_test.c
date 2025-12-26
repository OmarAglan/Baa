#include <stdio.h>

void w(FILE* f, const unsigned char* b, size_t s) { fwrite(b, 1, s, f); }

int main() {
    FILE* f = fopen("test.b", "wb");
    if (!f) return 1;

    // --- الكلمات المفتاحية والرموز ---
    unsigned char kw_int[] = {0xD8, 0xB5, 0xD8, 0xAD, 0xD9, 0x8A, 0xD8, 0xAD, 0x20}; // صحيح
    unsigned char kw_ret[] = {0xD8, 0xA5, 0xD8, 0xB1, 0xD8, 0xAC, 0xD8, 0xB9, 0x20}; // إرجع
    unsigned char kw_print[] = {0xD8, 0xA7, 0xD8, 0xB7, 0xD8, 0xA8, 0xD8, 0xB9, 0x20}; // اطبع
    unsigned char kw_for[] = {0xD9, 0x84, 0xD9, 0x83, 0xD9, 0x84, 0x20}; // لكل
    
    unsigned char name_main[] = {0xD8, 0xA7, 0xD9, 0x84, 0xD8, 0xB1, 0xD8, 0xA6, 0xD9, 0x8A, 0xD8, 0xB3, 0xD9, 0x8A, 0xD8, 0xA9}; // الرئيسية
    
    // المتغيرات
    unsigned char var_list[] = {0xD9, 0x82, 0xD8, 0xA7, 0xD8, 0xA6, 0xD9, 0x85, 0xD8, 0xA9}; // قائمة (array)
    unsigned char var_s[] = {0xD8, 0xB3, 0x20}; // س (counter)
    unsigned char var_sum[] = {0xD9, 0x85, 0xD8, 0xAC, 0xD9, 0x85, 0xD9, 0x88, 0xD8, 0xB9, 0x20}; // مجموع (sum)

    // الرموز
    unsigned char eq[] = {0x3D, 0x20}; // =
    unsigned char lt[] = {0x3C, 0x20}; // <
    unsigned char plus[] = {0x2B, 0x20}; // +
    unsigned char mul[] = {0x2A, 0x20}; // *
    unsigned char inc[] = {0x2B, 0x2B, 0x20}; // ++
    
    unsigned char dot[] = {0x2E, 0x0A}; // . \n
    unsigned char semi[] = {0xD8, 0x9B, 0x20}; // ؛ (فاصلة منقوطة عربية)
    
    unsigned char lp[] = {0x28, 0x20}; // (
    unsigned char rp[] = {0x29, 0x20}; // )
    unsigned char lb[] = {0x7B, 0x0A}; // { \n
    unsigned char rb[] = {0x7D, 0x0A}; // } \n
    unsigned char lbr[] = {0x5B}; // [
    unsigned char rbr[] = {0x5D, 0x20}; // ] 

    // الأرقام
    unsigned char n0[] = {0xD9, 0xA0}; // ٠
    unsigned char n5[] = {0xD9, 0xA5}; // ٥
    unsigned char n10[] = {0xD9, 0xA1, 0xD9, 0xA0}; // ١٠

    // --- بناء الكود ---

    // 1. تعريف الدالة الرئيسية: صحيح الرئيسية() {
    w(f, kw_int, sizeof(kw_int)); w(f, name_main, sizeof(name_main)); 
    w(f, lp, sizeof(lp)); w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

    // 2. تعريف المصفوفة: صحيح قائمة[٥].
    w(f, kw_int, sizeof(kw_int)); w(f, var_list, sizeof(var_list));
    w(f, lbr, sizeof(lbr)); w(f, n5, sizeof(n5)); w(f, rbr, sizeof(rbr));
    w(f, dot, sizeof(dot));

    // 3. الحلقة الأولى (الملء): لكل (صحيح س = ٠؛ س < ٥؛ س++) {
    w(f, kw_for, sizeof(kw_for)); w(f, lp, sizeof(lp));
    // التهيئة: صحيح س = ٠
    w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq)); w(f, n0, sizeof(n0)); w(f, semi, sizeof(semi));
    // الشرط: س < ٥
    w(f, var_s, sizeof(var_s)); w(f, lt, sizeof(lt)); w(f, n5, sizeof(n5)); w(f, semi, sizeof(semi));
    // الزيادة: س++
    w(f, var_s, sizeof(var_s)); w(f, inc, sizeof(inc));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // قائمة[س] = س * ١٠.
        w(f, var_list, sizeof(var_list)); w(f, lbr, sizeof(lbr)); w(f, var_s, sizeof(var_s)); w(f, rbr, sizeof(rbr));
        w(f, eq, sizeof(eq));
        w(f, var_s, sizeof(var_s)); w(f, mul, sizeof(mul)); w(f, n10, sizeof(n10));
        w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    // 4. متغير المجموع: صحيح مجموع = ٠.
    w(f, kw_int, sizeof(kw_int)); w(f, var_sum, sizeof(var_sum)); w(f, eq, sizeof(eq)); w(f, n0, sizeof(n0));
    w(f, dot, sizeof(dot));

    // 5. الحلقة الثانية (الجمع): لكل (صحيح س = ٠؛ س < ٥؛ س++) {
    // نعيد استخدام المتغير س (Shadowing or re-declaration works in local scope blocks)
    w(f, kw_for, sizeof(kw_for)); w(f, lp, sizeof(lp));
    w(f, kw_int, sizeof(kw_int)); w(f, var_s, sizeof(var_s)); w(f, eq, sizeof(eq)); w(f, n0, sizeof(n0)); w(f, semi, sizeof(semi));
    w(f, var_s, sizeof(var_s)); w(f, lt, sizeof(lt)); w(f, n5, sizeof(n5)); w(f, semi, sizeof(semi));
    w(f, var_s, sizeof(var_s)); w(f, inc, sizeof(inc));
    w(f, rp, sizeof(rp)); w(f, lb, sizeof(lb));

        // مجموع = مجموع + قائمة[س].
        w(f, var_sum, sizeof(var_sum)); w(f, eq, sizeof(eq));
        w(f, var_sum, sizeof(var_sum)); w(f, plus, sizeof(plus));
        w(f, var_list, sizeof(var_list)); w(f, lbr, sizeof(lbr)); w(f, var_s, sizeof(var_s)); w(f, rbr, sizeof(rbr));
        w(f, dot, sizeof(dot));

    w(f, rb, sizeof(rb)); // }

    // 6. الطباعة: اطبع مجموع.
    w(f, kw_print, sizeof(kw_print)); w(f, var_sum, sizeof(var_sum)); w(f, dot, sizeof(dot));

    // 7. الخروج: إرجع ٠.
    w(f, kw_ret, sizeof(kw_ret)); w(f, n0, sizeof(n0)); w(f, dot, sizeof(dot));
    
    w(f, rb, sizeof(rb)); // } Main

    fclose(f);
    printf("Generated test.b with Arrays and For Loops.\n");
    return 0;
}
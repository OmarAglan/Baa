/**
 * @brief هل النوع قيمة مركبة لا تدعم النسخ المباشر حالياً؟
 */
static bool is_aggregate_assignment_type(DataType t)
{
    return t == TYPE_STRUCT || t == TYPE_UNION;
}

/**
 * @brief تحليل يمين إسناد مركب دون تكرار خطأ "استخدام المركب كقيمة" للحالة البسيطة.
 */
static void analyze_aggregate_assignment_rhs(Node* value)
{
    if (!value) return;
    if (value->type == NODE_VAR_REF) {
        Symbol* rhs = lookup(value->data.var_ref.name, true);
        if (rhs && is_aggregate_assignment_type(rhs->type)) return;
    }
    (void)infer_type(value);
}

/**
 * @brief إصدار تشخيص سياسة إسناد القيم المركبة.
 */
static void report_aggregate_assignment_policy(Node* node, const char* target_kind, const char* name)
{
    semantic_error(node,
                   "إسناد القيم المركبة بالنسخ غير مدعوم حالياً (%s '%s')؛ عدّل الحقول عبر ':'.",
                   target_kind ? target_kind : "الهدف",
                   name ? name : "???");
}

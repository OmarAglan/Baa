// ============================================================================
// تهيئة حقول الهيكل (Struct Field Initializer Lists)
// ============================================================================

/**
 * @brief تحليل قائمة تهيئة حقول هيكل بالشكل: = { حقل: تعبير, ... }.
 *
 * الصيغة مقصودة للتهيئة المسماة فقط، لذلك لا توجد تهيئة موضعية ضمن هذه المرحلة.
 *
 * @param out_count عدد عناصر التهيئة الفعلية
 * @param out_has_init هل تم العثور على '=' (تهيئة صريحة)
 * @return رأس قائمة NODE_STRUCT_FIELD_INIT أو NULL عند عدم وجود تهيئة/قائمة فارغة
 */
static Node* parse_struct_initializer_list(int* out_count, bool* out_has_init)
{
    if (out_count) *out_count = 0;
    if (out_has_init) *out_has_init = false;

    if (parser.current.type != TOKEN_ASSIGN) {
        return NULL;
    }

    if (out_has_init) *out_has_init = true;
    eat(TOKEN_ASSIGN);

    if (parser.current.type != TOKEN_LBRACE) {
        error_report(parser.current, "متوقع '{' بعد '=' في تهيئة الهيكل.");
        return NULL;
    }

    eat(TOKEN_LBRACE);

    Node* head = NULL;
    Node* tail = NULL;
    int count = 0;

    if (parser.current.type != TOKEN_RBRACE) {
        while (1) {
            if (parser.current.type != TOKEN_IDENTIFIER) {
                error_report(parser.current, "متوقع اسم حقل داخل تهيئة الهيكل.");
                synchronize_mode(PARSER_SYNC_STATEMENT);
                break;
            }

            Token tok_field = parser.current;
            char* field_name = strdup(parser.current.value);
            eat(TOKEN_IDENTIFIER);

            if (parser.current.type != TOKEN_COLON) {
                error_report(parser.current, "متوقع ':' بعد اسم الحقل في تهيئة الهيكل.");
                free(field_name);
                synchronize_mode(PARSER_SYNC_STATEMENT);
                break;
            }
            eat(TOKEN_COLON);

            Node* value = parse_expression();
            if (!value) {
                free(field_name);
                break;
            }

            Node* item = ast_node_new(NODE_STRUCT_FIELD_INIT, tok_field);
            if (!item) {
                free(field_name);
                break;
            }
            item->data.struct_field_init.field_name = field_name;
            item->data.struct_field_init.value = value;
            parser_list_append(&head, &tail, item);
            count++;

            if (parser.current.type == TOKEN_COMMA) {
                eat(TOKEN_COMMA);
                // السماح بفاصلة أخيرة قبل '}' كما في قوائم التهيئة الأخرى.
                if (parser.current.type == TOKEN_RBRACE) break;
                continue;
            }
            break;
        }
    }

    eat(TOKEN_RBRACE);

    if (out_count) *out_count = count;
    return head;
}

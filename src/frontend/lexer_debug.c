const char* token_type_to_str(BaaTokenType type) {
    switch (type) {
        case TOKEN_EOF: return "نهاية_الملف";
        case TOKEN_INT: return "عدد_صحيح";
        case TOKEN_FLOAT: return "عدد_عشري";
        case TOKEN_STRING: return "نص";
        case TOKEN_CHAR: return "حرف";
        case TOKEN_IDENTIFIER: return "معرّف";
        
        // كلمات مفتاحية
        case TOKEN_KEYWORD_INT: return "صحيح";
        case TOKEN_KEYWORD_I8: return "ص٨";
        case TOKEN_KEYWORD_I16: return "ص١٦";
        case TOKEN_KEYWORD_I32: return "ص٣٢";
        case TOKEN_KEYWORD_I64: return "ص٦٤";
        case TOKEN_KEYWORD_U8: return "ط٨";
        case TOKEN_KEYWORD_U16: return "ط١٦";
        case TOKEN_KEYWORD_U32: return "ط٣٢";
        case TOKEN_KEYWORD_U64: return "ط٦٤";
        case TOKEN_KEYWORD_STRING: return "نص";
        case TOKEN_KEYWORD_BOOL: return "منطقي";
        case TOKEN_KEYWORD_CHAR: return "حرف";
        case TOKEN_KEYWORD_FLOAT: return "عشري";
        case TOKEN_KEYWORD_FLOAT32: return "عشري٣٢";
        case TOKEN_KEYWORD_VOID: return "عدم";
        case TOKEN_CAST: return "كـ";
        case TOKEN_SIZEOF: return "حجم";
        case TOKEN_TYPE_ALIAS: return "نوع";
        case TOKEN_CONST: return "ثابت";
        case TOKEN_STATIC: return "ساكن";
        case TOKEN_RETURN: return "إرجع";
        case TOKEN_PRINT: return "اطبع";
        case TOKEN_READ: return "اقرأ";
        case TOKEN_ASM: return "مجمع";
        case TOKEN_IF: return "إذا";
        case TOKEN_ELSE: return "وإلا";
        case TOKEN_WHILE: return "طالما";
        case TOKEN_FOR: return "لكل";
        case TOKEN_BREAK: return "توقف";
        case TOKEN_CONTINUE: return "استمر";
        case TOKEN_SWITCH: return "اختر";
        case TOKEN_CASE: return "حالة";
        case TOKEN_DEFAULT: return "افتراضي";
        case TOKEN_TRUE: return "صواب";
        case TOKEN_FALSE: return "خطأ";
        case TOKEN_ENUM: return "تعداد";
        case TOKEN_STRUCT: return "هيكل";
        case TOKEN_UNION: return "اتحاد";
        
        // رموز
        case TOKEN_ASSIGN: return "=";
        case TOKEN_DOT: return ".";
        case TOKEN_ELLIPSIS: return "...";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return "؛";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_INC: return "++";
        case TOKEN_DEC: return "--";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LTE: return "<=";
        case TOKEN_GTE: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_AMP: return "&";
        case TOKEN_PIPE: return "|";
        case TOKEN_CARET: return "^";
        case TOKEN_TILDE: return "~";
        case TOKEN_SHL: return "<<";
        case TOKEN_SHR: return ">>";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        
        default: return "UNKNOWN";
    }
}

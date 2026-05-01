Token lexer_next_token(Lexer* l) {
    Token token = {0};
    
    // 0. حلقة لتجاوز المسافات ومعالجة التضمين
    while (1) {
        // تخطي المسافات البيضاء والأسطر الجديدة
        while (peek(l) != '\0' && isspace(peek(l))) {
            advance_pos(l);
        }

        // تخطي التعليقات //
        if (peek(l) == '/' && peek_next(l) == '/') {
            while (peek(l) != '\0' && peek(l) != '\n') {
                l->state.cur_char++; // تخطي سريع
            }
            continue; // إعادة المحاولة (لمعالجة السطر الجديد)
        }

        // ====================================================================
        // معالجة الموجهات (Preprocessor Directives)
        // ====================================================================
        if (peek(l) == '#') {
            advance_pos(l); // consume '#'
            
            // قراءة الموجه
            if (is_arabic_start_byte(peek(l))) {
                 char* dir_start = l->state.cur_char;
                 while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                 
                 size_t len = l->state.cur_char - dir_start;
                 char* directive = malloc(len + 1);
                 if (!directive) {
                     lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة التوجيه.");
                 }
                 if (len) memcpy(directive, dir_start, len);
                 directive[len] = '\0';
                 
                 // 1. #تضمين (Include)
                 if (strcmp(directive, "تضمين") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     if (peek(l) != '"') {
                         lex_fatal(l, "خطأ قبلي: متوقع اسم ملف نصي بعد #تضمين.");
                     }
                     advance_pos(l); // skip "
                     char* path_start = l->state.cur_char;
                     while (peek(l) != '"' && peek(l) != '\0') advance_pos(l);
                     size_t path_len = l->state.cur_char - path_start;
                     char* path = malloc(path_len + 1);
                     if (!path) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة مسار #تضمين.");
                     }
                     if (path_len) memcpy(path, path_start, path_len);
                     path[path_len] = '\0';
                     advance_pos(l); // skip "

                     char* resolved_include_path = NULL;
                     char* new_src = lex_resolve_and_read_include(l, path, &resolved_include_path);
                     if (!new_src || !resolved_include_path) {
                         lex_fatal(l,
                                   "خطأ قبلي: تعذر تضمين الملف '%s'. "
                                   "المسارات المدعومة: مسار الملف الحالي، المسار المباشر، "
                                   "BAA_HOME، مسارات -I، stdlib/، BAA_STDLIB.",
                                   path);
                     }

                     if (lex_include_would_cycle(l, resolved_include_path)) {
                         free(new_src);
                         lex_fatal_include_cycle(l, path, resolved_include_path);
                     }
                      
                     size_t max_include_depth = sizeof(l->stack) / sizeof(l->stack[0]);
                     if ((size_t)l->stack_depth >= max_include_depth) {
                         free(new_src);
                         free(resolved_include_path);
                         lex_fatal(l, "خطأ قبلي: تم تجاوز عمق التضمين الأقصى (%zu).", max_include_depth);
                     }
                     l->stack[l->stack_depth++] = l->state;
                      
                     l->state.source = new_src;
                     l->state.cur_char = new_src;
                     l->state.filename = resolved_include_path;
                     l->state.line = 1;
                     l->state.col = 1;
                     lex_record_dependency(l, resolved_include_path);
                     lex_skip_utf8_bom(&l->state);
                      error_register_source(l->state.filename, new_src);
                     
                     free(directive);
                     free(path);
                     continue;
                 }
                 // 2. #تعريف (Define)
                 else if (strcmp(directive, "تعريف") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم ماكرو.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     // قراءة القيمة (باقي السطر)
                     while (peek(l) != '\0' && isspace(peek(l)) && peek(l) != '\n') advance_pos(l);
                     char* val_start = l->state.cur_char;
                     while (peek(l) != '\n' && peek(l) != '\0' && peek(l) != '\r') advance_pos(l); // لنهاية السطر
                     size_t val_len = l->state.cur_char - val_start;
                     char* val = malloc(val_len + 1);
                     if (!val) {
                         free(name);
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة قيمة ماكرو.");
                     }
                     if (val_len) memcpy(val, val_start, val_len);
                     val[val_len] = '\0';

                     // تنظيف القيمة من المسافات الزائدة في النهاية
                     while(val_len > 0 && isspace(val[val_len-1])) val[--val_len] = '\0';

                     add_macro(l, name, val);
                     free(name);
                     free(val);
                     free(directive);
                     continue;
                 }
                 // 3. #إذا_عرف (If Defined)
                 else if (strcmp(directive, "إذا_عرف") == 0) {
                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم #إذا_عرف.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     bool defined = (get_macro_value(l, name) != NULL);

                     if (l->if_depth >= (int)(sizeof(l->if_stack) / sizeof(l->if_stack[0]))) {
                         free(name);
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: تجاوزت الشروط المتداخلة الحد الأقصى.");
                     }

                     bool parent = pp_active(l);
                     l->if_stack[l->if_depth].parent_active = parent ? 1 : 0;
                     l->if_stack[l->if_depth].cond_true = (parent && defined) ? 1 : 0;
                     l->if_stack[l->if_depth].in_else = 0;
                     l->if_depth++;
                     pp_recompute_skipping(l);
                     
                     free(name);
                     free(directive);
                     continue;
                 }
                 // 4. #وإلا (Else)
                 else if (strcmp(directive, "وإلا") == 0) {
                     if (l->if_depth <= 0) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: #وإلا بدون #إذا_عرف مطابق.");
                     }
                     int i = l->if_depth - 1;
                     if (l->if_stack[i].in_else) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: تكرار #وإلا داخل نفس الكتلة.");
                     }
                     l->if_stack[i].in_else = 1;
                     pp_recompute_skipping(l);
                     free(directive);
                     continue;
                 }
                 // 5. #نهاية (End)
                 else if (strcmp(directive, "نهاية") == 0) {
                     if (l->if_depth <= 0) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: #نهاية بدون #إذا_عرف مطابق.");
                     }
                     l->if_depth--;
                     pp_recompute_skipping(l);
                     free(directive);
                     continue;
                 }
                 // 6. #الغاء_تعريف (Undefine)
                 else if (strcmp(directive, "الغاء_تعريف") == 0) {
                     if (l->skipping) { free(directive); continue; }

                     // قراءة الاسم
                     while (peek(l) != '\0' && isspace(peek(l))) advance_pos(l);
                     char* name_start = l->state.cur_char;
                     while (!isspace(peek(l)) && peek(l) != '\0') advance_pos(l);
                     size_t name_len = l->state.cur_char - name_start;
                     char* name = malloc(name_len + 1);
                     if (!name) {
                         free(directive);
                         lex_fatal(l, "خطأ قبلي: نفدت الذاكرة أثناء قراءة اسم #الغاء_تعريف.");
                     }
                     if (name_len) memcpy(name, name_start, name_len);
                     name[name_len] = '\0';

                     remove_macro(l, name);
                     free(name);
                     free(directive);
                     continue;
                 }
                 
                 free(directive);
             }
             lex_fatal(l, "خطأ قبلي: توجيه غير معروف.");
        }

        // نهاية الملف EOF
        if (peek(l) == '\0') {
            if (l->if_depth > 0 && l->stack_depth == 0) {
                lex_fatal(l, "خطأ قبلي: نهاية الملف قبل إغلاق #إذا_عرف (مفقود #نهاية).");
            }

            // إذا كنا داخل ملف مضمن، نعود للملف السابق (Pop)
            if (l->stack_depth > 0) {
                free(l->state.source);
                free((void*)l->state.filename);
                l->state = l->stack[--l->stack_depth];
                continue; 
            }
            token.type = TOKEN_EOF;
            token.filename = l->state.filename;
            token.line = l->state.line;
            token.col = l->state.col;
            return lex_finish_token(l, token);
        }

        // إذا كنا في وضع التخطي، نتجاهل كل شيء حتى نجد #
        if (l->skipping) {
            advance_pos(l);
            continue;
        }

        break; // وجدنا بداية رمز صالح ونحن لسن في وضع التخطي
    }

    // تسجيل الموقع
    token.line = l->state.line;
    token.col = l->state.col;
    token.filename = l->state.filename;

    char* current = l->state.cur_char;

    // --- معالجة النصوص ("...") ---
    if (*current == '"') {
        advance_pos(l); // تخطي " البداية

        size_t cap = 32;
        size_t len = 0;
        char* str = (char*)malloc(cap);
        if (!str) {
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء قراءة النص.");
        }

        while (peek(l) != '"' && peek(l) != '\0') {
            if (peek(l) == '\\') {
                advance_pos(l); // تخطي '\'
                if (peek(l) == '\0') {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل هروب غير مكتمل داخل النص.");
                }

                unsigned char out = 0;
                if (peek(l) == '\\') { out = (unsigned char)'\\'; advance_pos(l); }
                else if (peek(l) == '"') { out = (unsigned char)'"'; advance_pos(l); }
                else if (peek(l) == '\'') { out = (unsigned char)'\''; advance_pos(l); }
                else if (lex_decode_arabic_escape(l, &out)) { /* done */ }
                else {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل هروب غير مدعوم داخل النص.");
                }

                if (!lex_append_byte(&str, &len, &cap, out)) {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
                }
                continue;
            }

            unsigned char b0 = (unsigned char)peek(l);
            if (b0 >= 0x80u) {
                int ulen = 0;
                if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                    free(str);
                    lex_fatal(l, "خطأ لفظي: تسلسل UTF-8 غير صالح داخل النص.");
                }
                for (int i = 0; i < ulen; i++) {
                    if (!lex_append_byte(&str, &len, &cap, (unsigned char)*l->state.cur_char)) {
                        free(str);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
                    }
                    advance_pos(l);
                }
                continue;
            }

            if (!lex_append_byte(&str, &len, &cap, b0)) {
                free(str);
                lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء بناء النص.");
            }
            advance_pos(l);
        }

        if (peek(l) == '\0') {
            free(str);
            lex_fatal(l, "خطأ لفظي: النص غير مُغلق.");
        }

        if (!lex_append_byte(&str, &len, &cap, 0)) {
            free(str);
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء إنهاء النص.");
        }

        advance_pos(l); // تخطي " النهاية
        token.type = TOKEN_STRING;
        token.value = str;
        return lex_finish_token(l, token);
    }

    // --- معالجة الحروف ('...') ---
    if (*current == '\'') {
        advance_pos(l);

        unsigned char bytes[4];
        int blen = 0;

        if (peek(l) == '\\')
        {
            // تسلسلات الهروب (عربية + بنيوية)
            advance_pos(l);
            unsigned char esc0 = (unsigned char)peek(l);
            if (esc0 == '\0') {
                lex_fatal(l, "خطأ لفظي: الحرف الحرفي غير مُغلق.");
            }

            if (esc0 == '\\') { bytes[0] = '\\'; blen = 1; advance_pos(l); }
            else if (esc0 == '\'') { bytes[0] = '\''; blen = 1; advance_pos(l); }
            else if (esc0 == '"') { bytes[0] = '"'; blen = 1; advance_pos(l); }
            else if (lex_decode_arabic_escape(l, &bytes[0])) { blen = 1; }
            else
            {
                lex_fatal(l, "خطأ لفظي: تسلسل هروب غير معروف داخل الحرف الحرفي.");
            }
        }
        else
        {
            int ulen = 0;
            if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                lex_fatal(l, "خطأ لفظي: UTF-8 غير صالح داخل الحرف الحرفي.");
            }
            for (int i = 0; i < ulen; i++) {
                bytes[i] = (unsigned char)*l->state.cur_char;
                advance_pos(l);
            }
            blen = ulen;
        }

        if (peek(l) != '\'')
        {
            lex_fatal(l, "خطأ لفظي: متوقع إغلاق الحرف الحرفي بعلامة ' .");
        }
        advance_pos(l);

        token.type = TOKEN_CHAR;
        char* val = malloc((size_t)blen + 1);
        if (!val) { lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء إنشاء الحرف الحرفي."); }
        if (blen) memcpy(val, bytes, (size_t)blen);
        val[blen] = '\0';
        token.value = val;
        return lex_finish_token(l, token);
    }

    // معالجة الفاصلة المنقوطة العربية (؛)
    if ((unsigned char)*current == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
        token.type = TOKEN_SEMICOLON;
        l->state.cur_char += 2; l->state.col += 2; 
        return lex_finish_token(l, token);
    }

    // معالجة الفاصلة العربية (،)
    if ((unsigned char)*current == 0xD8 && (unsigned char)*(l->state.cur_char + 1) == 0x8C) {
        token.type = TOKEN_COMMA;
        l->state.cur_char += 2;
        l->state.col += 2;
        return lex_finish_token(l, token);
    }

    // معالجة الرموز والعمليات
    if (*current == '.') {
        if (*(l->state.cur_char + 1) == '.' && *(l->state.cur_char + 2) == '.') {
            token.type = TOKEN_ELLIPSIS;
            l->state.cur_char += 3;
            l->state.col += 3;
            return lex_finish_token(l, token);
        }
        token.type = TOKEN_DOT;
        advance_pos(l);
        return lex_finish_token(l, token);
    }
    if (*current == ',') { token.type = TOKEN_COMMA; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == ':') { token.type = TOKEN_COLON; advance_pos(l); return lex_finish_token(l, token); }
    
    // الجمع والزيادة (++ و +)
    if (*current == '+') { 
        if (*(l->state.cur_char+1) == '+') {
            token.type = TOKEN_INC; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_PLUS; advance_pos(l); return lex_finish_token(l, token); 
    }
    
    // الطرح والنقصان (-- و -)
    if (*current == '-') { 
        if (*(l->state.cur_char+1) == '-') {
            token.type = TOKEN_DEC; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_MINUS; advance_pos(l); return lex_finish_token(l, token); 
    }

    if (*current == '*') { token.type = TOKEN_STAR; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '/') { token.type = TOKEN_SLASH; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '%') { token.type = TOKEN_PERCENT; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '(') { token.type = TOKEN_LPAREN; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == ')') { token.type = TOKEN_RPAREN; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '{') { token.type = TOKEN_LBRACE; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '}') { token.type = TOKEN_RBRACE; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == '[') { token.type = TOKEN_LBRACKET; advance_pos(l); return lex_finish_token(l, token); }
    if (*current == ']') { token.type = TOKEN_RBRACKET; advance_pos(l); return lex_finish_token(l, token); }

    // معالجة العمليات المنطقية/البتية (&&، ||، !، &، |، ^، ~)
    if (*current == '&') {
        if (*(l->state.cur_char+1) == '&') {
            token.type = TOKEN_AND; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_AMP; advance_pos(l); return lex_finish_token(l, token);
    }
    if (*current == '|') {
        if (*(l->state.cur_char+1) == '|') {
            token.type = TOKEN_OR; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_PIPE; advance_pos(l); return lex_finish_token(l, token);
    }
    if (*current == '^') {
        token.type = TOKEN_CARET; advance_pos(l); return lex_finish_token(l, token);
    }
    if (*current == '~') {
        token.type = TOKEN_TILDE; advance_pos(l); return lex_finish_token(l, token);
    }
    if (*current == '!') {
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_NEQ; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_NOT; advance_pos(l); return lex_finish_token(l, token);
    }

    // معالجة عمليات المقارنة والتعيين (=، ==، <، <=، >، >=)
    if (*current == '=') { 
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_EQ; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_ASSIGN; advance_pos(l); return lex_finish_token(l, token); 
    }
    if (*current == '<') {
        if (*(l->state.cur_char+1) == '<') {
            token.type = TOKEN_SHL; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_LTE; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_LT; advance_pos(l); return lex_finish_token(l, token);
    }
    if (*current == '>') {
        if (*(l->state.cur_char+1) == '>') {
            token.type = TOKEN_SHR; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        if (*(l->state.cur_char+1) == '=') {
            token.type = TOKEN_GTE; l->state.cur_char += 2; l->state.col += 2; return lex_finish_token(l, token);
        }
        token.type = TOKEN_GT; advance_pos(l); return lex_finish_token(l, token);
    }

    // معالجة الأرقام
    // ملاحظة: is_arabic_digit تتوقع مؤشر char*، لذا نمرر current.
    // ملاحظة: current هنا مؤشر آمن ضمن المصدر الحالي.
    
    if (isdigit((unsigned char)*current) || is_arabic_digit(current)) {
        token.type = TOKEN_INT;
        char buffer[64] = {0}; 
        int buf_idx = 0;
        
        while (1) {
            char* c = l->state.cur_char;
            if (isdigit((unsigned char)*c)) { 
                if (buf_idx >= (int)sizeof(buffer) - 1) {
                    lex_fatal(l, "خطأ لفظي: العدد الصحيح أطول من الحد المسموح.");
                }
                buffer[buf_idx++] = *c;
                advance_pos(l); 
            } 
            else if (is_arabic_digit(c)) {
                if (buf_idx >= (int)sizeof(buffer) - 1) {
                    lex_fatal(l, "خطأ لفظي: العدد الصحيح أطول من الحد المسموح.");
                }
                buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                l->state.cur_char += 2; l->state.col += 2;
            } else { break; }
        }

        // رقم عشري: جزء كسري بعد '.' متبوعاً برقم
        if (peek(l) == '.' && (isdigit((unsigned char)peek_next(l)) || is_arabic_digit(l->state.cur_char + 1)))
        {
            if (buf_idx >= (int)sizeof(buffer) - 1) {
                lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
            }
            buffer[buf_idx++] = '.';
            advance_pos(l); // تخطي '.'

            while (1) {
                char* c = l->state.cur_char;
                if (isdigit((unsigned char)*c)) {
                    if (buf_idx >= (int)sizeof(buffer) - 1) {
                        lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
                    }
                    buffer[buf_idx++] = *c;
                    advance_pos(l);
                }
                else if (is_arabic_digit(c)) {
                    if (buf_idx >= (int)sizeof(buffer) - 1) {
                        lex_fatal(l, "خطأ لفظي: العدد العشري أطول من الحد المسموح.");
                    }
                    buffer[buf_idx++] = ((unsigned char)c[1] - 0xA0) + '0';
                    l->state.cur_char += 2; l->state.col += 2;
                } else { break; }
            }

            token.type = TOKEN_FLOAT;
            token.value = strdup(buffer);
            return lex_finish_token(l, token);
        }

        token.value = strdup(buffer);
        return lex_finish_token(l, token);
    }

    // معالجة المعرفات والكلمات المفتاحية
    if (is_arabic_start_byte(*current)) {
        char* start = l->state.cur_char;
        while (!isspace(peek(l)) && peek(l) != '\0' && 
               strchr(".+-,=:(){}[]!<>*/%&|^~\"'", peek(l)) == NULL) {
            // تحقق من الفاصلة المنقوطة العربية (؛)
            if ((unsigned char)*l->state.cur_char == 0xD8 && (unsigned char)*(l->state.cur_char+1) == 0x9B) {
                break;
            }
            // تحقق من الفاصلة العربية (،)
            if ((unsigned char)*l->state.cur_char == 0xD8 && (unsigned char)*(l->state.cur_char + 1) == 0x8C) {
                break;
            }
            unsigned char b0 = (unsigned char)*l->state.cur_char;
            if (b0 >= 0x80u) {
                int ulen = 0;
                if (!lex_utf8_validate_at(l->state.cur_char, &ulen)) {
                    lex_fatal(l, "خطأ لفظي: UTF-8 غير صالح داخل المعرّف.");
                }
                for (int i = 0; i < ulen; i++) {
                    advance_pos(l);
                }
            } else {
                advance_pos(l);
            }
        }
        
        size_t len = l->state.cur_char - start;
        char* word = malloc(len + 1);
        if (!word) {
            lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ المعرّف.");
        }
        if (len) memcpy(word, start, len);
        word[len] = '\0';

        // 1. التحقق من استبدال الماكرو
        char* macro_val = get_macro_value(l, word);
        if (macro_val != NULL) {
            // استبدال الرمز بقيمة الماكرو
            if (macro_val[0] == '"') {
                token.type = TOKEN_STRING;
                // إزالة علامات التنصيص
                size_t vlen = strlen(macro_val);
                if (vlen >= 2) {
                    char* val = malloc(vlen - 1);
                    if (!val) {
                        free(word);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                    }
                    if (vlen > 2) memcpy(val, macro_val + 1, vlen - 2);
                    val[vlen - 2] = '\0';
                    token.value = val;
                } else {
                    token.value = strdup("");
                    if (!token.value) {
                        free(word);
                        lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                    }
                }
            } 
            else if (isdigit((unsigned char)macro_val[0]) || is_arabic_digit(macro_val)) {
                token.type = TOKEN_INT;
                token.value = strdup(macro_val);
                if (!token.value) {
                    free(word);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                }
            }
            else {
                token.type = TOKEN_IDENTIFIER;
                token.value = strdup(macro_val);
                if (!token.value) {
                    free(word);
                    lex_fatal(l, "خطأ لفظي: نفدت الذاكرة أثناء نسخ قيمة الماكرو.");
                }
            }
            free(word);
            return lex_finish_token(l, token);
        }

        // 2. الكلمات المفتاحية المحجوزة
        if (strcmp(word, "إرجع") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(word, "اطبع") == 0) token.type = TOKEN_PRINT;
        else if (strcmp(word, "اقرأ") == 0) token.type = TOKEN_READ;
        else if (strcmp(word, "مجمع") == 0) token.type = TOKEN_ASM;
        else if (strcmp(word, "صحيح") == 0) token.type = TOKEN_KEYWORD_INT;
        else if (strcmp(word, "ص٨") == 0) token.type = TOKEN_KEYWORD_I8;
        else if (strcmp(word, "ص١٦") == 0) token.type = TOKEN_KEYWORD_I16;
        else if (strcmp(word, "ص٣٢") == 0) token.type = TOKEN_KEYWORD_I32;
        else if (strcmp(word, "ص٦٤") == 0) token.type = TOKEN_KEYWORD_I64;
        else if (strcmp(word, "ط٨") == 0) token.type = TOKEN_KEYWORD_U8;
        else if (strcmp(word, "ط١٦") == 0) token.type = TOKEN_KEYWORD_U16;
        else if (strcmp(word, "ط٣٢") == 0) token.type = TOKEN_KEYWORD_U32;
        else if (strcmp(word, "ط٦٤") == 0) token.type = TOKEN_KEYWORD_U64;
        else if (strcmp(word, "نص") == 0) token.type = TOKEN_KEYWORD_STRING;
        else if (strcmp(word, "منطقي") == 0) token.type = TOKEN_KEYWORD_BOOL;
        else if (strcmp(word, "حرف") == 0) token.type = TOKEN_KEYWORD_CHAR;
        else if (strcmp(word, "عشري") == 0) token.type = TOKEN_KEYWORD_FLOAT;
        else if (strcmp(word, "عشري٣٢") == 0) token.type = TOKEN_KEYWORD_FLOAT32;
        else if (strcmp(word, "عدم") == 0) token.type = TOKEN_KEYWORD_VOID;
        else if (strcmp(word, "كـ") == 0) token.type = TOKEN_CAST;
        else if (strcmp(word, "حجم") == 0) token.type = TOKEN_SIZEOF;
        else if (strcmp(word, "ثابت") == 0) token.type = TOKEN_CONST;
        else if (strcmp(word, "ساكن") == 0) token.type = TOKEN_STATIC;
        else if (strcmp(word, "إذا") == 0) token.type = TOKEN_IF;
        else if (strcmp(word, "وإلا") == 0) token.type = TOKEN_ELSE;
        else if (strcmp(word, "طالما") == 0) token.type = TOKEN_WHILE;
        else if (strcmp(word, "لكل") == 0) token.type = TOKEN_FOR;
        else if (strcmp(word, "توقف") == 0) token.type = TOKEN_BREAK;
        else if (strcmp(word, "استمر") == 0) token.type = TOKEN_CONTINUE;
        else if (strcmp(word, "اختر") == 0) token.type = TOKEN_SWITCH;
        else if (strcmp(word, "حالة") == 0) token.type = TOKEN_CASE;
        else if (strcmp(word, "افتراضي") == 0) token.type = TOKEN_DEFAULT;
        else if (strcmp(word, "صواب") == 0) token.type = TOKEN_TRUE;
        else if (strcmp(word, "خطأ") == 0) token.type = TOKEN_FALSE;
        else if (strcmp(word, "تعداد") == 0) token.type = TOKEN_ENUM;
        else if (strcmp(word, "هيكل") == 0) token.type = TOKEN_STRUCT;
        else if (strcmp(word, "اتحاد") == 0) token.type = TOKEN_UNION;
        else {
            token.type = TOKEN_IDENTIFIER;
            token.value = word;
            return lex_finish_token(l, token);
        }
        free(word);
        return lex_finish_token(l, token);
    }

    // إذا وصلنا لهنا، فهذا يعني وجود محرف غير معروف
    token.type = TOKEN_INVALID;
    lex_fatal(l, "خطأ لفظي: بايت غير معروف 0x%02X.", (unsigned char)*current);
    return lex_finish_token(l, token);
}

/**
 * @brief تحويل نوع الوحدة إلى نص مقروء (لأغراض التنقيح ورسائل الخطأ).
 */

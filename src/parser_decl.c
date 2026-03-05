Node* parse_declaration() {
    ParserDeclQualifiers decl_q;
    parser_parse_decl_qualifiers(&decl_q);

    if (parser_current_is_type_alias_keyword() && parser.next.type == TOKEN_IDENTIFIER) {
        if (decl_q.is_const || decl_q.is_static) {
            error_report(parser.current, "تعريف اسم النوع البديل لا يقبل 'ثابت' أو 'ساكن'.");
            return parse_type_alias_declaration(false);
        }
        return parse_type_alias_declaration(true);
    }

    bool is_const = decl_q.is_const;
    bool is_static = decl_q.is_static;

    if (parser_current_starts_type()) {
        Token tok_type = parser.current;
        DataType dt = TYPE_INT;
        char* type_name = NULL;
        DataType ptr_base_type = TYPE_INT;
        char* ptr_base_type_name = NULL;
        int ptr_depth = 0;
        FuncPtrSig* type_func_sig = NULL;
        (void)tok_type;

        if (!parse_type_spec(&dt, &type_name,
                             &ptr_base_type, &ptr_base_type_name,
                             &ptr_depth,
                             &type_func_sig)) {
            error_report(parser.current, "متوقع نوع.");
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return NULL;
        }

        // تعريف تعداد/هيكل/اتحاد: تعداد <name> { ... }  |  هيكل <name> { ... } | اتحاد <name> { ... }
        if ((dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) && parser.current.type == TOKEN_LBRACE) {
            // تواقيع مؤشرات الدوال لا تنطبق على هذا المسار.
            parser_funcsig_free(type_func_sig);
            type_func_sig = NULL;

            if (is_const || is_static) {
                error_report(parser.current, "لا يمكن وسم تعريف نوع بـ 'ثابت' أو 'ساكن'.");
            }

            Token tok_name = tok_type;
            if (dt == TYPE_ENUM) {
                eat(TOKEN_LBRACE);

                Node* head = NULL;
                Node* tail = NULL;

                if (parser.current.type != TOKEN_RBRACE) {
                    while (1) {
                        if (parser.current.type != TOKEN_IDENTIFIER) {
                            error_report(parser.current, "متوقع اسم عنصر تعداد.");
                            break;
                        }
                        Token tok_mem = parser.current;
                        char* mem_name = strdup(parser.current.value);
                        eat(TOKEN_IDENTIFIER);

                        Node* mem = ast_node_new(NODE_ENUM_MEMBER, tok_mem);
                        if (!mem) return NULL;
                        mem->data.enum_member.name = mem_name;
                        mem->data.enum_member.value = 0;
                        mem->data.enum_member.has_value = false;

                        if (!head) { head = mem; tail = mem; }
                        else { tail->next = mem; tail = mem; }

                        if (parser.current.type == TOKEN_COMMA) {
                            eat(TOKEN_COMMA);
                            if (parser.current.type == TOKEN_RBRACE) break; // السماح بفاصلة أخيرة
                            continue;
                        }
                        break;
                    }
                }

                eat(TOKEN_RBRACE);

                Node* en = ast_node_new(NODE_ENUM_DECL, tok_name);
                if (!en) return NULL;
                en->data.enum_decl.name = type_name;
                en->data.enum_decl.members = head;
                return en;
            }

            // struct / union
            eat(TOKEN_LBRACE);
            Node* head_f = NULL;
            Node* tail_f = NULL;
            while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF) {
                bool field_const = false;
                Token tok_field_const = {0};
                if (parser.current.type == TOKEN_CONST) {
                    field_const = true;
                    tok_field_const = parser.current;
                    eat(TOKEN_CONST);
                }

                DataType fdt = TYPE_INT;
                char* ftype_name = NULL;
                DataType fptr_base_type = TYPE_INT;
                char* fptr_base_type_name = NULL;
                int fptr_depth = 0;
                FuncPtrSig* ffunc_sig = NULL;
                if (!parse_type_spec(&fdt, &ftype_name,
                                     &fptr_base_type, &fptr_base_type_name,
                                     &fptr_depth,
                                     &ffunc_sig)) {
                    error_report(parser.current, "متوقع نوع حقل داخل الهيكل/الاتحاد.");
                    free(ftype_name);
                    free(fptr_base_type_name);
                    parser_funcsig_free(ffunc_sig);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                if (parser.current.type != TOKEN_IDENTIFIER) {
                    error_report(parser.current, "متوقع اسم حقل داخل الهيكل/الاتحاد.");
                    free(ftype_name);
                    free(fptr_base_type_name);
                    parser_funcsig_free(ffunc_sig);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                Token tok_field_name = parser.current;
                char* fname = strdup(parser.current.value);
                eat(TOKEN_IDENTIFIER);

                if (parser.current.type == TOKEN_LBRACKET) {
                    error_report(parser.current, "حقول المصفوفات داخل الهيكل/الاتحاد غير مدعومة بعد.");
                    free(ftype_name);
                    free(fptr_base_type_name);
                    parser_funcsig_free(ffunc_sig);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    break;
                }

                eat(TOKEN_DOT);

                Node* field = ast_node_new(NODE_VAR_DECL, tok_field_name);
                if (!field) return NULL;
                field->data.var_decl.name = fname;
                field->data.var_decl.type = fdt;
                field->data.var_decl.type_name = ftype_name;
                field->data.var_decl.ptr_base_type = fptr_base_type;
                field->data.var_decl.ptr_base_type_name = fptr_base_type_name;
                field->data.var_decl.ptr_depth = fptr_depth;
                field->data.var_decl.func_sig = ffunc_sig;
                field->data.var_decl.struct_size = 0;
                field->data.var_decl.struct_align = 0;
                field->data.var_decl.expression = NULL;
                field->data.var_decl.is_global = false;
                field->data.var_decl.is_const = field_const;
                (void)tok_field_const;

                if (!head_f) { head_f = field; tail_f = field; }
                else { tail_f->next = field; tail_f = field; }
            }
            eat(TOKEN_RBRACE);

            if (dt == TYPE_STRUCT) {
                Node* st = ast_node_new(NODE_STRUCT_DECL, tok_name);
                if (!st) return NULL;
                st->data.struct_decl.name = type_name;
                st->data.struct_decl.fields = head_f;
                return st;
            }

            Node* un = ast_node_new(NODE_UNION_DECL, tok_name);
            if (!un) return NULL;
            un->data.union_decl.name = type_name;
            un->data.union_decl.fields = head_f;
            return un;
        }

        // متغير/دالة: نحتاج اسم الرمز بعد نوعه
        Token tok_name = parser.current;
        if (parser.current.type != TOKEN_IDENTIFIER) {
            error_report(parser.current, "متوقع اسم معرّف بعد النوع.");
            free(type_name);
            free(ptr_base_type_name);
            parser_funcsig_free(type_func_sig);
            synchronize_mode(PARSER_SYNC_DECLARATION);
            return NULL;
        }

        char* name = strdup(parser.current.value);
        eat(TOKEN_IDENTIFIER);

        if (dt == TYPE_VOID && parser.current.type != TOKEN_LPAREN) {
            error_report(tok_type, "لا يمكن تعريف متغير عام من نوع 'عدم'.");
        }

        // دالة: نسمح فقط بأنواع بدائية حالياً
        if (parser.current.type == TOKEN_LPAREN) {
                if (dt == TYPE_ENUM || dt == TYPE_STRUCT || dt == TYPE_UNION) {
                    error_report(parser.current, "أنواع الإرجاع المعرفة من المستخدم غير مدعومة بعد في تواقيع الدوال.");
                    free(type_name);
                    free(ptr_base_type_name);
                    parser_funcsig_free(type_func_sig);
                    free(name);
                    synchronize_mode(PARSER_SYNC_DECLARATION);
                    return NULL;
                }
            if (is_const || is_static) {
                error_report(parser.current, "لا يمكن تعريف الدوال بوسم 'ثابت' أو 'ساكن'.");
            }

            eat(TOKEN_LPAREN);
            Node* head_param = NULL;
            Node* tail_param = NULL;
            bool is_variadic = false;
            if (parser.current.type != TOKEN_RPAREN) {
                while (1) {
                    if (parser.current.type == TOKEN_ELLIPSIS) {
                        if (!head_param) {
                            error_report(parser.current, "غير مدعوم: '...' يتطلب وجود معامل ثابت واحد على الأقل.");
                        } else {
                            is_variadic = true;
                        }
                        eat(TOKEN_ELLIPSIS);
                        break;
                    }

                    // معاملات الدالة: نسمح بالأنواع البدائية العددية/النص/منطقي/حرف + سكر نحوي 'T[]' (يُعامل كمؤشر).
                    Token tok_param_type = parser.current;
                    DataType param_dt = TYPE_INT;
                    char* param_tn = NULL;
                    DataType param_ptr_base_type = TYPE_INT;
                    char* param_ptr_base_type_name = NULL;
                    int param_ptr_depth = 0;
                    FuncPtrSig* param_func_sig = NULL;
                    if (!parse_type_spec(&param_dt, &param_tn,
                                         &param_ptr_base_type, &param_ptr_base_type_name,
                                         &param_ptr_depth,
                                         &param_func_sig)) {
                        error_report(parser.current, "متوقع نوع للمعامل.");
                        synchronize_mode(PARSER_SYNC_DECLARATION);
                        return NULL;
                    }
                    if (param_dt == TYPE_ENUM || param_dt == TYPE_STRUCT || param_dt == TYPE_UNION) {
                        error_report(tok_param_type, "أنواع المعاملات المعرفة من المستخدم غير مدعومة بعد في تواقيع الدوال.");
                        if (param_tn) free(param_tn);
                        if (param_ptr_base_type_name) free(param_ptr_base_type_name);
                        parser_funcsig_free(param_func_sig);
                        param_tn = NULL;
                        param_dt = TYPE_INT;
                        param_func_sig = NULL;
                    } else if (param_dt == TYPE_VOID) {
                        error_report(tok_param_type, "لا يمكن استخدام 'عدم' كنوع معامل.");
                        if (param_tn) free(param_tn);
                        if (param_ptr_base_type_name) free(param_ptr_base_type_name);
                        parser_funcsig_free(param_func_sig);
                        param_tn = NULL;
                        param_dt = TYPE_INT;
                        param_func_sig = NULL;
                    }

                    char* pname = NULL;

                    // -----------------------------------------------------------------
                    // سكر نحوي لمعلمات الدالة: T[] ⇢ T*
                    // ملاحظة: هذا يختلف عن مصفوفات اللغة الثابتة الحجم؛ هنا المقصود "مؤشر" مثل C.
                    // -----------------------------------------------------------------
                    if (parser.current.type == TOKEN_LBRACKET) {
                        Token tok_lb = parser.current;
                        eat(TOKEN_LBRACKET);
                        if (parser.current.type != TOKEN_RBRACKET) {
                            error_report(tok_lb, "غير مدعوم: معاملات الدوال تدعم فقط '[]' بدون حجم (سكر نحوي لمؤشر).");
                            // محاولة استرداد بسيطة: التخلي عن تحليل هذه الدالة.
                            free(pname);
                            free(param_tn);
                            free(param_ptr_base_type_name);
                            parser_funcsig_free(param_func_sig);
                            synchronize_mode(PARSER_SYNC_DECLARATION);
                            return NULL;
                        }
                        eat(TOKEN_RBRACKET);

                        if (parser.current.type == TOKEN_LBRACKET) {
                            error_report(tok_lb, "غير مدعوم: أبعاد متعددة '[][]' في معاملات الدوال.");
                            free(pname);
                            free(param_tn);
                            free(param_ptr_base_type_name);
                            parser_funcsig_free(param_func_sig);
                            synchronize_mode(PARSER_SYNC_DECLARATION);
                            return NULL;
                        }

                        if (param_dt == TYPE_FUNC_PTR) {
                            error_report(tok_param_type, "غير مدعوم: لاحقة '[]' بعد نوع مؤشر دالة.");
                            // استرداد: نتجاهل '[]' ونُبقي النوع كما هو.
                        } else if (param_dt == TYPE_POINTER) {
                            // مثل C: T*[] ⇢ T**
                            param_ptr_depth++;
                        } else {
                            param_ptr_base_type = param_dt;
                            param_ptr_depth = 1;
                            param_dt = TYPE_POINTER;
                        }
                    }

                    Token tok_param_name = tok_param_type;
                    if (parser.current.type == TOKEN_IDENTIFIER) {
                        tok_param_name = parser.current;
                        pname = strdup(parser.current.value);
                        eat(TOKEN_IDENTIFIER);
                    }

                    Node* param = ast_node_new(NODE_VAR_DECL, tok_param_name);
                    if (!param) {
                        free(pname);
                        free(param_tn);
                        free(param_ptr_base_type_name);
                        parser_funcsig_free(param_func_sig);
                        return NULL;
                    }
                    param->data.var_decl.name = pname;
                    param->data.var_decl.type = param_dt;
                    param->data.var_decl.type_name = param_tn;
                    param->data.var_decl.ptr_base_type = param_ptr_base_type;
                    param->data.var_decl.ptr_base_type_name = param_ptr_base_type_name;
                    param->data.var_decl.ptr_depth = param_ptr_depth;
                    param->data.var_decl.func_sig = param_func_sig;
                    param->data.var_decl.expression = NULL;
                    param->data.var_decl.is_global = false;
                    param->data.var_decl.is_const = false;
                    if (head_param == NULL) { head_param = param; tail_param = param; }
                    else { tail_param->next = param; tail_param = param; }

                    if (parser.current.type == TOKEN_COMMA) eat(TOKEN_COMMA);
                    else break;
                }
            }
            eat(TOKEN_RPAREN);

            Node* body = NULL;
            bool is_proto = false;

            if (parser.current.type == TOKEN_DOT) {
                eat(TOKEN_DOT);
                is_proto = true;
            } else {
                body = parse_block();
                is_proto = false;
            }

            Node* func = ast_node_new(NODE_FUNC_DEF, tok_name);
            if (!func) {
                free(name);
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(type_func_sig);
                return NULL;
            }
            func->data.func_def.name = name;
            func->data.func_def.return_type = dt;
            func->data.func_def.return_ptr_base_type = ptr_base_type;
            func->data.func_def.return_ptr_base_type_name = ptr_base_type_name;
            func->data.func_def.return_ptr_depth = ptr_depth;
            func->data.func_def.return_func_sig = type_func_sig;
            func->data.func_def.params = head_param;
            func->data.func_def.body = body;
            func->data.func_def.is_variadic = is_variadic;
            func->data.func_def.is_prototype = is_proto;
            free(type_name);
            return func;
        }

        // تعريف مصفوفة عامة ثابتة الأبعاد
        if (parser.current.type == TOKEN_LBRACKET) {
            if (dt == TYPE_FUNC_PTR) {
                error_report(parser.current, "مصفوفات من نوع مؤشر دالة غير مدعومة بعد.");
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(type_func_sig);
                free(name);
                synchronize_mode(PARSER_SYNC_DECLARATION);
                return NULL;
            }
            int* dims = NULL;
            int dim_count = 0;
            int64_t total_elems = 0;
            if (!parse_array_dimensions(&dims, &dim_count, &total_elems)) {
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(type_func_sig);
                free(name);
                synchronize_mode(PARSER_SYNC_DECLARATION);
                return NULL;
            }

            int init_count = 0;
            bool has_init = false;
            Node* init_vals = parse_array_initializer_list(&init_count, &has_init);

            eat(TOKEN_DOT);

            Node* arr = ast_node_new(NODE_ARRAY_DECL, tok_name);
            if (!arr) {
                free(dims);
                free(type_name);
                free(name);
                parser_funcsig_free(type_func_sig);
                return NULL;
            }
            arr->data.array_decl.name = name;
            arr->data.array_decl.element_type = dt;
            arr->data.array_decl.element_type_name = type_name;
            arr->data.array_decl.element_ptr_base_type = ptr_base_type;
            arr->data.array_decl.element_ptr_base_type_name = ptr_base_type_name;
            arr->data.array_decl.element_ptr_depth = ptr_depth;
            arr->data.array_decl.element_struct_size = 0;
            arr->data.array_decl.element_struct_align = 0;
            arr->data.array_decl.dims = dims;
            arr->data.array_decl.dim_count = dim_count;
            arr->data.array_decl.total_elems = total_elems;
            arr->data.array_decl.is_global = true;
            arr->data.array_decl.is_const = is_const;
            arr->data.array_decl.is_static = is_static;
            arr->data.array_decl.has_init = has_init;
            arr->data.array_decl.init_values = init_vals;
            arr->data.array_decl.init_count = init_count;
            parser_funcsig_free(type_func_sig);
            return arr;
        }

        // متغير عام
        if (dt == TYPE_STRUCT || dt == TYPE_UNION) {
            if (is_const) {
                error_report(parser.current, "لا يمكن تعريف نوع مركب ثابت بدون تهيئة (غير مدعوم حالياً).");
            }
            if (parser.current.type == TOKEN_ASSIGN) {
                error_report(parser.current, "تهيئة الأنواع المركبة العامة بالصيغة '=' غير مدعومة حالياً.");
                eat(TOKEN_ASSIGN);
                (void)parse_expression();
            }
            eat(TOKEN_DOT);

            Node* var = ast_node_new(NODE_VAR_DECL, tok_name);
            if (!var) {
                free(type_name);
                free(ptr_base_type_name);
                parser_funcsig_free(type_func_sig);
                free(name);
                return NULL;
            }
            var->data.var_decl.name = name;
            var->data.var_decl.type = dt;
            var->data.var_decl.type_name = type_name;
            var->data.var_decl.ptr_base_type = ptr_base_type;
            var->data.var_decl.ptr_base_type_name = ptr_base_type_name;
            var->data.var_decl.ptr_depth = ptr_depth;
            var->data.var_decl.func_sig = type_func_sig;
            var->data.var_decl.expression = NULL;
            var->data.var_decl.is_global = true;
            var->data.var_decl.is_const = is_const;
            var->data.var_decl.is_static = is_static;
            return var;
        }

        Node* expr = NULL;
        if (parser.current.type == TOKEN_ASSIGN) {
            eat(TOKEN_ASSIGN);
            expr = parse_expression();
        }
        eat(TOKEN_DOT);

        Node* var = ast_node_new(NODE_VAR_DECL, tok_name);
        if (!var) {
            free(type_name);
            free(ptr_base_type_name);
            parser_funcsig_free(type_func_sig);
            free(name);
            return NULL;
        }
        var->data.var_decl.name = name;
        var->data.var_decl.type = dt;
        var->data.var_decl.type_name = type_name;
        var->data.var_decl.ptr_base_type = ptr_base_type;
        var->data.var_decl.ptr_base_type_name = ptr_base_type_name;
        var->data.var_decl.ptr_depth = ptr_depth;
        var->data.var_decl.func_sig = type_func_sig;
        var->data.var_decl.expression = expr;
        var->data.var_decl.is_global = true;
        var->data.var_decl.is_const = is_const;
        var->data.var_decl.is_static = is_static;
        return var;
    }
    
    if (!parser.panic_mode) {
        error_report(parser.current, "متوقع تصريح (دالة أو متغير عام).");
        parser.panic_mode = true;
    }
    synchronize_mode(PARSER_SYNC_DECLARATION);
    return NULL;
}

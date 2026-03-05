// ============================================================================
// IR Block Construction
// ============================================================================

/**
 * Create a new basic block
 */
IRBlock* ir_block_new(const char* label, int id) {
    IRBlock* block = (IRBlock*)ir_alloc(sizeof(IRBlock), _Alignof(IRBlock));
    if (!block) return NULL;
    
    block->label = label ? ir_strdup(label) : NULL;
    block->id = id;
    block->parent = NULL;
    block->first = NULL;
    block->last = NULL;
    block->inst_count = 0;
    block->succs[0] = NULL;
    block->succs[1] = NULL;
    block->succ_count = 0;
    block->preds = NULL;
    block->pred_count = 0;
    block->pred_capacity = 0;
    block->idom = NULL;
    block->dom_frontier = NULL;
    block->dom_frontier_count = 0;
    block->next = NULL;
    
    return block;
}

/**
 * Append an instruction to a block
 */
void ir_block_append(IRBlock* block, IRInst* inst) {
    if (!block || !inst) return;

    if (block->parent) {
        ir_func_invalidate_defuse(block->parent);
    }

    inst->parent = block;
    if (inst->id < 0 && block->parent) {
        inst->id = block->parent->next_inst_id++;
    }
    
    inst->prev = block->last;
    inst->next = NULL;
    
    if (block->last) {
        block->last->next = inst;
    } else {
        block->first = inst;
    }
    block->last = inst;
    block->inst_count++;
}

/**
 * Add a predecessor to a block
 */
void ir_block_add_pred(IRBlock* block, IRBlock* pred) {
    if (!block || !pred) return;
    
    // Grow array if needed
    if (block->pred_count >= block->pred_capacity) {
        int new_cap = block->pred_capacity == 0 ? 4 : block->pred_capacity * 2;
        IRBlock** new_preds = (IRBlock**)realloc(block->preds, new_cap * sizeof(IRBlock*));
        if (!new_preds) return;
        block->preds = new_preds;
        block->pred_capacity = new_cap;
    }
    
    block->preds[block->pred_count++] = pred;
}

/**
 * Add a successor to a block
 */
void ir_block_add_succ(IRBlock* block, IRBlock* succ) {
    if (!block || !succ || block->succ_count >= 2) return;
    block->succs[block->succ_count++] = succ;
    ir_block_add_pred(succ, block);
}

/**
 * Check if a block is terminated (ends with br, br_cond, or ret)
 */
int ir_block_is_terminated(IRBlock* block) {
    if (!block || !block->last) return 0;
    IROp op = block->last->op;
    return op == IR_OP_BR || op == IR_OP_BR_COND || op == IR_OP_RET;
}

/**
 * Free a basic block and its instructions
 */
void ir_block_free(IRBlock* block) {
    (void)block;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للكتل.
}

// ============================================================================
// IR Function Construction
// ============================================================================

/**
 * Create a new function
 */
IRFunc* ir_func_new(const char* name, IRType* ret_type) {
    IRFunc* func = (IRFunc*)ir_alloc(sizeof(IRFunc), _Alignof(IRFunc));
    if (!func) return NULL;
    
    func->name = name ? ir_strdup(name) : NULL;
    func->ret_type = ret_type ? ret_type : IR_TYPE_VOID_T;
    func->params = NULL;
    func->param_count = 0;
    func->entry = NULL;
    func->blocks = NULL;
    func->block_count = 0;
    func->next_reg = 0;
    func->next_inst_id = 0;
    func->ir_epoch = 1;
    func->def_use = NULL;
    func->next_block_id = 0;
    func->is_prototype = false;
    func->is_variadic = false;
    func->next = NULL;
    
    return func;
}

/**
 * Allocate a new virtual register in a function
 */
int ir_func_alloc_reg(IRFunc* func) {
    if (!func) return -1;
    return func->next_reg++;
}

/**
 * Allocate a new block ID in a function
 */
int ir_func_alloc_block_id(IRFunc* func) {
    if (!func) return -1;
    return func->next_block_id++;
}

/**
 * Add a basic block to a function
 */
void ir_func_add_block(IRFunc* func, IRBlock* block) {
    if (!func || !block) return;

    block->parent = func;
    
    if (!func->blocks) {
        func->blocks = block;
        func->entry = block;
    } else {
        // Find last block and append
        IRBlock* last = func->blocks;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
    }
    func->block_count++;
}

/**
 * Create and add a new block to a function
 */
IRBlock* ir_func_new_block(IRFunc* func, const char* label) {
    if (!func) return NULL;
    
    int id = ir_func_alloc_block_id(func);
    IRBlock* block = ir_block_new(label, id);
    if (!block) return NULL;
    
    ir_func_add_block(func, block);
    return block;
}

/**
 * Add a parameter to a function
 */
void ir_func_add_param(IRFunc* func, const char* name, IRType* type) {
    if (!func) return;

    // توسيع مصفوفة المعاملات داخل الساحة (Arena) عبر نسخ إلى مصفوفة جديدة.
    int new_count = func->param_count + 1;
    IRParam* new_params = (IRParam*)ir_alloc((size_t)new_count * sizeof(IRParam), _Alignof(IRParam));
    if (!new_params) return;
    for (int i = 0; i < func->param_count; i++) {
        new_params[i] = func->params[i];
    }
    func->params = new_params;

    func->params[func->param_count].name = name ? ir_strdup(name) : NULL;
    func->params[func->param_count].type = type;
    func->params[func->param_count].reg = ir_func_alloc_reg(func);
    func->param_count = new_count;
}

/**
 * Free a function and all its blocks
 */
void ir_func_free(IRFunc* func) {
    (void)func;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للدوال.
}

// ============================================================================
// IR Global Variable Construction
// ============================================================================

/**
 * Create a new global variable
 */
IRGlobal* ir_global_new(const char* name, IRType* type, int is_const) {
    IRGlobal* global = (IRGlobal*)ir_alloc(sizeof(IRGlobal), _Alignof(IRGlobal));
    if (!global) return NULL;
    
    global->name = name ? ir_strdup(name) : NULL;
    global->type = type;
    global->init = NULL;
    global->init_elems = NULL;
    global->init_elem_count = 0;
    global->has_init_list = false;
    global->is_const = is_const ? true : false;
    global->is_internal = false;
    global->next = NULL;
    
    return global;
}

/**
 * Set initializer for a global
 */
void ir_global_set_init(IRGlobal* global, IRValue* init) {
    if (!global) return;
    global->init = init;
}

/**
 * Free a global variable
 */
void ir_global_free(IRGlobal* global) {
    (void)global;
    // تُدار الذاكرة عبر الساحة (Arena): لا تحرير فردي للعوام.
}

// ============================================================================
// IR Module Construction
// ============================================================================

/**
 * Create a new module
 */
IRModule* ir_module_new(const char* name) {
    IRModule* module = (IRModule*)malloc(sizeof(IRModule));
    if (!module) return NULL;

    // تهيئة الساحة أولاً ثم اجعل هذه الوحدة هي الوحدة الحالية.
    ir_arena_init(&module->arena, 0);
    module->cached_i8_ptr_type = NULL;
    ir_module_set_current(module);

    module->name = name ? ir_strdup(name) : NULL;
    module->globals = NULL;
    module->global_count = 0;
    module->funcs = NULL;
    module->func_count = 0;
    module->strings = NULL;
    module->string_count = 0;

    module->baa_strings = NULL;
    module->baa_string_count = 0;
     
    return module;
}

/**
 * Add a function to a module
 */
void ir_module_add_func(IRModule* module, IRFunc* func) {
    if (!module || !func) return;
    
    if (!module->funcs) {
        module->funcs = func;
    } else {
        IRFunc* last = module->funcs;
        while (last->next) {
            last = last->next;
        }
        last->next = func;
    }
    module->func_count++;
}

/**
 * Add a global variable to a module
 */
void ir_module_add_global(IRModule* module, IRGlobal* global) {
    if (!module || !global) return;
    
    if (!module->globals) {
        module->globals = global;
    } else {
        IRGlobal* last = module->globals;
        while (last->next) {
            last = last->next;
        }
        last->next = global;
    }
    module->global_count++;
}

/**
 * Add a string constant to a module (returns string ID)
 */
int ir_module_add_string(IRModule* module, const char* str) {
    if (!module || !str) return -1;

    // اجعل الوحدة الحالية صحيحة قبل أي تخصيص.
    ir_module_set_current(module);
    
    // Check if string already exists
    IRStringEntry* entry = module->strings;
    while (entry) {
        if (entry->content && strcmp(entry->content, str) == 0) {
            return entry->id;
        }
        entry = entry->next;
    }
    
    // Create new entry
    IRStringEntry* new_entry = (IRStringEntry*)ir_alloc(sizeof(IRStringEntry), _Alignof(IRStringEntry));
    if (!new_entry) return -1;
    
    new_entry->id = module->string_count++;
    new_entry->content = ir_strdup(str);
    new_entry->next = module->strings;
    module->strings = new_entry;
    
    return new_entry->id;
}

/**
 * Find a function by name
 */
IRFunc* ir_module_find_func(IRModule* module, const char* name) {
    if (!module || !name) return NULL;
    
    IRFunc* func = module->funcs;
    while (func) {
        if (func->name && strcmp(func->name, name) == 0) {
            return func;
        }
        func = func->next;
    }
    return NULL;
}

/**
 * Find a global by name
 */
IRGlobal* ir_module_find_global(IRModule* module, const char* name) {
    if (!module || !name) return NULL;
    
    IRGlobal* global = module->globals;
    while (global) {
        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
        global = global->next;
    }
    return NULL;
}

/**
 * Get a string by ID
 */
const char* ir_module_get_string(IRModule* module, int id) {
    if (!module) return NULL;
    
    IRStringEntry* entry = module->strings;
    while (entry) {
        if (entry->id == id) {
            return entry->content;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * Free a module and all its contents
 */
void ir_module_free(IRModule* module) {
    if (!module) return;

    // تحرير ما تبقى من كاشات التحليل التي تم تخصيصها بالـ heap.
    // (الكتل/التعليمات/القيم نفسها داخل الساحة.)
    for (IRFunc* f = module->funcs; f; f = f->next) {
        if (f->def_use) {
            ir_defuse_free(f->def_use);
            f->def_use = NULL;
        }
        for (IRBlock* b = f->blocks; b; b = b->next) {
            ir_block_free_analysis_caches(b);
        }
    }

    // تدمير الساحة ثم تحرير هيكل الوحدة.
    ir_arena_destroy(&module->arena);

    if (g_ir_current_module == module) {
        g_ir_current_module = NULL;
    }

    free(module);
}


#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    return node.type == TYPE_INT ? static_cast<Value *>(CONST_INT(node.i_val))
                                 : static_cast<Value *>(CONST_FP(node.f_val));
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    Type *element_type = type_of(node.type);
    Type *decl_type = element_type;
    unsigned size = 0;
    if (node.num != nullptr) {
        size = static_cast<unsigned>(node.num->i_val);
        decl_type = module->get_array_type(element_type, size);
    }
    if (scope.in_global()) {
        auto *init = ConstantZero::get(decl_type, module.get());
        auto *global = GlobalVariable::create(node.id, module.get(), decl_type,
                                              false, init);
        scope.push(node.id, global);
    } else {
        auto *current_block = builder->get_insert_block();
        builder->set_insert_point(context.entry_block);
        auto *alloca = builder->create_alloca_begin(decl_type);
        builder->set_insert_point(current_block);
        scope.push(node.id, alloca);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        param_types.push_back(param->isarray
                                  ? module->get_pointer_type(type_of(param->type))
                                  : type_of(param->type));
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    context.entry_block = funBB;
    builder->set_insert_point(funBB);
    scope.enter();
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        auto &param = node.params[i];
        if (param->isarray) {
            scope.push(param->id, args[i]);
        } else {
            auto *alloca = builder->create_alloca_begin(type_of(param->type));
            builder->create_store(args[i], alloca);
            scope.push(param->id, alloca);
        }
    }
    context.compound_depth = 0;
    node.compound_stmt->accept(*this);
    if (not builder->get_insert_block()->is_terminated()) 
    {
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    bool nested = context.compound_depth++ > 0;
    if (nested)
        scope.enter();
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())
            break;
    }
    if (nested)
        scope.exit();
    --context.compound_depth;
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    if (node.expression != nullptr)
        node.expression->accept(*this);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    auto *cond = cast_value(node.expression->accept(*this), INT1_T);
    auto id = std::to_string(context.block_id++);
    auto *then_bb = BasicBlock::create(module.get(), "if.then." + id, context.func);
    auto *else_bb = node.else_statement
                        ? BasicBlock::create(module.get(), "if.else." + id, context.func)
                        : nullptr;
    auto *merge_bb = BasicBlock::create(module.get(), "if.end." + id, context.func);
    builder->create_cond_br(cond, then_bb, node.else_statement ? else_bb : merge_bb);
    builder->set_insert_point(then_bb);
    node.if_statement->accept(*this);
    bool then_terminated = builder->get_insert_block()->is_terminated();
    if (!then_terminated)
        builder->create_br(merge_bb);
    if (node.else_statement) {
        builder->set_insert_point(else_bb);
        node.else_statement->accept(*this);
        bool else_terminated = builder->get_insert_block()->is_terminated();
        if (!else_terminated)
            builder->create_br(merge_bb);
        if (then_terminated && else_terminated) {
            builder->set_insert_point(merge_bb);
            if (context.func->get_return_type()->is_void_type())
                builder->create_void_ret();
            else if (context.func->get_return_type()->is_float_type())
                builder->create_ret(CONST_FP(0.));
            else
                builder->create_ret(CONST_INT(0));
            return nullptr;
        }
    }
    builder->set_insert_point(merge_bb);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    auto id = std::to_string(context.block_id++);
    auto *cond_bb = BasicBlock::create(module.get(), "while.cond." + id, context.func);
    auto *body_bb = BasicBlock::create(module.get(), "while.body." + id, context.func);
    auto *end_bb = BasicBlock::create(module.get(), "while.end." + id, context.func);
    builder->create_br(cond_bb);
    builder->set_insert_point(cond_bb);
    auto *cond = cast_value(node.expression->accept(*this), INT1_T);
    builder->create_cond_br(cond, body_bb, end_bb);
    builder->set_insert_point(body_bb);
    node.statement->accept(*this);
    if (!builder->get_insert_block()->is_terminated())
        builder->create_br(cond_bb);
    builder->set_insert_point(end_bb);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
        return nullptr;
    } else {
        auto *value = node.expression->accept(*this);
        builder->create_ret(cast_value(value, context.func->get_return_type()));
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    auto *address = get_address(node);
    if (node.expression == nullptr &&
        address->get_type()->get_pointer_element_type()->is_array_type())
        return builder->create_gep(address, {CONST_INT(0), CONST_INT(0)});
    if (node.expression == nullptr &&
        address->get_type()->get_pointer_element_type()->is_pointer_type())
        return address;
    if (node.expression == nullptr && address->is<Argument>())
        return address;
    return builder->create_load(address);
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    auto *address = get_address(*node.var);
    auto *value = node.expression->accept(*this);
    auto *element_type = address->get_type()->get_pointer_element_type();
    value = cast_value(value, element_type);
    builder->create_store(value, address);
    return value;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    auto *lhs = node.additive_expression_l->accept(*this);
    if (node.additive_expression_r == nullptr)
        return lhs;
    auto *rhs = node.additive_expression_r->accept(*this);
    if (lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type()) {
        lhs = cast_value(lhs, FLOAT_T);
        rhs = cast_value(rhs, FLOAT_T);
        switch (node.op) {
        case OP_LE: return builder->create_fcmp_le(lhs, rhs);
        case OP_LT: return builder->create_fcmp_lt(lhs, rhs);
        case OP_GT: return builder->create_fcmp_gt(lhs, rhs);
        case OP_GE: return builder->create_fcmp_ge(lhs, rhs);
        case OP_EQ: return builder->create_fcmp_eq(lhs, rhs);
        default: return builder->create_fcmp_ne(lhs, rhs);
        }
    }
    lhs = cast_value(lhs, INT32_T);
    rhs = cast_value(rhs, INT32_T);
    switch (node.op) {
    case OP_LE: return builder->create_icmp_le(lhs, rhs);
    case OP_LT: return builder->create_icmp_lt(lhs, rhs);
    case OP_GT: return builder->create_icmp_gt(lhs, rhs);
    case OP_GE: return builder->create_icmp_ge(lhs, rhs);
    case OP_EQ: return builder->create_icmp_eq(lhs, rhs);
    default: return builder->create_icmp_ne(lhs, rhs);
    }
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    auto *lhs = node.additive_expression ? node.additive_expression->accept(*this) : nullptr;
    auto *rhs = node.term->accept(*this);
    if (lhs == nullptr)
        return rhs;
    if (lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type()) {
        lhs = cast_value(lhs, FLOAT_T); rhs = cast_value(rhs, FLOAT_T);
        return node.op == OP_PLUS ? static_cast<Value *>(builder->create_fadd(lhs, rhs))
                                  : static_cast<Value *>(builder->create_fsub(lhs, rhs));
    }
    return node.op == OP_PLUS ? static_cast<Value *>(builder->create_iadd(lhs, rhs))
                              : static_cast<Value *>(builder->create_isub(lhs, rhs));
}

Value* CminusfBuilder::visit(ASTTerm &node) {
    auto *lhs = node.term ? node.term->accept(*this) : nullptr;
    auto *rhs = node.factor->accept(*this);
    if (lhs == nullptr)
        return rhs;
    if (lhs->get_type()->is_float_type() || rhs->get_type()->is_float_type()) {
        lhs = cast_value(lhs, FLOAT_T); rhs = cast_value(rhs, FLOAT_T);
        return node.op == OP_MUL ? static_cast<Value *>(builder->create_fmul(lhs, rhs))
                                 : static_cast<Value *>(builder->create_fdiv(lhs, rhs));
    }
    return node.op == OP_MUL ? static_cast<Value *>(builder->create_imul(lhs, rhs))
                             : static_cast<Value *>(builder->create_isdiv(lhs, rhs));
}

Value* CminusfBuilder::visit(ASTCall &node) {
    auto *func = static_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    for (unsigned i = 0; i < node.args.size(); ++i) {
        auto *arg = node.args[i]->accept(*this);
        args.push_back(cast_value(arg, func->get_function_type()->get_param_type(i)));
    }
    auto *call = builder->create_call(func, args);
    return func->get_return_type()->is_void_type() ? nullptr : call;
}

Type *CminusfBuilder::type_of(CminusType type) {
    if (type == TYPE_FLOAT) return FLOAT_T;
    if (type == TYPE_VOID) return VOID_T;
    return INT32_T;
}

Value *CminusfBuilder::cast_value(Value *value, Type *type) {
    if (value == nullptr || value->get_type() == type)
        return value;
    if (type == INT1_T) {
        if (value->get_type()->is_float_type())
            return builder->create_fcmp_ne(value, CONST_FP(0.));
        return builder->create_icmp_ne(value, CONST_INT(0));
    }
    if (type->is_int32_type() && value->get_type()->is_int1_type())
        return builder->create_zext(value, INT32_T);
    if (type->is_float_type() && value->get_type()->is_int1_type())
        value = builder->create_zext(value, INT32_T);
    if (type->is_float_type() && value->get_type()->is_int32_type())
        return builder->create_sitofp(value, FLOAT_T);
    if (type->is_int32_type() && value->get_type()->is_float_type())
        return builder->create_fptosi(value, INT32_T);
    return value;
}

Value *CminusfBuilder::get_address(ASTVar &node) {
    auto *address = scope.find(node.id);
    if (node.expression == nullptr)
        return address;
    auto *index = node.expression->accept(*this);
    index = cast_value(index, INT32_T);
    auto *negative = builder->create_icmp_lt(index, CONST_INT(0));
    auto id = std::to_string(context.block_id++);
    auto *bad_bb = BasicBlock::create(module.get(), "neg.index." + id, context.func);
    auto *ok_bb = BasicBlock::create(module.get(), "index.ok." + id, context.func);
    builder->create_cond_br(negative, bad_bb, ok_bb);
    builder->set_insert_point(bad_bb);
    builder->create_call(scope.find("neg_idx_except"), {});
    builder->create_br(ok_bb);
    builder->set_insert_point(ok_bb);
    std::vector<Value *> indices{CONST_INT(0), index};
    if (!address->get_type()->get_pointer_element_type()->is_array_type())
        indices = {index};
    return builder->create_gep(address, indices);
}

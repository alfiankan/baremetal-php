#include <zend_API.h>
#include <zend_ast.h>

extern "C" {
    #include "llvm_wrapper.h"
}

#include "zend_types.h"
#include "zend_type_info.h"
#include "zend_compile.h"
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <vector>

struct LLVMManager {
	LLVMContextRef *llvm_ctx;
	LLVMModuleRef *llvm_module;
	LLVMBuilderRef *llvm_builder;
};

LLVMTypeRef zend_eval_llvm_type(char *type_val_label, LLVMManager *llvm_manager);


static inline bool zend_is_scope_known(void) /* {{{ */
{
	if (!CG(active_op_array)) {
		/* This can only happen when evaluating a default value string. */
		return 0;
	}

	if (CG(active_op_array)->fn_flags & ZEND_ACC_CLOSURE) {
		/* Closures can be rebound to a different scope */
		return 0;
	}

	if (!CG(active_class_entry)) {
		/* The scope is known if we're in a free function (no scope), but not if we're in
		 * a file/eval (which inherits including/eval'ing scope). */
		return CG(active_op_array)->function_name != NULL;
	}

	/* For traits self etc refers to the using class, not the trait itself */
	return (CG(active_class_entry)->ce_flags & ZEND_ACC_TRAIT) == 0;
}

static char* zend_compile_single_typename_to_string(zend_ast *ast)
{
	ZEND_ASSERT(!(ast->attr & ZEND_TYPE_NULLABLE));

	zend_string *class_name = zend_ast_get_str(ast);
	printf("TYPE: %s  == %d\n", class_name->val, ast->attr);

	return class_name->val;
}


void build_ir_to_executable(LLVMModuleRef *module, char* out_path) {
    std::ostringstream oss_ir;
    oss_ir << out_path << ".ll";
    LLVMPrintModuleToFile(*module, oss_ir.str().c_str(), NULL);
    LLVMPrintModuleToFile(*module, "/dev/stdout", NULL);

    std::ostringstream oss;
    oss << "clang" << " " << oss_ir.str() << " -o" << out_path << ".exe";
    int result = system(oss.str().c_str());

    if (result == -1) {
        std::cerr << "Error: Unable to execute command\n";
    } else {
        std::cout << "Command exited with status: " << WEXITSTATUS(result) << "\n";
    }
}

void compile_func_params(zend_ast *ast, std::vector<LLVMTypeRef> *params_types, LLVMManager *llvm_manager) {
	zend_ast_list *list = zend_ast_get_list(ast);

	for (int i = 0; i < list->children; ++i) {
		zend_ast *param_ast = list->child[i];
		zend_ast *type_ast = param_ast->child[0];
		zend_ast *var_ast = param_ast->child[1];
		zend_string *name = zval_make_interned_string(zend_ast_get_zval(var_ast));

		params_types->push_back(zend_eval_llvm_type(zend_ast_get_str(type_ast)->val, llvm_manager));

		printf("ARG %d = %s typeof = %s == %d\n", i, name->val, zend_ast_get_str(type_ast)->val, type_ast->attr);
	}
}

LLVMTypeRef zend_eval_llvm_type(char *type_val_label, LLVMManager *llvm_manager) {
	if (strcmp(type_val_label, "int") == 0) {
		return LLVMInt32TypeInContext(*llvm_manager->llvm_ctx);
	}
	if (strcmp(type_val_label, "bool") == 0) {
		return LLVMInt1TypeInContext(*llvm_manager->llvm_ctx);
	}
	if (strcmp(type_val_label, "string") == 0) {
		return NULL;
	}
	return NULL;

}

static bool is_globals_fetch(const zend_ast *ast)
{
	if (ast->kind == ZEND_AST_VAR && ast->child[0]->kind == ZEND_AST_ZVAL) {
		zval *name = zend_ast_get_zval(ast->child[0]);
		return Z_TYPE_P(name) == IS_STRING && zend_string_equals_literal(Z_STR_P(name), "GLOBALS");
	}

	return 0;
}

static bool is_global_var_fetch(zend_ast *ast)
{
	return ast->kind == ZEND_AST_DIM && is_globals_fetch(ast->child[0]);
}


void ast_traverse_compile(zend_ast *ast, LLVMManager *llvm_manager) {
    if (ast->kind == ZEND_AST_STMT_LIST) {
        printf("ZEND_AST_STMT_LIST\n");
    	zend_ast_list *list = zend_ast_get_list(ast);
    	uint32_t i;
    	for (i = 0; i < list->children; ++i) {
    		printf("AST KIND FROM LIST %d \n", list->child[i]->kind);
    	    ast_traverse_compile(list->child[i], llvm_manager);
    	}
    }
	if (ast->kind == ZEND_AST_ASSIGN) {
		printf("ZEND_AST_ASSIGN \n");
		zend_ast *var_ast = ast->child[0];
		zend_ast *expr_ast = ast->child[1];

		zend_ast_kind kind = is_global_var_fetch(var_ast) ? ZEND_AST_VAR : var_ast->kind;

		if (kind == ZEND_AST_VAR) {
			zend_ast *name_ast = var_ast->child[0];
			if (name_ast->kind == ZEND_AST_ZVAL) {
				zend_string *name = zval_make_interned_string(zend_ast_get_zval(name_ast));
				printf("STMT VAR %d %s\n", var_ast->kind  == ZEND_AST_VAR, name->val);

				// create allocation to assign
				LLVMTypeRef var_type = zend_eval_llvm_type("int", llvm_manager);
				auto alloca = LLVMBuildAlloca(*llvm_manager->llvm_builder, var_type, name->val);

				LLVMValueRef value = LLVMConstInt(zend_eval_llvm_type("int", llvm_manager), 42, 0);
				LLVMBuildStore(*llvm_manager->llvm_builder, value, alloca);
			}
		}

	}

    if (ast->kind == ZEND_AST_FUNC_DECL) {
        auto *decl = reinterpret_cast<zend_ast_decl*>(ast);
        zend_ast *params_ast = decl->child[0];
        zend_ast *uses_ast = decl->child[1];
        zend_ast *stmt_ast = decl->child[2];
        zend_ast *return_type_ast = decl->child[3];

    	LLVMTypeRef return_type = zend_eval_llvm_type(zend_compile_single_typename_to_string(return_type_ast), llvm_manager);

    	std::vector<LLVMTypeRef> params_types;
    	compile_func_params(params_ast, &params_types, llvm_manager);

    	LLVMTypeRef  main_function_type = LLVMFunctionType(return_type, params_types.data(), params_types.size(), false);
    	LLVMValueRef main_function = LLVMAddFunction(*llvm_manager->llvm_module, decl->name->val, main_function_type);

    	printf("GEN->ZEND_AST_FUNC_DECL %s\n", decl->name->val);

    	printf("    GEN->ZEND_AST_FUNC_DECL->RETURN %s\n", zend_compile_single_typename_to_string(return_type_ast));

		// function block
		if (ZEND_AST_STMT_LIST == stmt_ast->kind) {
			printf("HAS BLOCK\n");
			LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(*llvm_manager->llvm_ctx, main_function, "entry");
			LLVMPositionBuilderAtEnd(*llvm_manager->llvm_builder, entry);

			ast_traverse_compile(stmt_ast, llvm_manager);

		}
    }
}



void compile_ast_to_native(zend_ast *ast, char* out_path) {
    printf("COMPILING USING LLVM\n");
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  module = LLVMModuleCreateWithNameInContext("hello", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

	LLVMManager llvm_manager = {
		.llvm_ctx = &context,
		.llvm_module = &module ,
		.llvm_builder = &builder ,
	};

    ast_traverse_compile(ast, &llvm_manager);

    // LLVMTypeRef int_32_type = LLVMInt32TypeInContext(context);
    //
    // LLVMTypeRef  main_function_type = LLVMFunctionType(int_32_type, NULL, 0, false);
    // LLVMValueRef main_function = LLVMAddFunction(module, "main", main_function_type);
    //
    // LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_function, "entry");
    // LLVMPositionBuilderAtEnd(builder, entry);
    //
    // LLVMBuildRet(builder, LLVMConstInt(int_32_type, 90, false));


    //build_ir_to_executable(&module, out_path);
    LLVMPrintModuleToFile(module, "/dev/stdout", NULL);

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
}
#include <zend_API.h>
#include <zend_ast.h>

extern "C" {
    #include "llvm_wrapper.h"
}
#include "llvm/IR/Verifier.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>

#include "zend_types.h"
#include "zend_compile.h"
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>


class LLVMCompiler {
	private:
		llvm::LLVMContext llvm_ctx;
		llvm::Module *llvm_module;
		llvm::IRBuilder<> *llvm_builder;
		bool debug_on = true;

	protected:
		void ast_traverse_func_block(llvm::Function *FN, zend_ast *ast) {

			auto bb = llvm::BasicBlock::Create(llvm_ctx, "entry", FN);
			llvm_builder->SetInsertPoint(bb);
			//ast_traverse_func_decl(stmt_ast, llvm_manager);

			// solve block stmt
			zend_ast_list *list = zend_ast_get_list(ast);
			uint32_t i;
			for (i = 0; i < list->children; ++i) {
				DBG("AST FROM INSIDE BLOCK ", list->child[i]->kind);
				auto block_ast = list->child[i];

				if (block_ast->kind == ZEND_AST_RETURN) {
					DBG("ZEND_AST_RETURN");
					auto ret_val = llvm::ConstantInt::get(FN->getReturnType(), 10, false);
					llvm_builder->CreateRet(ret_val);

					// Verify the function
					llvm::verifyFunction(*FN);

				}
			}
		}

		void ast_traverse_func_params(zend_ast *ast, std::vector<llvm::Type*> *params_types) {
			zend_ast_list *list = zend_ast_get_list(ast);

			for (int i = 0; i < list->children; ++i) {
				zend_ast *param_ast = list->child[i];
				zend_ast *type_ast = param_ast->child[0];
				zend_ast *var_ast = param_ast->child[1];
				zend_string *name = zval_make_interned_string(zend_ast_get_zval(var_ast));

				auto param_type = zend_eval_llvm_type(zend_ast_get_str(type_ast)->val);
				params_types->push_back(param_type);

				DBG("ARG ", i, " = ", name->val, " typeof = ", zend_ast_get_str(type_ast)->val);
			}
		}

		void ast_traverse_func_decl(zend_ast *ast) {
			auto *decl = reinterpret_cast<zend_ast_decl*>(ast);
			zend_ast *params_ast = decl->child[0];
			zend_ast *uses_ast = decl->child[1];
			zend_ast *stmt_ast = decl->child[2];
			zend_ast *return_type_ast = decl->child[3];

			llvm::Type *return_type = zend_eval_llvm_type(zend_compile_single_typename_to_string(return_type_ast));

			std::vector<llvm::Type*> params_types;
			ast_traverse_func_params(params_ast, &params_types);

			llvm::FunctionType *FT = llvm::FunctionType::get(return_type, params_types, false);
			llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, decl->name->val, llvm_module);


			DBG("GEN->ZEND_AST_FUNC_DECL ", decl->name->val);
			DBG("    GEN->ZEND_AST_FUNC_DECL->RETURN ", zend_compile_single_typename_to_string(return_type_ast));

			// function block solve this until end of list
			if (ZEND_AST_STMT_LIST == stmt_ast->kind) {
				ast_traverse_func_block(F, stmt_ast);
				DBG("HAS BLOCK");
			}
			// if (ZEND_AST_RETURN == stmt_ast->kind) {
			// 	printf("RETURN DECL \n");
			// 	ast_traverse_func_decl(stmt_ast, llvm_manager);
			// }
		}

		void ast_assign_variable(zend_ast *ast) {
			zend_ast *var_ast = ast->child[0];
			zend_ast *expr_ast = ast->child[1];

			if (var_ast->kind == ZEND_AST_VAR) {
				zend_ast *name_ast = var_ast->child[0];
				if (name_ast->kind == ZEND_AST_ZVAL) {
					zend_string *name = zval_make_interned_string(zend_ast_get_zval(name_ast));
					DBG("STMT VAR ", var_ast->kind  == ZEND_AST_VAR, name->val);

					auto var_type = zend_eval_llvm_type("int");
					auto *alloca = llvm_builder->CreateAlloca(var_type, nullptr, name->val);

					auto value = llvm::ConstantInt::get(var_type, 10, false);
					auto store = llvm_builder->CreateStore(value, alloca);

				}
			}
		}

		void ast_traverse_root(zend_ast *ast) {
			DBG("TOKEN => ", ast->kind);

			if (ast->kind == ZEND_AST_STMT_LIST) {
				DBG("ZEND_AST_STMT_LIST");
				zend_ast_list *list = zend_ast_get_list(ast);

				for (uint32_t i = 0; i < list->children; ++i) {
					DBG("AST KIND FROM LIST ", list->child[i]->kind);
					ast_traverse_root(list->child[i]);
				}
			}
			if (ast->kind == ZEND_AST_ASSIGN) {
				DBG("ZEND_AST_ASSIGN");
				ast_assign_variable(ast);
			}

			if (ast->kind == ZEND_AST_FUNC_DECL) {
				ast_traverse_func_decl(ast);
			}
		}

	public:
		template<typename... DebugValue>
		void DBG(DebugValue... dbg_value) {
			if (debug_on) {
				std::cout << "[DEBUG] ";
				((std::cout << dbg_value), ...);
				std::cout << std::endl;
			}
		}

		void build_ir_to_executable(const std::string& out_path) {
			std::ostringstream oss_ir;
			oss_ir << out_path << ".ll";
			std::error_code EC;
			llvm::raw_fd_ostream dest(oss_ir.str(), EC);
			if (EC) {
				llvm::errs() << "Could not open file: " << EC.message() << "\n";
				return;
			}
			llvm_module->print(dest, nullptr);

			std::ostringstream oss;
			oss << "clang" << " " << oss_ir.str() << " -o" << out_path << ".exe";
			int result = system(oss.str().c_str());

			if (result == -1) {
				std::cerr << "Error: Unable to execute command\n";
			} else {
				std::cout << "Command exited with status: " << WEXITSTATUS(result) << "\n";
			}
		}

		char* zend_compile_single_typename_to_string(zend_ast *ast) {
			ZEND_ASSERT(!(ast->attr & ZEND_TYPE_NULLABLE));

			zend_string *class_name = zend_ast_get_str(ast);
			DBG("TYPE: ", class_name->val, ast->attr);
			return class_name->val;
		}

		llvm::Type *zend_eval_llvm_type(const std::string& type_val_label) {
			if (type_val_label == "int") {
				return llvm::Type::getInt32Ty(llvm_ctx);
			}
			if (type_val_label == "bool") {
				return llvm::Type::getInt1Ty(llvm_ctx);
			}
			if (type_val_label == "string") {
				return nullptr;
			}
			return nullptr;
		}

		int compile_ast(zend_ast *ast) {
			ast_traverse_root(ast);
			llvm_module->print(llvm::errs(), nullptr);
			return 0;
		}

		explicit LLVMCompiler(const std::string& module_name) {
			/*
			 * Every file while compiled indipendently than will be linked later
			 * will be dependencies registry later to determine how to link
			 */
			llvm_module = new llvm::Module(module_name, llvm_ctx);
			llvm_builder = new llvm::IRBuilder<>(llvm_ctx);
		}
};





void compile_ast_to_native(zend_ast *ast, char* out_path) {
    printf("COMPILING USING LLVM\n");
	auto ns = "main";

	LLVMCompiler llvm_compiler(ns);
	llvm_compiler.compile_ast(ast);

}
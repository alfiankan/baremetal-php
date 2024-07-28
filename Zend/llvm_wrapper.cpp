#include <zend_ast.h>

extern "C" {
    #include "llvm_wrapper.h"
}

#include "zend_types.h"
#include <cstdlib>
#include <sstream>
#include <iostream>

void build_ir_to_executable(LLVMModuleRef *module, char* out_path) {
    std::ostringstream oss_ir;
    oss_ir << out_path << ".ll";
    LLVMPrintModuleToFile(*module, oss_ir.str().c_str(), NULL);

    std::ostringstream oss;
    oss << "clang" << " " << oss_ir.str() << " -o" << out_path << ".exe";
    int result = system(oss.str().c_str());

    if (result == -1) {
        std::cerr << "Error: Unable to execute command\n";
    } else {
        std::cout << "Command exited with status: " << WEXITSTATUS(result) << "\n";
    }
}

void compile_ast_to_native(zend_ast *ast, char* out_path) {
    printf("COMPILING USING LLVM\n");
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  module = LLVMModuleCreateWithNameInContext("hello", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMTypeRef int_32_type = LLVMInt32TypeInContext(context);

    LLVMTypeRef  main_function_type = LLVMFunctionType(int_32_type, NULL, 0, false);
    LLVMValueRef main_function = LLVMAddFunction(module, "main", main_function_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_function, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMBuildRet(builder, LLVMConstInt(int_32_type, 0, false));

    build_ir_to_executable(&module, out_path);

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
}
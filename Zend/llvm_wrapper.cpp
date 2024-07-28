#include <zend_ast.h>

extern "C" {
    #include "llvm_wrapper.h"
}

#include "zend_types.h"

void compile_ast_to_native(zend_ast *ast, char* out_path) {
    printf("COMPILING USING LLVM\n");
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  module = LLVMModuleCreateWithNameInContext("hello", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMTypeRef int_32_type = LLVMInt32TypeInContext(context);

    LLVMTypeRef  main_function_type = LLVMFunctionType(int_32_type, NULL, 0, false);
    LLVMValueRef main_function = LLVMAddFunction(module, "main", main_function_type);
    LLVMPrintModuleToFile(module, "hello.ll", NULL);
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
}
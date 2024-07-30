// llvm_wrapper.h
#ifndef LLVM_WRAPPER_H
#define LLVM_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zend_types.h"

    void compile_ast_to_native(zend_ast *ast, char* out_path);


#ifdef __cplusplus
}
#endif

#endif // LLVM_WRAPPER_H

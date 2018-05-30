#pragma once

void InitLLVM();
void ShutdownLLVM();

struct LLVMFunction;

LLVMFunction *CompileFunction(const uint32_t *pCode, size_t codeSize);
Shader GetFuncPointer(LLVMFunction *func, const char *name);
void DestroyFunction(LLVMFunction *func);
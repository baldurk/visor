#include "precompiled.h"
#include "spirv_compile.h"
#include <set>
#include "3rdparty/GLSL.std.450.h"
#include "3rdparty/spirv.hpp"
#include "gpu.h"

#pragma warning(push)
#pragma warning(disable : 4141)
#pragma warning(disable : 4146)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#pragma warning(disable : 4291)
#pragma warning(disable : 4624)
#pragma warning(disable : 4800)

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#pragma warning(pop)

typedef llvm::orc::RTDyldObjectLinkingLayer Linker;

#define DEBUG_SPIRV 0

namespace
{
llvm::LLVMContext *context = NULL;
llvm::SectionMemoryManager *memManager = NULL;

llvm::TargetMachine *target = NULL;

Linker *globallinker = NULL;
llvm::orc::SimpleCompiler *compiler = NULL;

llvm::Module *globalFunctions = NULL;

llvm::Type *t_VkImage = NULL;
llvm::Type *t_VkBuffer = NULL;
llvm::Type *t_VkDescriptorSet = NULL;
llvm::Type *t_VkPipeline = NULL;

llvm::Type *t_GPUStateRef = NULL;
llvm::Type *t_VertexCacheEntry = NULL;

llvm::Type *t_VertexShader = NULL;
llvm::Type *t_FragmentShader = NULL;
};

struct LLVMFunction
{
  ~LLVMFunction()
  {
    delete linker;
    delete module;
  }
  Linker *linker = NULL;
  llvm::Module *module = NULL;
  std::string ir, optir;
};

static void llvm_fatal(void *user_data, const std::string &reason, bool gen_crash_diag)
{
  assert(false);
}

static Linker *makeLinker()
{
  return new Linker([] {
    return std::shared_ptr<llvm::SectionMemoryManager>(memManager,
                                                       [](llvm::SectionMemoryManager *) {});
  });
}

static void DeclareGlobalFunctions(llvm::Module *m)
{
  using namespace llvm;

  LLVMContext &c = *context;

  Function::Create(FunctionType::get(Type::getVoidTy(c),
                                     {
                                         // uint32_t pCode0
                                         Type::getInt32Ty(c),
                                         // uint32_t pCode1
                                         Type::getInt32Ty(c),
                                         // uint32_t pCode2
                                         Type::getInt32Ty(c),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "DebugSPIRVOp", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float4 mat[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 vec,
                            VectorType::get(Type::getFloatTy(c), 4),
                            // float4 &result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Float4x4TimesVec4", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float3 mat[3],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 3), 0),
                            // float3 vec,
                            VectorType::get(Type::getFloatTy(c), 3),
                            // float3 &result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 3), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Float3x3TimesVec3", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float4 mat[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 vec,
                            VectorType::get(Type::getFloatTy(c), 4),
                            // float4 &result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Vec4TimesFloat4x4", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float3 mat[3],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 3), 0),
                            // float3 vec,
                            VectorType::get(Type::getFloatTy(c), 3),
                            // float3 &result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 3), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Vec3TimesFloat3x3", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float4 a[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 b[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 *result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Float4x4TimesFloat4x4", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float4 in[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 *result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Float4x4Transpose", m);

  Function::Create(
      FunctionType::get(Type::getVoidTy(c),
                        {
                            // float4 in[4],
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                            // float4 *result,
                            PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                        },
                        false),
      GlobalValue::ExternalLinkage, "Float4x4Inverse", m);

  Function::Create(FunctionType::get(PointerType::get(Type::getInt8Ty(c), 0),
                                     {
                                         t_GPUStateRef, Type::getInt32Ty(c), Type::getInt32Ty(c),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "GetDescriptorBufferPointer", m);

  Function::Create(FunctionType::get(PointerType::get(Type::getVoidTy(c), 0),
                                     {
                                         t_GPUStateRef, Type::getInt32Ty(c), Type::getInt32Ty(c),
                                         PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "GetVertexAttributeData", m);

  Function::Create(FunctionType::get(PointerType::get(Type::getInt8Ty(c), 0),
                                     {
                                         t_GPUStateRef, Type::getInt32Ty(c),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "GetPushConstantPointer", m);

  Function::Create(FunctionType::get(t_VkImage,
                                     {
                                         t_GPUStateRef, Type::getInt32Ty(c), Type::getInt32Ty(c),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "GetDescriptorImage", m);

  Function::Create(FunctionType::get(Type::getVoidTy(c),
                                     {
                                         // float u
                                         Type::getFloatTy(c),
                                         // float v
                                         Type::getFloatTy(c),
                                         // VkImage tex
                                         t_VkImage,
                                         // VkDeviceSize byteOffs
                                         Type::getInt64Ty(c),
                                         // float4 &out
                                         PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "sample_tex_wrapped", m);

  Function::Create(FunctionType::get(Type::getVoidTy(c),
                                     {
                                         // float x
                                         Type::getFloatTy(c),
                                         // float y
                                         Type::getFloatTy(c),
                                         // float z
                                         Type::getFloatTy(c),
                                         // VkImage tex
                                         t_VkImage,
                                         // float4 &out
                                         PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "sample_cube_wrapped", m);

  Function::Create(FunctionType::get(Type::getVoidTy(c),
                                     {
                                         // float x
                                         Type::getFloatTy(c),
                                         // float y
                                         Type::getFloatTy(c),
                                         // float z
                                         Type::getFloatTy(c),
                                         // VkImage tex
                                         t_VkImage,
                                         // float4 &out
                                         PointerType::get(VectorType::get(Type::getFloatTy(c), 4), 0),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "sample_cube_wrapped", m);
}

void InitLLVM()
{
  using namespace llvm;

  install_fatal_error_handler(&llvm_fatal);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  context = new LLVMContext();

  memManager = new llvm::SectionMemoryManager();

  target = EngineBuilder().selectTarget();
  globallinker = makeLinker();
  compiler = new llvm::orc::SimpleCompiler(*target);

  std::string str;
  sys::DynamicLibrary::LoadLibraryPermanently(NULL, &str);

  LLVMContext &c = *context;

  t_VkImage = PointerType::get(StructType::create(c, "VkImage"), 0);
  t_VkBuffer = PointerType::get(StructType::create(c, "VkBuffer"), 0);
  t_VkDescriptorSet = PointerType::get(StructType::create(c, "VkDescriptorSet"), 0);
  t_VkPipeline = PointerType::get(StructType::create(c, "VkPipeline"), 0);

  t_GPUStateRef = PointerType::get(StructType::create(c, "GPUState"), 0);

  Type *t_float4 = VectorType::get(Type::getFloatTy(c), 4);

  t_VertexCacheEntry = StructType::get(c,
                                       {
                                           // float4 position
                                           t_float4,
                                           // float4 interps[8]
                                           ArrayType::get(t_float4, 8),
                                       },
                                       "VertexCacheEntry");

  t_VertexShader = FunctionType::get(Type::getVoidTy(c),
                                     {
                                         // const GPUState &state
                                         t_GPUStateRef,
                                         // uint32_t vertexIndex
                                         Type::getInt32Ty(c),
                                         // VertexCacheEntry &out
                                         PointerType::get(t_VertexCacheEntry, 0),
                                     },
                                     false);
  t_FragmentShader = FunctionType::get(Type::getVoidTy(c),
                                       {
                                           // const GPUState &state
                                           t_GPUStateRef,
                                           // float pixdepth
                                           Type::getFloatTy(c),
                                           // const float4 &bary
                                           PointerType::get(t_float4, 0),
                                           // const VertexCacheEntry tri[3]
                                           PointerType::get(t_VertexCacheEntry, 0),
                                           // float4 &out
                                           PointerType::get(t_float4, 0),
                                       },
                                       false);

  globalFunctions = new llvm::Module("globalFunctions", c);

  DeclareGlobalFunctions(globalFunctions);

  IRBuilder<> builder(c, ConstantFolder());

  std::vector<Function *> functions;

  // we could define library functions in LLVM here instead of C to allow it to inline/optimise

  {
    std::string str;
    raw_string_ostream os(str);

    if(verifyModule(*globalFunctions, &os))
    {
      printf("Global Module did not verify: %s\n", os.str().c_str());
      assert(false);
    }
  }

  {
    std::string str;
    raw_string_ostream os(str);

    globalFunctions->print(os, NULL);
  }

  PassManagerBuilder pm_builder;
  pm_builder.OptLevel = 3;
  pm_builder.SizeLevel = 0;
  pm_builder.LoopVectorize = true;
  pm_builder.SLPVectorize = true;

  legacy::FunctionPassManager function_pm(globalFunctions);
  legacy::PassManager module_pm;

  pm_builder.populateFunctionPassManager(function_pm);
  pm_builder.populateModulePassManager(module_pm);

  function_pm.doInitialization();
  for(Function *f : functions)
    function_pm.run(*f);
  module_pm.run(*globalFunctions);

  {
    std::string str;
    raw_string_ostream os(str);

    globalFunctions->print(os, NULL);
  }

  globalFunctions->setDataLayout(target->createDataLayout());

  auto symbolResolver = llvm::orc::createLambdaResolver(
      [](const std::string &name) { return globallinker->findSymbol(name, false); },
      [](const std::string &name) {
        uint64_t sym_addr = RTDyldMemoryManager::getSymbolAddressInProcess(name);
        if(sym_addr)
          return JITSymbol(sym_addr, JITSymbolFlags::Exported);

        return JITSymbol(NULL);
      });

  orc::SimpleCompiler &comp = *compiler;

  globallinker->addObject(
      std::make_shared<orc::SimpleCompiler::CompileResult>(comp(*globalFunctions)), symbolResolver);
}

void ShutdownLLVM()
{
  delete globalFunctions;
  delete compiler;
  delete globallinker;
  delete target;
  delete memManager;
  delete context;
}

extern "C" __declspec(dllexport) void DebugSPIRVOp(uint32_t pCode0, uint32_t pCode1, uint32_t pCode2)
{
#if DEBUG_SPIRV
  spv::Op opcode = spv::Op(pCode0 & spv::OpCodeMask);

  uint16_t WordCount = pCode0 >> spv::WordCountShift;
#endif
}

extern "C" __declspec(dllexport) void Float4x4TimesVec4(float *fmat, const float4 &vec, float4 &out)
{
  for(int row = 0; row < 4; row++)
  {
    out.v[row] = 0.0f;
    for(int col = 0; col < 4; col++)
      out.v[row] += fmat[col * 4 + row] * vec.v[col];
  }
}

extern "C" __declspec(dllexport) void Float3x3TimesVec3(float *fmat, const float4 &vec, float4 &out)
{
  for(int row = 0; row < 3; row++)
  {
    out.v[row] = 0.0f;
    for(int col = 0; col < 3; col++)
      out.v[row] += fmat[col * 4 + row] * vec.v[col];
  }
}

extern "C" __declspec(dllexport) void Vec4TimesFloat4x4(float *fmat, const float4 &vec, float4 &out)
{
  for(int row = 0; row < 4; row++)
  {
    out.v[row] = 0.0f;
    for(int col = 0; col < 4; col++)
      out.v[row] += fmat[row * 4 + col] * vec.v[col];
  }
}

extern "C" __declspec(dllexport) void Vec3TimesFloat3x3(float *fmat, const float4 &vec, float4 &out)
{
  for(int row = 0; row < 3; row++)
  {
    out.v[row] = 0.0f;
    for(int col = 0; col < 3; col++)
      out.v[row] += fmat[row * 4 + col] * vec.v[col];
  }
}

extern "C" __declspec(dllexport) void Float4x4TimesFloat4x4(const float *a, const float *b, float *out)
{
  for(size_t x = 0; x < 4; x++)
  {
    for(size_t y = 0; y < 4; y++)
    {
      out[x * 4 + y] = b[x * 4 + 0] * a[0 * 4 + y] + b[x * 4 + 1] * a[1 * 4 + y] +
                       b[x * 4 + 2] * a[2 * 4 + y] + b[x * 4 + 3] * a[3 * 4 + y];
    }
  }
}

extern "C" __declspec(dllexport) void Float4x4Transpose(const float *in, float *out)
{
  for(size_t x = 0; x < 4; x++)
    for(size_t y = 0; y < 4; y++)
      out[x * 4 + y] = in[y * 4 + x];
}

extern "C" __declspec(dllexport) void Float4x4Inverse(const float *in, float *out)
{
  float a0 = in[0] * in[5] - in[1] * in[4];
  float a1 = in[0] * in[6] - in[2] * in[4];
  float a2 = in[0] * in[7] - in[3] * in[4];
  float a3 = in[1] * in[6] - in[2] * in[5];
  float a4 = in[1] * in[7] - in[3] * in[5];
  float a5 = in[2] * in[7] - in[3] * in[6];
  float b0 = in[8] * in[13] - in[9] * in[12];
  float b1 = in[8] * in[14] - in[10] * in[12];
  float b2 = in[8] * in[15] - in[11] * in[12];
  float b3 = in[9] * in[14] - in[10] * in[13];
  float b4 = in[9] * in[15] - in[11] * in[13];
  float b5 = in[10] * in[15] - in[11] * in[14];

  float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
  if(fabsf(det) > FLT_EPSILON)
  {
    out[0] = +in[5] * b5 - in[6] * b4 + in[7] * b3;
    out[4] = -in[4] * b5 + in[6] * b2 - in[7] * b1;
    out[8] = +in[4] * b4 - in[5] * b2 + in[7] * b0;
    out[12] = -in[4] * b3 + in[5] * b1 - in[6] * b0;
    out[1] = -in[1] * b5 + in[2] * b4 - in[3] * b3;
    out[5] = +in[0] * b5 - in[2] * b2 + in[3] * b1;
    out[9] = -in[0] * b4 + in[1] * b2 - in[3] * b0;
    out[13] = +in[0] * b3 - in[1] * b1 + in[2] * b0;
    out[2] = +in[13] * a5 - in[14] * a4 + in[15] * a3;
    out[6] = -in[12] * a5 + in[14] * a2 - in[15] * a1;
    out[10] = +in[12] * a4 - in[13] * a2 + in[15] * a0;
    out[14] = -in[12] * a3 + in[13] * a1 - in[14] * a0;
    out[3] = -in[9] * a5 + in[10] * a4 - in[11] * a3;
    out[7] = +in[8] * a5 - in[10] * a2 + in[11] * a1;
    out[11] = -in[8] * a4 + in[9] * a2 - in[11] * a0;
    out[15] = +in[8] * a3 - in[9] * a1 + in[10] * a0;

    float invDet = 1.0f / det;
    out[0] *= invDet;
    out[1] *= invDet;
    out[2] *= invDet;
    out[3] *= invDet;
    out[4] *= invDet;
    out[5] *= invDet;
    out[6] *= invDet;
    out[7] *= invDet;
    out[8] *= invDet;
    out[9] *= invDet;
    out[10] *= invDet;
    out[11] *= invDet;
    out[12] *= invDet;
    out[13] *= invDet;
    out[14] *= invDet;
    out[15] *= invDet;

    return;
  }

  memset(out, 0, sizeof(float) * 16);
}

extern "C" __declspec(dllexport) const byte *GetDescriptorBufferPointer(const GPUState &state,
                                                                        uint32_t set, uint32_t bind)
{
  const VkDescriptorBufferInfo &buf = state.sets[set]->binds[bind].data.bufferInfo;

  return buf.buffer->bytes + buf.offset;
}

extern "C" __declspec(dllexport) VkImage
    GetDescriptorImage(const GPUState &state, uint32_t set, uint32_t bind)
{
  return state.sets[set]->binds[bind].data.imageInfo.imageView->image;
}

extern "C" __declspec(dllexport) const byte *GetPushConstantPointer(const GPUState &state,
                                                                    uint32_t offset)
{
  return state.pushconsts + offset;
}

extern "C" __declspec(dllexport) void GetVertexAttributeData(const GPUState &state,
                                                             uint32_t vertexIndex, uint32_t attr,
                                                             float4 &out)
{
  out = float4(0, 0, 0, 1);

  uint32_t vb = state.pipeline->vattrs[attr].vb;
  byte *ptr = state.vbs[vb].buffer->bytes + state.vbs[vb].offset;

  ptr += state.pipeline->vattrs[attr].offset;
  ptr += state.pipeline->vattrs[attr].stride * vertexIndex;

  float *f32 = (float *)ptr;
  uint32_t *u32 = (uint32_t *)ptr;

  switch(state.pipeline->vattrs[attr].format)
  {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    {
      out.w = f32[3];
      // deliberate fallthrough
    }
    case VK_FORMAT_R32G32B32_SFLOAT:
    {
      out.z = f32[2];
      // deliberate fallthrough
    }
    case VK_FORMAT_R32G32_SFLOAT:
    {
      out.y = f32[1];
      // deliberate fallthrough
    }
    case VK_FORMAT_R32_SFLOAT:
    {
      out.x = f32[0];
      break;
    }
    case VK_FORMAT_R8G8B8A8_UNORM:
    {
      out.x = float((u32[0] & 0x000000ff) >> 0x00) / 255.0f;
      out.y = float((u32[0] & 0x0000ff00) >> 0x08) / 255.0f;
      out.z = float((u32[0] & 0x00ff0000) >> 0x10) / 255.0f;
      out.w = float((u32[0] & 0xff000000) >> 0x18) / 255.0f;
      break;
    }
    default: assert(false && "Unhandled vertex attribute format");
  }
}

llvm::Value *CreateDot(llvm::IRBuilder<> &builder, llvm::Value *a, llvm::Value *b, int numcomps)
{
  llvm::Value *accum = builder.CreateFMul(builder.CreateExtractElement(a, 0LLU),
                                          builder.CreateExtractElement(b, 0LLU));
  if(numcomps > 1)
    accum = builder.CreateFAdd(accum, builder.CreateFMul(builder.CreateExtractElement(a, 1LLU),
                                                         builder.CreateExtractElement(b, 1LLU)));
  if(numcomps > 2)
    accum = builder.CreateFAdd(accum, builder.CreateFMul(builder.CreateExtractElement(a, 2LLU),
                                                         builder.CreateExtractElement(b, 2LLU)));
  if(numcomps > 3)
    accum = builder.CreateFAdd(accum, builder.CreateFMul(builder.CreateExtractElement(a, 3LLU),
                                                         builder.CreateExtractElement(b, 3LLU)));
  return accum;
}

LLVMFunction *CompileFunction(const uint32_t *pCode, size_t codeSize)
{
  using namespace llvm;

  LLVMContext &c = *context;

  assert(pCode[0] == spv::MagicNumber);
  assert(pCode[1] <= spv::Version);

  const uint32_t generator = pCode[2];
  const uint32_t idbound = pCode[3];

  assert(pCode[4] == 0);

  LLVMFunction *ret = new LLVMFunction();

  char uniq_name[512];
  sprintf_s(uniq_name, "visormodule_%p", ret);

  ret->module = new Module(uniq_name, c);
  ret->linker = makeLinker();

  Module *m = ret->module;

  DeclareGlobalFunctions(m);

  std::vector<Function *> functions;
  Function *curfunc = NULL;
  BasicBlock *curlabel = NULL;
  Argument *functionargs = NULL;

  IRBuilder<> builder(c, ConstantFolder());

  std::vector<const uint32_t *> entries;

  struct IDDecoration
  {
    uint32_t id;
    spv::Decoration dec;
    uint32_t param;
    uint32_t member;

    bool operator<(const IDDecoration &o) const { return id < o.id; }
  };
  std::vector<IDDecoration> decorations;
  std::set<uint32_t> blocks, cube;

  // in lieu of a proper SPIR-V representation, this lets us go from pointer type -> struct type
  std::map<uint32_t, uint32_t> ptrtypes;

  std::vector<Value *> values;
  std::vector<Type *> types;
  std::map<uint32_t, std::string> names;

  uint32_t GLSLstd450_instset = 0;

  values.resize(idbound);
  types.resize(idbound);

  auto getname = [&names](uint32_t id) -> std::string {
    std::string name = names[id];
    if(name.empty())
      name = "spv__" + std::to_string(id);
    return name;
  };

  std::vector<Type *> globalTypes;

  struct FunctionTypeInfo
  {
    std::vector<Type *> params;
    Type *ret;
  };
  std::map<uint32_t, FunctionTypeInfo> functypes;

  struct ExternalBinding
  {
    spv::StorageClass storageClass;
    Value *value;
    IDDecoration decoration;
  };
  std::vector<ExternalBinding> externals;

  const uint32_t *opStart = pCode + 5;
  const uint32_t *opEnd = pCode + codeSize;

  // we do three passes to make things easier for instruction generation:
  // 1. Gather information:
  //    - Parse types and create corresponding LLVM types
  //    - Generate LLVM constants immediately
  //    - Store names and debug info locally
  //    - Gather decoration information locally
  //    - Gather types of globals locally, in order
  //
  // Generate global struct here.
  //
  // 2. Function-only pass. Find functions and create them (so that if we encounter a function call
  //    going forward we already have the Function* ready for it). Within each function, create all
  //    basic blocks (again so that forward branches already have the blocks ready).
  //
  // 2. Process and generate LLVM functions and globals
  //
  // After this we generate wrapper entry points that fetch and populate globals, but that doesn't
  // iterate over SPIR-V it just uses data gathered before.

  pCode = opStart;
  while(pCode < opEnd)
  {
    uint16_t WordCount = pCode[0] >> spv::WordCountShift;

    spv::Op opcode = spv::Op(pCode[0] & spv::OpCodeMask);

    switch(opcode)
    {
      ////////////////////////////////////////////////
      // Debug instructions
      ////////////////////////////////////////////////

      case spv::OpName:
      {
        names[pCode[1]] = (const char *)&pCode[2];
        break;
      }

      ////////////////////////////////////////////////
      // Globals/declarations/decorations
      ////////////////////////////////////////////////

      case spv::OpExtInstImport:
      {
        GLSLstd450_instset = pCode[1];
        assert(!strcmp((char *)(pCode + 2), "GLSL.std.450"));
        break;
      }
      case spv::OpEntryPoint:
      {
        // we'll do this in a final pass once we have the function resolved
        entries.push_back(pCode);
        break;
      }
      case spv::OpDecorate:
      {
        IDDecoration search = {pCode[1]};
        auto it = std::lower_bound(decorations.begin(), decorations.end(), search);
        if(WordCount == 3)
        {
          decorations.insert(it, {pCode[1], (spv::Decoration)pCode[2], 0, ~0U});

          if(pCode[2] == spv::DecorationBlock)
            blocks.insert(pCode[1]);
        }
        else
        {
          decorations.insert(it, {pCode[1], (spv::Decoration)pCode[2], pCode[3], ~0U});
        }

        break;
      }
      case spv::OpMemberDecorate:
      {
        IDDecoration search = {pCode[1]};
        auto it = std::lower_bound(decorations.begin(), decorations.end(), search);

        if(WordCount == 4)
          decorations.insert(it, {pCode[1], (spv::Decoration)pCode[3], 0, pCode[2]});
        else
          decorations.insert(it, {pCode[1], (spv::Decoration)pCode[3], pCode[4], pCode[2]});
        break;
      }

      ////////////////////////////////////////////////
      // Type declarations
      ////////////////////////////////////////////////

      case spv::OpTypeVoid:
      {
        types[pCode[1]] = Type::getVoidTy(c);
        break;
      }
      case spv::OpTypeFloat:
      {
        if(pCode[2] == 16)
          types[pCode[1]] = Type::getHalfTy(c);
        else if(pCode[2] == 32)
          types[pCode[1]] = Type::getFloatTy(c);
        else if(pCode[2] == 64)
          types[pCode[1]] = Type::getDoubleTy(c);
        break;
      }
      case spv::OpTypeBool:
      {
        types[pCode[1]] = Type::getInt1Ty(c);
        break;
      }
      case spv::OpTypeInt:
      {
        types[pCode[1]] = IntegerType::get(c, pCode[2]);
        // ignore pCode[3] signedness
        break;
      }
      case spv::OpTypeVector:
      {
        types[pCode[1]] = VectorType::get(types[pCode[2]], pCode[3]);
        break;
      }
      case spv::OpTypeArray:
      {
        types[pCode[1]] = ArrayType::get(
            types[pCode[2]], ((ConstantInt *)values[pCode[3]])->getValue().getLimitedValue());
        break;
      }
      case spv::OpTypeMatrix:
      {
        // implement matrix as just array
        types[pCode[1]] = ArrayType::get(types[pCode[2]], pCode[3]);
        break;
      }
      case spv::OpTypePointer:
      {
        types[pCode[1]] = PointerType::get(types[pCode[3]], 0);

        // if the pointed type is a block and this is a uniform pointer, propagate that to the
        // pointer
        if(blocks.find(pCode[3]) != blocks.end() &&
           (pCode[2] == spv::StorageClassUniform || pCode[2] == spv::StorageClassPushConstant))
          blocks.insert(pCode[1]);

        ptrtypes[pCode[1]] = pCode[3];
        break;
      }
      case spv::OpTypeStruct:
      {
        std::vector<Type *> members;
        for(uint16_t i = 2; i < WordCount; i++)
          members.push_back(types[pCode[i]]);
        types[pCode[1]] = StructType::create(c, members, getname(pCode[1]));
        break;
      }
      case spv::OpTypeFunction:
      {
        // to patch in the globals struct, we declare OpTypeFunction the first time we meet an
        // OpFunction (after any global variables - in the second pass).
        FunctionTypeInfo &f = functypes[pCode[1]];
        for(uint16_t i = 3; i < WordCount; i++)
          f.params.push_back(types[pCode[i]]);
        f.ret = types[pCode[2]];
        break;
      }
      case spv::OpTypeImage:
      case spv::OpTypeSampledImage:
      {
        if(opcode == spv::OpTypeImage && pCode[3] == spv::DimCube)
          cube.insert(pCode[1]);
        else if(opcode == spv::OpTypeSampledImage && cube.find(pCode[2]) != cube.end())
          cube.insert(pCode[1]);

        // TODO proper image handling
        types[pCode[1]] = t_VkImage;
        break;
      }

      ////////////////////////////////////////////////
      // Variables and Constants
      ////////////////////////////////////////////////

      case spv::OpVariable:
      {
        // global variable
        Type *t = types[pCode[1]];
        assert(isa<PointerType>(t));

        bool blockbind = (blocks.find(pCode[1]) != blocks.end());

        // for blocks we declare the global variable as a pointer to a struct (and the variable
        // itself is a pointer), this lets us just write the pointer value of where the buffer is
        // without needing to copy the whole UBO.
        // for non-blocks, e.g. int gl_PerVertex we take the "pointer to int" that the SPIR-V
        // declares and only declare the global variable as an int - and as above the variable
        // itself is a pointer. The value then gets stored/loaded directly
        if(!blockbind)
          t = t->getPointerElementType();
        else
          blocks.insert(pCode[2]);

        globalTypes.push_back(t);

        // initialisers not handled
        assert(WordCount == 4);
        break;
      }
      case spv::OpConstant:
      {
        Type *t = types[pCode[1]];
        if(t->isFloatTy())
          values[pCode[2]] = ConstantFP::get(t, ((float *)pCode)[3]);
        else
          values[pCode[2]] = ConstantInt::get(t, pCode[3]);
        break;
      }
      case spv::OpConstantComposite:
      {
        Type *t = types[pCode[1]];
        assert(t->isVectorTy());
        std::vector<Constant *> members;
        for(uint16_t i = 3; i < WordCount; i++)
          members.push_back((Constant *)values[pCode[i]]);
        values[pCode[2]] = ConstantVector::get(members);
        break;
      }
    }

    // stop once we hit the functions
    if(opcode == spv::OpFunction)
      break;

    pCode += WordCount;
  }

  // declare globals struct here
  Type *globalStructType = StructType::create(globalTypes, "GlobalsStruct");

  for(auto it = functypes.begin(); it != functypes.end(); ++it)
  {
    // all functions take the globals struct as the first parameter
    it->second.params.insert(it->second.params.begin(), PointerType::get(globalStructType, 0));
  }

  std::map<uint32_t, Function *> functiondefs;
  std::map<uint32_t, BasicBlock *> labels;

  pCode = opStart;
  while(pCode < opEnd)
  {
    uint16_t WordCount = pCode[0] >> spv::WordCountShift;

    spv::Op opcode = spv::Op(pCode[0] & spv::OpCodeMask);

    switch(opcode)
    {
      case spv::OpFunction:
      {
        sprintf_s(uniq_name, "%p_%d", ret, pCode[2]);

        if(types[pCode[4]] == NULL)
        {
          const FunctionTypeInfo &f = functypes[pCode[4]];
          types[pCode[4]] = FunctionType::get(f.ret, f.params, false);
        }

        curfunc = Function::Create((llvm::FunctionType *)types[pCode[4]],
                                   llvm::GlobalValue::InternalLinkage, uniq_name, m);

        functiondefs[pCode[2]] = curfunc;
        break;
      }
      case spv::OpFunctionEnd:
      {
        curfunc = NULL;
        break;
      }
      case spv::OpLabel:
      {
        sprintf_s(uniq_name, "%p_%d", ret, pCode[1]);

        assert(curfunc);
        labels[pCode[1]] = BasicBlock::Create(c, uniq_name, curfunc, NULL);
        break;
      }
    }

    pCode += WordCount;
  }

  uint64_t globalsIndex = 0;
  Value *globalStruct = NULL;

  auto makeglobal = [&globalsIndex]() -> Value * {
    Value *ret = (Value *)(0x1 + globalsIndex);
    globalsIndex++;
    return ret;
  };

  auto getvalue = [&builder, &values, globalStructType, &globalStruct,
                   &globalsIndex](Value *v) -> Value * {
    if(v > (Value *)globalsIndex)
      return v;

    uint64_t idx = uint64_t(v) - 0x1;

    return builder.CreateConstInBoundsGEP2_32(globalStructType, globalStruct, 0, (uint32_t)idx);
  };

  pCode = opStart;
  while(pCode < opEnd)
  {
    uint16_t WordCount = pCode[0] >> spv::WordCountShift;

    spv::Op opcode = spv::Op(pCode[0] & spv::OpCodeMask);

#if DEBUG_SPIRV
    if(curlabel)
    {
      builder.CreateCall(m->getFunction("DebugSPIRVOp"),
                         {
                             ConstantInt::get(Type::getInt32Ty(c), pCode[0]),
                             ConstantInt::get(Type::getInt32Ty(c), WordCount >= 2 ? pCode[1] : ~0U),
                             ConstantInt::get(Type::getInt32Ty(c), WordCount >= 3 ? pCode[2] : ~0U),
                         });
    }
#endif

    switch(opcode)
    {
      case spv::OpCapability:
      case spv::OpMemoryModel:
      case spv::OpExecutionMode:
      case spv::OpExtInstImport:
      case spv::OpSource:
      case spv::OpSourceExtension:
      case spv::OpMemberName:
      {
        // instructions we don't care about right now
        break;
      }

      case spv::OpName:
      case spv::OpEntryPoint:
      case spv::OpDecorate:
      case spv::OpMemberDecorate:
      case spv::OpConstantComposite:
      case spv::OpConstant:
      case spv::OpTypeVoid:
      case spv::OpTypeBool:
      case spv::OpTypeInt:
      case spv::OpTypeFloat:
      case spv::OpTypeVector:
      case spv::OpTypeArray:
      case spv::OpTypeMatrix:
      case spv::OpTypePointer:
      case spv::OpTypeStruct:
      case spv::OpTypeFunction:
      case spv::OpTypeImage:
      case spv::OpTypeSampledImage:
      {
        // Opcodes processed in first pass that we don't need
        break;
      }

      ////////////////////////////////////////////////
      // Variables and Constants
      ////////////////////////////////////////////////

      case spv::OpVariable:
      {
        if(curfunc)
        {
          assert(pCode[3] == spv::StorageClassFunction);
          values[pCode[2]] = builder.CreateAlloca(types[pCode[1]]->getPointerElementType(), NULL,
                                                  getname(pCode[2]));

          if(WordCount > 4)
            builder.CreateStore(values[pCode[4]], values[pCode[2]]);
        }
        else
        {
          // value is a 'virtual' value that indicates access to it must come through the globals
          // struct in the function
          values[pCode[2]] = makeglobal();

          bool blockbind = (blocks.find(pCode[1]) != blocks.end());

          IDDecoration search = {pCode[2]};
          auto it = std::lower_bound(decorations.begin(), decorations.end(), search);

          if(it->id != search.id)
          {
            search.id = ptrtypes[pCode[1]];
            it = std::lower_bound(decorations.begin(), decorations.end(), search);
          }

          if(pCode[3] <= spv::StorageClassOutput || pCode[3] == spv::StorageClassPushConstant)
          {
            for(; it != decorations.end() && it->id == search.id; ++it)
              externals.push_back({(spv::StorageClass)pCode[3], values[pCode[2]], *it});
          }
        }

        break;
      }

      ////////////////////////////////////////////////
      // Functions
      ////////////////////////////////////////////////

      case spv::OpFunction:
      {
        curfunc = functiondefs[pCode[2]];

        curfunc->addFnAttr(Attribute::AlwaysInline);

        // ignore function return type pCode[1]
        // ignore function control pCode[3]
        functions.push_back(curfunc);

        functionargs = curfunc->arg_begin();

        // globals is always the first implicit arg
        globalStruct = functionargs;
        functionargs++;

        values[pCode[2]] = curfunc;
        break;
      }
      case spv::OpFunctionParameter:
      {
        values[pCode[2]] = functionargs;
        functionargs++;
        break;
      }
      case spv::OpFunctionEnd:
      {
        assert(curfunc);

        std::string str;
        raw_string_ostream os(str);

        str.clear();
        if(verifyFunction(*curfunc, &os))
        {
          printf("Function did not verify: %s\n", os.str().c_str());
          assert(false);
        }

        curfunc = NULL;
        globalStruct = NULL;
        break;
      }

      ////////////////////////////////////////////////
      // Logic Instructions
      ////////////////////////////////////////////////

      case spv::OpFOrdLessThan:
      {
        values[pCode[2]] = builder.CreateFCmpOLT(values[pCode[3]], values[pCode[4]]);
        break;
      }
      case spv::OpFOrdGreaterThan:
      {
        values[pCode[2]] = builder.CreateFCmpOGT(values[pCode[3]], values[pCode[4]]);
        break;
      }

      ////////////////////////////////////////////////
      // Flow control Instructions
      ////////////////////////////////////////////////

      case spv::OpLabel:
      {
        assert(curfunc);
        builder.SetInsertPoint(labels[pCode[1]]);
        curlabel = labels[pCode[1]];
        break;
      }
      case spv::OpBranch:
      {
        builder.CreateBr(labels[pCode[1]]);
        curlabel = NULL;
        break;
      }
      case spv::OpSelectionMerge:
      {
        // don't need to do anything
        break;
      }
      case spv::OpBranchConditional:
      {
        builder.CreateCondBr(values[pCode[1]], labels[pCode[2]], labels[pCode[3]]);
        curlabel = NULL;
        break;
      }
      case spv::OpFunctionCall:
      {
        // always pass implicit global struct
        std::vector<Value *> args = {globalStruct};
        for(uint16_t i = 4; i < WordCount; i++)
          args.push_back(values[pCode[i]]);
        values[pCode[2]] = builder.CreateCall(functiondefs[pCode[3]], args);
        break;
      }
      case spv::OpReturn:
      {
        assert(curfunc);
        builder.CreateRetVoid();
        curlabel = NULL;
        break;
      }
      case spv::OpReturnValue:
      {
        assert(curfunc);
        builder.CreateRet(values[pCode[1]]);
        curlabel = NULL;
        break;
      }

      ////////////////////////////////////////////////
      // Memory access Instructions
      ////////////////////////////////////////////////

      case spv::OpLoad:
      {
        values[pCode[2]] = builder.CreateLoad(getvalue(values[pCode[3]]));
        // ignore memory access pCode[4]

        if(cube.find(pCode[1]) != cube.end())
          cube.insert(pCode[2]);

        break;
      }
      case spv::OpStore:
      {
        builder.CreateStore(values[pCode[2]], getvalue(values[pCode[1]]));
        // ignore memory access pCode[3]
        break;
      }
      case spv::OpAccessChain:
      {
        std::vector<Value *> indices = {ConstantInt::get(Type::getInt32Ty(c), 0)};
        for(uint16_t i = 4; i < WordCount; i++)
          indices.push_back(values[pCode[i]]);

        Value *base = getvalue(values[pCode[3]]);

        if(blocks.find(pCode[3]) != blocks.end())
        {
          // if this is a block pointer, we need to load the pointer value out of it then GEP that
          base = builder.CreateLoad(base);
        }

        values[pCode[2]] = builder.CreateGEP(base, indices);
        // ignore memory access pCode[3]
        break;
      }

      ////////////////////////////////////////////////
      // Maths Instructions
      ////////////////////////////////////////////////

      case spv::OpVectorTimesMatrix:
      case spv::OpMatrixTimesVector:
      {
        Value *mat = NULL, *vec = NULL;

        if(opcode == spv::OpMatrixTimesVector)
        {
          mat = values[pCode[3]];
          vec = values[pCode[4]];
        }
        else
        {
          vec = values[pCode[3]];
          mat = values[pCode[4]];
        }

        // create output value
        Value *retptr = builder.CreateAlloca(vec->getType());

        // create temporary array
        Value *arrayptr = builder.CreateAlloca(mat->getType());

        // fill temporary array with values
        for(unsigned i = 0; i < mat->getType()->getArrayNumElements(); i++)
        {
          builder.CreateStore(builder.CreateExtractValue(mat, {i}),
                              builder.CreateConstInBoundsGEP2_32(mat->getType(), arrayptr, 0, i));
        }

        unsigned vecsize = types[pCode[1]]->getVectorNumElements();

        // only support square matrix multiplies
        assert(vecsize == mat->getType()->getArrayNumElements());
        assert(vecsize == mat->getType()->getArrayElementType()->getVectorNumElements());

        Function *mulfunc = NULL;

        if(vecsize == 3)
        {
          mulfunc = m->getFunction("Float3x3TimesVec3");
          if(opcode == spv::OpVectorTimesMatrix)
            mulfunc = m->getFunction("Vec3TimesFloat3x3");
        }
        else if(vecsize == 4)
        {
          mulfunc = m->getFunction("Float4x4TimesVec4");
          if(opcode == spv::OpVectorTimesMatrix)
            mulfunc = m->getFunction("Vec4TimesFloat4x4");
        }

        assert(mulfunc);

        // call function
        builder.CreateCall(
            mulfunc,
            {
                builder.CreateConstInBoundsGEP2_32(mat->getType(), arrayptr, 0, 0), vec, retptr,
            });

        // load return value
        values[pCode[2]] = builder.CreateLoad(retptr);
        break;
      }
      case spv::OpMatrixTimesMatrix:
      {
        // create output value
        Value *retptr = builder.CreateAlloca(types[pCode[1]]);

        // create temporary arrays
        Value *aptr = builder.CreateAlloca(types[pCode[1]]);
        Value *bptr = builder.CreateAlloca(types[pCode[1]]);

        // fill temporary array with values
        for(unsigned i = 0; i < types[pCode[1]]->getArrayNumElements(); i++)
        {
          builder.CreateStore(builder.CreateExtractValue(values[pCode[3]], {i}),
                              builder.CreateConstInBoundsGEP2_32(types[pCode[1]], aptr, 0, i));
          builder.CreateStore(builder.CreateExtractValue(values[pCode[4]], {i}),
                              builder.CreateConstInBoundsGEP2_32(types[pCode[1]], bptr, 0, i));
        }

        // call function
        builder.CreateCall(m->getFunction("Float4x4TimesFloat4x4"),
                           {
                               builder.CreateConstInBoundsGEP2_32(types[pCode[1]], aptr, 0, 0),
                               builder.CreateConstInBoundsGEP2_32(types[pCode[1]], bptr, 0, 0),
                               builder.CreateConstInBoundsGEP2_32(types[pCode[1]], retptr, 0, 0),
                           });

        // load return value
        values[pCode[2]] = builder.CreateLoad(retptr);
        break;
      }
      case spv::OpTranspose:
      {
        // create output value
        Value *retptr = builder.CreateAlloca(types[pCode[1]]);

        // create temporary array
        Value *matptr = builder.CreateAlloca(types[pCode[1]]);

        // fill temporary array with values
        for(unsigned i = 0; i < types[pCode[1]]->getArrayNumElements(); i++)
        {
          builder.CreateStore(builder.CreateExtractValue(values[pCode[3]], {i}),
                              builder.CreateConstInBoundsGEP2_32(types[pCode[1]], matptr, 0, i));
        }

        // call function
        builder.CreateCall(m->getFunction("Float4x4Transpose"),
                           {
                               builder.CreateConstInBoundsGEP2_32(types[pCode[1]], matptr, 0, 0),
                               builder.CreateConstInBoundsGEP2_32(types[pCode[1]], retptr, 0, 0),
                           });

        // load return value
        values[pCode[2]] = builder.CreateLoad(retptr);
        break;
      }
      case spv::OpVectorTimesScalar:
      {
        Value *splat = builder.CreateVectorSplat(
            values[pCode[3]]->getType()->getVectorNumElements(), values[pCode[4]]);
        values[pCode[2]] = builder.CreateFMul(values[pCode[3]], splat);
        break;
      }
      case spv::OpFMul:
      {
        values[pCode[2]] = builder.CreateFMul(values[pCode[3]], values[pCode[4]]);
        break;
      }
      case spv::OpFAdd:
      {
        values[pCode[2]] = builder.CreateFAdd(values[pCode[3]], values[pCode[4]]);
        break;
      }
      case spv::OpFSub:
      {
        values[pCode[2]] = builder.CreateFSub(values[pCode[3]], values[pCode[4]]);
        break;
      }
      case spv::OpFNegate:
      {
        values[pCode[2]] = builder.CreateFNeg(values[pCode[3]]);
        break;
      }
      case spv::OpDPdx:
      {
        // TODO
        values[pCode[2]] = builder.CreateShuffleVector(values[pCode[3]], values[pCode[3]], {2, 3, 0});
        break;
      }
      case spv::OpDPdy:
      {
        // TODO
        values[pCode[2]] = builder.CreateShuffleVector(values[pCode[3]], values[pCode[3]], {1, 3, 2});
        break;
      }

#define ARG(n) (values[pCode[5 + n]])

      case spv::OpExtInst:
      {
        assert(pCode[3] == GLSLstd450_instset);

        GLSLstd450 inst = (GLSLstd450)pCode[4];

        switch(inst)
        {
          case GLSLstd450FMax:
          case GLSLstd450FMin:
          {
            assert(WordCount == 7);
            Value *a = ARG(0);
            Value *b = ARG(1);
            values[pCode[2]] = builder.CreateSelect(
                inst == GLSLstd450FMin ? builder.CreateFCmpOLT(a, b) : builder.CreateFCmpOGT(a, b),
                a, b);
            break;
          }
          case GLSLstd450FClamp:
          {
            assert(WordCount == 8);
            Value *val = ARG(0);
            Value *lowerBound = ARG(1);
            Value *upperBound = ARG(1);

            Value *upperClamped =
                builder.CreateSelect(builder.CreateFCmpOLT(val, upperBound), val, upperBound);

            values[pCode[2]] =
                builder.CreateSelect(builder.CreateFCmpOGT(val, lowerBound), val, lowerBound);

            break;
          }
          case GLSLstd450Normalize:
          {
            assert(WordCount == 6);
            Value *a = ARG(0);
            Value *sqrlen = CreateDot(builder, a, a, a->getType()->getVectorNumElements());
            Value *sqrt = Intrinsic::getDeclaration(m, Intrinsic::sqrt, {Type::getFloatTy(c)});
            Value *len = builder.CreateCall(sqrt, {sqrlen});
            Value *invlen = builder.CreateVectorSplat(
                a->getType()->getVectorNumElements(),
                builder.CreateFDiv(ConstantFP::get(Type::getFloatTy(c), 1.0), len));
            values[pCode[2]] = builder.CreateFMul(a, invlen);
            break;
          }
          case GLSLstd450Cross:
          {
            assert(WordCount == 7);
            // TODO
            values[pCode[2]] = ARG(0);
            break;
          }
          case GLSLstd450Pow:
          {
            assert(WordCount == 7);
            Value *x = ARG(0);
            Value *y = ARG(1);
            Value *pow = Intrinsic::getDeclaration(m, Intrinsic::pow, {Type::getFloatTy(c)});
            if(types[pCode[1]]->isVectorTy())
            {
              // vectors are calculated component-wise
              Value *vec = UndefValue::get(types[pCode[1]]);
              for(unsigned v = 0; v < types[pCode[1]]->getVectorNumElements(); v++)
              {
                Value *result = builder.CreateCall(
                    pow, {
                             builder.CreateExtractElement(x, v), builder.CreateExtractElement(y, v),
                         });
                vec = builder.CreateInsertElement(vec, result, v);
              }
              values[pCode[2]] = vec;
            }
            else
            {
              values[pCode[2]] = builder.CreateCall(pow, {x, y});
            }
            break;
          }
          case GLSLstd450Reflect:
          {
            assert(WordCount == 7);
            Value *I = ARG(0);
            Value *N = ARG(1);
            Value *NdotI = CreateDot(builder, I, N, I->getType()->getVectorNumElements());
            Value *NdotI2 = builder.CreateFMul(NdotI, ConstantFP::get(Type::getFloatTy(c), 2.0f));
            Value *splat = builder.CreateVectorSplat(I->getType()->getVectorNumElements(), NdotI2);
            Value *Nsplat = builder.CreateFMul(splat, N);
            // result = I - 2 * dot(N, I) * N
            values[pCode[2]] = builder.CreateFSub(I, Nsplat);
            break;
          }
          case GLSLstd450MatrixInverse:
          {
            assert(WordCount == 6);
            Value *mat = ARG(0);
            // create output value
            Value *retptr = builder.CreateAlloca(types[pCode[1]]);

            // create temporary array
            Value *matptr = builder.CreateAlloca(types[pCode[1]]);

            // fill temporary array with values
            for(unsigned i = 0; i < types[pCode[1]]->getArrayNumElements(); i++)
            {
              builder.CreateStore(builder.CreateExtractValue(mat, {i}),
                                  builder.CreateConstInBoundsGEP2_32(types[pCode[1]], matptr, 0, i));
            }

            // call function
            builder.CreateCall(m->getFunction("Float4x4Transpose"),
                               {
                                   builder.CreateConstInBoundsGEP2_32(types[pCode[1]], matptr, 0, 0),
                                   builder.CreateConstInBoundsGEP2_32(types[pCode[1]], retptr, 0, 0),
                               });

            // load return value
            values[pCode[2]] = builder.CreateLoad(retptr);
            break;
          }
          default:
          {
            values[pCode[2]] = UndefValue::get(types[pCode[1]]);
            assert(false && "Unhandled GLSL extended instruction");
            break;
          }
        }

        break;
      }

#undef ARG

      case spv::OpDot:
      {
        values[pCode[2]] = CreateDot(builder, values[pCode[3]], values[pCode[4]],
                                     values[pCode[3]]->getType()->getVectorNumElements());
        break;
      }

      ////////////////////////////////////////////////
      // Aggregate Instructions
      ////////////////////////////////////////////////

      case spv::OpCompositeExtract:
      {
        if(WordCount == 5)
        {
          values[pCode[2]] = builder.CreateExtractElement(values[pCode[3]], pCode[4]);
        }
        else if(WordCount == 6)
        {
          Value *subel = builder.CreateExtractValue(values[pCode[3]], {pCode[4]});
          values[pCode[2]] = builder.CreateExtractElement(subel, pCode[5]);
        }
        break;
      }
      case spv::OpCompositeConstruct:
      {
        Type *vecType = types[pCode[1]];
        values[pCode[2]] = UndefValue::get(vecType);
        for(uint16_t i = 0; i < WordCount - 3; i++)
        {
          // is matrix
          if(vecType->isArrayTy())
            values[pCode[2]] = builder.CreateInsertValue(values[pCode[2]], values[pCode[3 + i]], {i});
          else
            values[pCode[2]] = builder.CreateInsertElement(values[pCode[2]], values[pCode[3 + i]], i);
        }

        break;
      }
      case spv::OpVectorShuffle:
      {
        std::vector<uint32_t> indices;
        for(uint16_t i = 5; i < WordCount; i++)
          indices.push_back(pCode[i]);
        values[pCode[2]] = builder.CreateShuffleVector(values[pCode[3]], values[pCode[4]], indices);
        break;
      }

      ////////////////////////////////////////////////
      // Texture Instructions
      ////////////////////////////////////////////////

      case spv::OpImageSampleImplicitLod:
      {
        // TODO proper
        Value *retptr = builder.CreateAlloca(types[pCode[1]]);

        if(cube.find(pCode[3]) != cube.end())
        {
          // call function
          builder.CreateCall(m->getFunction("sample_cube_wrapped"),
                             {
                                 // x
                                 builder.CreateExtractElement(values[pCode[4]], 0ULL),
                                 // y
                                 builder.CreateExtractElement(values[pCode[4]], 1ULL),
                                 // z
                                 builder.CreateExtractElement(values[pCode[4]], 2ULL),
                                 // tex
                                 values[pCode[3]],
                                 // out
                                 retptr,
                             });
        }
        else
        {
          // call function
          builder.CreateCall(m->getFunction("sample_tex_wrapped"),
                             {
                                 // u
                                 builder.CreateExtractElement(values[pCode[4]], 0ULL),
                                 // v
                                 builder.CreateExtractElement(values[pCode[4]], 1ULL),
                                 // tex
                                 values[pCode[3]],
                                 // byteOffset
                                 ConstantInt::get(Type::getInt64Ty(c), 0),
                                 // out
                                 retptr,
                             });
        }

        // load return value
        values[pCode[2]] = builder.CreateLoad(retptr);

        break;
      }

      default: assert(false && "Unhandled SPIR-V opcode"); break;
    }

    pCode += WordCount;
  }

  for(const uint32_t *e : entries)
  {
    spv::ExecutionModel model = (spv::ExecutionModel)e[1];

    // name of the void entry() function we generated above
    sprintf_s(uniq_name, "%p_%s", ret, (char *)&e[3]);

    Function *f = (Function *)values[e[2]];

    Function *exportedEntry = NULL;

    std::map<uint32_t, uint32_t> descset;

    // cache descriptor sets
    for(const ExternalBinding &ext : externals)
      if(ext.decoration.dec == spv::DecorationDescriptorSet)
        descset[ext.decoration.id] = ext.decoration.param;

    if(model == spv::ExecutionModelVertex)
    {
      exportedEntry = Function::Create((FunctionType *)t_VertexShader, GlobalValue::ExternalLinkage,
                                       uniq_name, m);

      // mark reference parameters
      exportedEntry->addParamAttr(0, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(2, Attribute::Dereferenceable);

      BasicBlock *block = BasicBlock::Create(c, "block", exportedEntry, NULL);
      builder.SetInsertPoint(block);

      globalStruct = builder.CreateAlloca(globalStructType, 0, NULL, "Globals");

      Argument *gpustate = exportedEntry->arg_begin();
      Argument *vtxidx = gpustate + 1;
      Argument *cache = vtxidx + 1;

      for(const ExternalBinding &ext : externals)
      {
        if(ext.storageClass == spv::StorageClassOutput)
          continue;

        uint32_t id = ext.decoration.id;

        Value *val = getvalue(ext.value);

        if(ext.decoration.dec == spv::DecorationBuiltIn)
        {
          spv::BuiltIn b = (spv::BuiltIn)ext.decoration.param;
          if(b == spv::BuiltInVertexIndex || b == spv::BuiltInVertexId)
          {
            builder.CreateStore(vtxidx, val);
          }
          else if(b == spv::BuiltInInstanceIndex || b == spv::BuiltInInstanceId)
          {
            // TODO if the shader actually uses this
            builder.CreateStore(ConstantInt::get(Type::getInt32Ty(c), 0), val);
          }
          else
          {
            printf("Unsupported buitin input");
            assert(false);
          }
        }
        else if(ext.decoration.dec == spv::DecorationLocation)
        {
          Value *outvec4 = builder.CreateAlloca(VectorType::get(Type::getFloatTy(c), 4));

          builder.CreateCall(m->getFunction("GetVertexAttributeData"),
                             {
                                 gpustate, vtxidx,
                                 ConstantInt::get(Type::getInt32Ty(c), ext.decoration.param), outvec4,
                             });

          Value *outval = builder.CreateLoad(outvec4);

          std::vector<uint32_t> shuf = {0, 1, 2, 3};
          shuf.resize(val->getType()->getPointerElementType()->getVectorNumElements());

          Value *truncval = builder.CreateShuffleVector(outval, outval, shuf);

          builder.CreateStore(truncval, val);
        }
        else if(ext.decoration.dec == spv::DecorationDescriptorSet)
        {
          // nothing to do, processed above
        }
        else if(ext.decoration.dec == spv::DecorationBinding)
        {
          Function *getfunc = m->getFunction("GetDescriptorBufferPointer");

          // TODO if image, GetImage
          assert(blocks.find(id) != blocks.end());

          Value *byteptr = builder.CreateCall(
              getfunc, {
                           gpustate, ConstantInt::get(Type::getInt32Ty(c), descset[id]),
                           ConstantInt::get(Type::getInt32Ty(c), ext.decoration.param),
                       });

          Value *bufptr = builder.CreatePointerCast(byteptr, val->getType()->getPointerElementType());

          builder.CreateStore(bufptr, val);
        }
        else if(ext.decoration.dec == spv::DecorationOffset &&
                ext.storageClass == spv::StorageClassPushConstant)
        {
          Function *getfunc = m->getFunction("GetPushConstantPointer");

          assert(blocks.find(id) != blocks.end());

          Value *byteptr = builder.CreateCall(
              getfunc, {
                           gpustate, ConstantInt::get(Type::getInt32Ty(c), ext.decoration.param),
                       });

          Value *bufptr = builder.CreatePointerCast(byteptr, val->getType()->getPointerElementType());

          builder.CreateStore(bufptr, val);
        }
      }

      builder.CreateCall(f, {globalStruct});

      Value *outpos =
          builder.CreateConstInBoundsGEP2_32(cache->getType()->getPointerElementType(), cache, 0, 0);
      Value *interp =
          builder.CreateConstInBoundsGEP2_32(cache->getType()->getPointerElementType(), cache, 0, 1);

      for(const ExternalBinding &ext : externals)
      {
        if(ext.storageClass != spv::StorageClassOutput)
          continue;

        uint32_t id = ext.decoration.id;

        Value *val = getvalue(ext.value);
        if(ext.decoration.member == ~0U)
        {
          val = builder.CreateLoad(val);
        }
        else
        {
          val = builder.CreateLoad(builder.CreateConstInBoundsGEP2_32(
              val->getType()->getPointerElementType(), val, 0, ext.decoration.member));
        }

        if(ext.decoration.dec == spv::DecorationLocation)
        {
          if(val->getType()->isVectorTy() && val->getType()->getVectorNumElements() < 4)
          {
            uint32_t mask[] = {0, 1, 2, 3};
            for(int i = val->getType()->getVectorNumElements(); i < 4; i++)
              mask[i] = 0;
            val = builder.CreateShuffleVector(val, val, mask);
          }
          else if(val->getType()->isFloatTy())
          {
            val = builder.CreateVectorSplat(4, val);
          }

          builder.CreateStore(
              val, builder.CreateConstInBoundsGEP2_32(interp->getType()->getPointerElementType(),
                                                      interp, 0, ext.decoration.param));
        }
        else if(ext.decoration.dec == spv::DecorationBuiltIn)
        {
          switch(ext.decoration.param)
          {
            case spv::BuiltInPosition: builder.CreateStore(val, outpos); break;
            case spv::BuiltInPointSize:
            case spv::BuiltInClipDistance:
            case spv::BuiltInCullDistance:
            {
              // TODO
              break;
            }
            default:
            {
              printf("Unsupported builtin output");
              assert(false);
            }
          }
        }
      }

      builder.CreateRetVoid();

      globalStruct = NULL;
    }
    else if(model == spv::ExecutionModelFragment)
    {
      exportedEntry = Function::Create((FunctionType *)t_FragmentShader,
                                       GlobalValue::ExternalLinkage, uniq_name, m);

      // mark reference parameters
      exportedEntry->addParamAttr(0, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(2, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(4, Attribute::Dereferenceable);

      BasicBlock *block = BasicBlock::Create(c, "block", exportedEntry, NULL);
      builder.SetInsertPoint(block);

      globalStruct = builder.CreateAlloca(globalStructType, 0, NULL, "Globals");

      Argument *gpustate = exportedEntry->arg_begin();
      Argument *pixdepth = gpustate + 1;
      Argument *bary = pixdepth + 1;
      Argument *tri = bary + 1;
      Argument *out = tri + 1;

      Value *loadedBary = builder.CreateLoad(bary);

      for(const ExternalBinding &ext : externals)
      {
        if(ext.storageClass == spv::StorageClassOutput)
          continue;

        uint32_t id = ext.decoration.id;

        Value *val = getvalue(ext.value);

        if(ext.decoration.dec == spv::DecorationBuiltIn)
        {
          spv::BuiltIn b = (spv::BuiltIn)ext.decoration.param;
          {
            printf("Unsupported buitin input");
            assert(false);
          }
        }
        else if(ext.decoration.dec == spv::DecorationLocation)
        {
          Type *interpType = val->getType()->getPointerElementType();
          Value *interp = NULL;

          uint32_t loc = ext.decoration.param;

          // load the vectors we're interpolating between
          Value *vertVec[3];
          for(unsigned comp = 0; comp < 3; comp++)
          {
            vertVec[comp] = builder.CreateLoad(
                builder.CreateInBoundsGEP(tri, {
                                                   // tri[comp]
                                                   ConstantInt::get(Type::getInt32Ty(c), comp),
                                                   // .interps
                                                   ConstantInt::get(Type::getInt32Ty(c), 1),
                                                   // [loc]
                                                   ConstantInt::get(Type::getInt32Ty(c), loc),
                                               }));
          }

          if(interpType->isVectorTy())
          {
            // for each component, dot() the barycentric co-ords with the component from each
            // verts' vector
            interp = UndefValue::get(interpType);

            for(unsigned i = 0; i < interpType->getVectorNumElements(); i++)
            {
              Value *compValue = Constant::getNullValue(VectorType::get(Type::getFloatTy(c), 4));

              for(unsigned comp = 0; comp < 3; comp++)
              {
                compValue = builder.CreateInsertElement(
                    compValue, builder.CreateExtractElement(vertVec[comp], i),
                    ConstantInt::get(Type::getInt32Ty(c), comp));
              }

              Value *interpComp = CreateDot(builder, loadedBary, compValue, 4);
              interp = builder.CreateInsertElement(interp, interpComp,
                                                   ConstantInt::get(Type::getInt32Ty(c), i));
            }
          }
          else
          {
            Value *compValue = Constant::getNullValue(VectorType::get(Type::getFloatTy(c), 4));

            for(unsigned comp = 0; comp < 3; comp++)
            {
              compValue = builder.CreateInsertElement(
                  compValue, builder.CreateExtractElement(vertVec[comp], 0LLU),
                  ConstantInt::get(Type::getInt32Ty(c), comp));
            }

            interp = CreateDot(builder, loadedBary, compValue, 4);
          }

          builder.CreateStore(interp, val);
        }
        else if(ext.decoration.dec == spv::DecorationDescriptorSet)
        {
          // nothing to do, processed above
        }
        else if(ext.decoration.dec == spv::DecorationBinding)
        {
          Function *getfunc = m->getFunction("GetDescriptorImage");

          Value *imgptr = builder.CreateCall(
              getfunc, {
                           gpustate, ConstantInt::get(Type::getInt32Ty(c), descset[id]),
                           ConstantInt::get(Type::getInt32Ty(c), ext.decoration.param),
                       });

          builder.CreateStore(imgptr, val);
        }
      }

      builder.CreateCall(f, {globalStruct});

      for(const ExternalBinding &ext : externals)
      {
        if(ext.storageClass != spv::StorageClassOutput)
          continue;

        uint32_t id = ext.decoration.id;

        Value *val = getvalue(ext.value);
        if(ext.decoration.member == ~0U)
        {
          val = builder.CreateLoad(val);
        }
        else
        {
          val = builder.CreateLoad(builder.CreateConstInBoundsGEP2_32(
              val->getType()->getPointerElementType(), val, 0, ext.decoration.member));
        }

        if(ext.decoration.dec == spv::DecorationLocation)
        {
          assert(ext.decoration.param == 0);

          builder.CreateStore(val, out);
        }
        else if(ext.decoration.dec == spv::DecorationBuiltIn)
        {
          printf("Unsupported builtin output");
          assert(false);
        }
      }

      builder.CreateRetVoid();

      globalStruct = NULL;
    }
    else
    {
      printf("Unsupported execution model");
      assert(false);
    }
  }

  std::string str;
  raw_string_ostream os(str);

  if(verifyModule(*m, &os))
  {
    printf("Module did not verify: %s\n", os.str().c_str());
    assert(false);
  }

  str.clear();
  m->print(os, NULL);
  ret->ir = os.str();

  PassManagerBuilder pm_builder;
  pm_builder.OptLevel = 3;
  pm_builder.SizeLevel = 0;
  pm_builder.LoopVectorize = true;
  pm_builder.SLPVectorize = true;

  legacy::FunctionPassManager function_pm(m);
  legacy::PassManager module_pm;

  pm_builder.populateFunctionPassManager(function_pm);
  pm_builder.populateModulePassManager(module_pm);

  function_pm.doInitialization();
  for(Function *f : functions)
    function_pm.run(*f);
  module_pm.run(*m);

  str.clear();
  m->print(os, NULL);
  ret->optir = os.str();

  m->setDataLayout(target->createDataLayout());

  auto symbolResolver = llvm::orc::createLambdaResolver(
      [ret](const std::string &name) {
        JITSymbol sym = ret->linker->findSymbol(name, false);
        if(sym)
          return sym;

        return globallinker->findSymbol(name, false);
      },
      [](const std::string &name) {
        uint64_t sym_addr = RTDyldMemoryManager::getSymbolAddressInProcess(name);
        if(sym_addr)
          return JITSymbol(sym_addr, JITSymbolFlags::Exported);

        return JITSymbol(NULL);
      });

  orc::SimpleCompiler &comp = *compiler;

  ret->linker->addObject(std::make_shared<orc::SimpleCompiler::CompileResult>(comp(*m)),
                         symbolResolver);

  return ret;
}

Shader GetFuncPointer(LLVMFunction *func, const char *name)
{
  char uniq_name[512];
  sprintf_s(uniq_name, "%p_%s", func, name);

  std::string str;
  llvm::raw_string_ostream os(str);
  llvm::Mangler::getNameWithPrefix(os, uniq_name, func->module->getDataLayout());

  llvm::JITSymbol sym = func->linker->findSymbol(os.str(), false);

  llvm::JITTargetAddress addr = *sym.getAddress();

  return (Shader)addr;
}

void DestroyFunction(LLVMFunction *func)
{
  delete func;
}
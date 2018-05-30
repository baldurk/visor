#include "precompiled.h"
#include "spirv_compile.h"
#include <set>
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
typedef llvm::orc::IRCompileLayer<Linker, llvm::orc::SimpleCompiler> Compiler;

namespace
{
llvm::LLVMContext *context = NULL;
llvm::SectionMemoryManager memManager;

llvm::TargetMachine *target = NULL;

Linker *linker = NULL;
Compiler *compiler = NULL;

auto symbolResolver = llvm::orc::createLambdaResolver(
    [](const std::string &name) {
      // foo
      return compiler->findSymbol(name, false);
    },
    [](const std::string &name) {
      uint64_t sym_addr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name);
      if(sym_addr)
        return llvm::JITSymbol(sym_addr, llvm::JITSymbolFlags::Exported);

      return llvm::JITSymbol(NULL);
    });

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
  ~LLVMFunction() { delete module; }
  llvm::Module *module = NULL;
  std::string ir, optir;
};

void llvm_fatal(void *user_data, const std::string &reason, bool gen_crash_diag)
{
  assert(false);
}

void InitLLVM()
{
  using namespace llvm;

  install_fatal_error_handler(&llvm_fatal);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  context = new LLVMContext();

  target = EngineBuilder().selectTarget();
  linker = new Linker([] {
    return std::shared_ptr<llvm::SectionMemoryManager>(&memManager, [](SectionMemoryManager *) {});
  });
  compiler = new Compiler(*linker, orc::SimpleCompiler(*target));

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
                                           ArrayType::get(t_VertexCacheEntry, 3),
                                           // float4 &out
                                           PointerType::get(t_float4, 0),
                                       },
                                       false);
}

void ShutdownLLVM()
{
  delete compiler;
  delete linker;
  delete target;
  delete context;
}

extern "C" __declspec(dllexport) void Float4x4TimesVec4(float4 mat[4], float4 vec, float4 &out)
{
  out = float4(0, 0, 0, 0);

  float *fmat = (float *)mat;
  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      out.v[row] += fmat[col * 4 + row] * vec.v[col];
  }
}

extern "C" __declspec(dllexport) byte *GetDescriptorBufferPointer(const GPUState &state,
                                                                  uint32_t set, uint32_t bind)
{
  assert(set == 0);
  const VkDescriptorBufferInfo &buf = state.set->binds[bind].data.bufferInfo;

  return buf.buffer->bytes + buf.offset;
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

  IntegerType *i32 = Type::getInt32Ty(c);

  Function *Float4x4TimesVec4 = Function::Create(
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
      llvm::GlobalValue::ExternalLinkage, "Float4x4TimesVec4", ret->module);

  Function *GetDescriptorBufferPointer = Function::Create(
      FunctionType::get(PointerType::get(Type::getInt8Ty(c), 0),
                        {
                            t_GPUStateRef, Type::getInt32Ty(c), Type::getInt32Ty(c),
                        },
                        false),
      llvm::GlobalValue::ExternalLinkage, "GetDescriptorBufferPointer", ret->module);

  std::vector<Function *> functions;
  Function *function = NULL;

  IRBuilder<> builder(c, ConstantFolder());

  std::vector<const uint32_t *> entries;

  struct IDDecoration
  {
    uint32_t id;
    spv::Decoration dec;
    uint32_t param;
  };
  std::vector<IDDecoration> decorations;
  std::set<uint32_t> inputs, outputs;

  std::vector<Value *> values;
  values.resize(idbound);

  std::vector<Type *> types;
  types.resize(idbound);

  std::vector<std::string> names;
  names.resize(idbound);

  auto getname = [&names](uint32_t id) -> std::string {
    std::string name = names[id];
    if(name.empty())
      name = "spv__" + std::to_string(id);
    return name;
  };

  size_t it = 5;
  pCode += it;
  while(it < codeSize)
  {
    uint16_t WordCount = pCode[0] >> spv::WordCountShift;

    spv::Op opcode = spv::Op(pCode[0] & spv::OpCodeMask);

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

      case spv::OpEntryPoint:
      {
        // we'll do this in a second pass once we have the function resolved
        entries.push_back(pCode);
        break;
      }
      case spv::OpDecorate:
      {
        if(WordCount == 3)
          decorations.push_back({pCode[1], (spv::Decoration)pCode[2], 0});
        else
          decorations.push_back({pCode[1], (spv::Decoration)pCode[2], pCode[3]});

        break;
      }
      case spv::OpMemberDecorate:
      {
        // TODO
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
        // ignore pCode[2] storageclass
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
        std::vector<Type *> params;
        for(uint16_t i = 3; i < WordCount; i++)
          params.push_back(types[pCode[i]]);
        types[pCode[1]] = FunctionType::get(types[pCode[2]], params, false);
        break;
      }
      case spv::OpTypeImage:
      case spv::OpTypeSampledImage:
      {
        // TODO proper image handling
        types[pCode[1]] = t_VkImage;
        break;
      }

      ////////////////////////////////////////////////
      // Variables and Constants
      ////////////////////////////////////////////////

      case spv::OpVariable:
      {
        if(function)
        {
          assert(pCode[3] == spv::StorageClassFunction);
          values[pCode[2]] = builder.CreateAlloca(types[pCode[1]]->getPointerElementType(), NULL,
                                                  getname(pCode[2]));

          if(WordCount > 4)
            builder.CreateStore(values[pCode[4]], values[pCode[2]]);
        }
        else
        {
          Type *t = types[pCode[1]];
          assert(isa<PointerType>(t));

          Constant *initialiser = Constant::getNullValue(t->getPointerElementType());

          if(WordCount > 4)
          {
            Value *initVal = values[pCode[4]];

            // "Initializer must be an <id> from a constant instruction or a global (module scope)
            // OpVariable instruction."
            // so either it's already a constant, or else the variable it points to is initialised
            // with a constant so we can steal it.
            if(isa<Constant>(initVal))
              initialiser = (Constant *)initVal;
            else
              initialiser = ((GlobalVariable *)initVal)->getInitializer();
          }

          values[pCode[2]] =
              new GlobalVariable(*ret->module, t->getPointerElementType(), false,
                                 GlobalValue::PrivateLinkage, initialiser, getname(pCode[2]));

          if(pCode[3] == spv::StorageClassInput)
            inputs.insert(pCode[2]);
          else if(pCode[3] == spv::StorageClassOutput)
            outputs.insert(pCode[2]);
        }

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

      ////////////////////////////////////////////////
      // Functions
      ////////////////////////////////////////////////

      case spv::OpFunction:
      {
        sprintf_s(uniq_name, "%p_%d", ret, pCode[2]);

        function = Function::Create((llvm::FunctionType *)types[pCode[4]],
                                    llvm::GlobalValue::InternalLinkage, uniq_name, ret->module);
        // ignore function return type pCode[1]
        // ignore function control pCode[3]
        functions.push_back(function);

        values[pCode[2]] = function;
        break;
      }
      case spv::OpFunctionEnd:
      {
        assert(function);

        std::string str;
        raw_string_ostream os(str);

        str.clear();
        if(verifyFunction(*function, &os))
        {
          printf("Function did not verify: %s\n", os.str().c_str());
          assert(false);
        }

        function = NULL;
        break;
      }

      ////////////////////////////////////////////////
      // Instructions
      ////////////////////////////////////////////////

      case spv::OpLabel:
      {
        assert(function);
        BasicBlock *block = BasicBlock::Create(c, "block", function, NULL);
        builder.SetInsertPoint(block);
        break;
      }
      case spv::OpLoad:
      {
        values[pCode[2]] = builder.CreateLoad(values[pCode[3]]);
        // ignore result type pCode[1]
        // ignore memory access pCode[4]
        break;
      }
      case spv::OpStore:
      {
        builder.CreateStore(values[pCode[2]], values[pCode[1]]);
        // ignore memory access pCode[3]
        break;
      }
      case spv::OpAccessChain:
      {
        std::vector<Value *> indices = {ConstantInt::get(Type::getInt32Ty(c), 0)};
        for(uint16_t i = 4; i < WordCount; i++)
          indices.push_back(values[pCode[i]]);
        values[pCode[2]] = builder.CreateGEP(values[pCode[3]], indices);
        // ignore memory access pCode[3]
        break;
      }
      case spv::OpMatrixTimesVector:
      {
        // create output value
        Value *retptr = builder.CreateAlloca(values[pCode[4]]->getType());

        // create temporary array
        Value *arrayptr = builder.CreateAlloca(values[pCode[3]]->getType());

        // fill temporary array with values
        for(unsigned i = 0; i < values[pCode[3]]->getType()->getArrayNumElements(); i++)
        {
          builder.CreateStore(
              builder.CreateExtractValue(values[pCode[3]], {i}),
              builder.CreateConstInBoundsGEP2_32(values[pCode[3]]->getType(), arrayptr, 0, i));
        }

        // call function
        builder.CreateCall(Float4x4TimesVec4, {
                                                  builder.CreateConstInBoundsGEP2_32(
                                                      values[pCode[3]]->getType(), arrayptr, 0, 0),
                                                  values[pCode[4]], retptr,
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
      case spv::OpVectorShuffle:
      {
        std::vector<uint32_t> indices;
        for(uint16_t i = 5; i < WordCount; i++)
          indices.push_back(pCode[i]);
        values[pCode[2]] = builder.CreateShuffleVector(values[pCode[3]], values[pCode[4]], indices);
        break;
      }
      case spv::OpDPdx:
      case spv::OpDPdy:
      {
        // TODO
        values[pCode[2]] = Constant::getNullValue(types[pCode[1]]);
        break;
      }
      case spv::OpExtInst:
      {
        // TODO
        values[pCode[2]] = values[pCode[5]];
        break;
      }
      case spv::OpDot:
      {
        // TODO
        values[pCode[2]] = builder.CreateFAdd(values[pCode[3]], values[pCode[4]]);
        break;
      }
      case spv::OpImageSampleImplicitLod:
      {
        // TODO
        values[pCode[2]] =
            builder.CreateShuffleVector(values[pCode[4]], values[pCode[4]], {0, 1, 0, 1});
        break;
      }
      case spv::OpReturn:
      {
        assert(function);
        builder.CreateRetVoid();
        break;
      }

      default: assert(false && "Unhandled SPIR-V opcode"); break;
    }

    pCode += WordCount;
    it += WordCount;
  }

  for(const uint32_t *e : entries)
  {
    spv::ExecutionModel model = (spv::ExecutionModel)e[1];

    // name of the void entry() function we generated above
    sprintf_s(uniq_name, "%p_%s", ret, (char *)&e[3]);

    Function *f = (Function *)values[e[2]];

    Function *exportedEntry = NULL;

    std::map<uint32_t, uint32_t> descset;

    if(model == spv::ExecutionModelVertex)
    {
      exportedEntry = Function::Create((FunctionType *)t_VertexShader, GlobalValue::ExternalLinkage,
                                       uniq_name, ret->module);

      // mark reference parameters
      exportedEntry->addParamAttr(0, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(2, Attribute::Dereferenceable);

      BasicBlock *block = BasicBlock::Create(c, "block", exportedEntry, NULL);
      builder.SetInsertPoint(block);

      Argument *gpustate = exportedEntry->arg_begin();
      Argument *vtxidx = gpustate + 1;
      Argument *cache = vtxidx + 1;

      for(const IDDecoration &dec : decorations)
      {
        uint32_t id = dec.id;

        if(dec.dec == spv::DecorationBuiltIn)
        {
          spv::BuiltIn b = (spv::BuiltIn)dec.param;
          if(b == spv::BuiltInVertexIndex)
            builder.CreateStore(vtxidx, values[id]);
        }
        else if(dec.dec == spv::DecorationLocation && inputs.find(id) != inputs.end())
        {
          // TODO populate input values from VBs
        }
        else if(dec.dec == spv::DecorationDescriptorSet)
        {
          descset[dec.id] = dec.param;
        }
        else if(dec.dec == spv::DecorationBinding)
        {
          Function *getfunc = GetDescriptorBufferPointer;

          // TODO if image, GetImage

          Value *byteptr = builder.CreateCall(
              getfunc, {
                           gpustate, ConstantInt::get(Type::getInt32Ty(c), descset[dec.id]),
                           ConstantInt::get(Type::getInt32Ty(c), dec.param),
                       });

          Value *bufptr = builder.CreatePointerCast(byteptr, values[dec.id]->getType());

          Value *buf = builder.CreateLoad(bufptr);

          builder.CreateStore(buf, values[dec.id]);
        }
      }

      builder.CreateCall(f, {});

      Value *outpos =
          builder.CreateConstInBoundsGEP2_32(cache->getType()->getPointerElementType(), cache, 0, 0);
      Value *interp =
          builder.CreateConstInBoundsGEP2_32(cache->getType()->getPointerElementType(), cache, 0, 1);

      for(const IDDecoration &dec : decorations)
      {
        uint32_t id = dec.id;

        if(dec.dec == spv::DecorationLocation && outputs.find(id) != outputs.end())
        {
          Value *val = builder.CreateLoad(values[id]);

          if(val->getType()->isVectorTy() && val->getType()->getVectorNumElements() < 4)
          {
            uint32_t mask[] = {0, 1, 2, 3};
            for(int i = val->getType()->getVectorNumElements(); i < 4; i++)
              mask[i] = 0;
            val = builder.CreateShuffleVector(val, val, mask);
          }

          builder.CreateStore(
              val, builder.CreateConstInBoundsGEP2_32(interp->getType()->getPointerElementType(),
                                                      interp, 0, dec.param));
        }
      }

      // hack
      {
        Value *pos =
            builder.CreateLoad(builder.CreateConstInBoundsGEP2_32(types[28], values[30], 0, 0));

        builder.CreateStore(pos, outpos);
      }

      builder.CreateRetVoid();
    }
    else if(model == spv::ExecutionModelFragment)
    {
      exportedEntry = Function::Create((FunctionType *)t_FragmentShader,
                                       GlobalValue::ExternalLinkage, uniq_name, ret->module);

      // mark reference parameters
      exportedEntry->addParamAttr(0, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(2, Attribute::Dereferenceable);
      exportedEntry->addParamAttr(4, Attribute::Dereferenceable);

      BasicBlock *block = BasicBlock::Create(c, "block", exportedEntry, NULL);
      builder.SetInsertPoint(block);

      // TODO interpolate inputs

      // TODO populate globals from descriptor sets

      builder.CreateCall(f, {});

      // TODO fetch outputs

      builder.CreateRetVoid();
    }
    else
    {
      printf("Unsupported execution model");
      assert(false);
    }
  }

  std::string str;
  raw_string_ostream os(str);

  if(verifyModule(*ret->module, &os))
  {
    printf("Module did not verify: %s\n", os.str().c_str());
    assert(false);
  }

  str.clear();
  ret->module->print(os, NULL);
  ret->ir = os.str();

  PassManagerBuilder pm_builder;
  pm_builder.OptLevel = 3;
  pm_builder.SizeLevel = 0;
  pm_builder.LoopVectorize = true;
  pm_builder.SLPVectorize = true;

  legacy::FunctionPassManager function_pm(ret->module);
  legacy::PassManager module_pm;

  pm_builder.populateFunctionPassManager(function_pm);
  pm_builder.populateModulePassManager(module_pm);

  function_pm.doInitialization();
  for(Function *f : functions)
    function_pm.run(*f);
  module_pm.run(*ret->module);

  str.clear();
  ret->module->print(os, NULL);
  ret->optir = os.str();

  ret->module->setDataLayout(target->createDataLayout());

  compiler->addModule(std::shared_ptr<Module>(ret->module, [](Module *) {}), symbolResolver);

  return ret;
}

Shader GetFuncPointer(LLVMFunction *func, const char *name)
{
  char uniq_name[512];
  sprintf_s(uniq_name, "%p_%s", func, name);

  std::string str;
  llvm::raw_string_ostream os(str);
  llvm::Mangler::getNameWithPrefix(os, uniq_name, func->module->getDataLayout());

  llvm::JITSymbol sym = compiler->findSymbol(os.str(), false);

  llvm::JITTargetAddress addr = *sym.getAddress();

  return (Shader)addr;
}

void DestroyFunction(LLVMFunction *func)
{
  delete func;
}
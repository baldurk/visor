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

namespace
{
llvm::LLVMContext *context = NULL;
llvm::SectionMemoryManager memManager;

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
    return std::shared_ptr<llvm::SectionMemoryManager>(&memManager,
                                                       [](llvm::SectionMemoryManager *) {});
  });
}

static void DeclareGlobalFunctions(llvm::Module *m)
{
  using namespace llvm;

  LLVMContext &c = *context;

  // LLVM implemented functions in globalFunctions

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

  // C implemented and exported functions

  Function::Create(FunctionType::get(PointerType::get(Type::getInt8Ty(c), 0),
                                     {
                                         t_GPUStateRef, Type::getInt32Ty(c), Type::getInt32Ty(c),
                                     },
                                     false),
                   GlobalValue::ExternalLinkage, "GetDescriptorBufferPointer", m);
}

void InitLLVM()
{
  using namespace llvm;

  install_fatal_error_handler(&llvm_fatal);

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  context = new LLVMContext();

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
                                           ArrayType::get(t_VertexCacheEntry, 3),
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
  delete context;
}

extern "C" __declspec(dllexport) void Float4x4TimesVec4(float4 mat[4], const float4 &vec, float4 &out)
{
  out = {0, 0, 0, 0};

  float *fmat = (float *)mat;
  for(int row = 0; row < 4; row++)
    for(int col = 0; col < 4; col++)
      out.v[row] += fmat[col * 4 + row] * vec.v[col];
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
  ret->linker = makeLinker();

  Module *m = ret->module;

  DeclareGlobalFunctions(m);

  std::vector<Function *> functions;
  Function *function = NULL;

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
  std::set<uint32_t> blocks;

  // in lieu of a proper SPIR-V representation, this lets us go from pointer type -> struct type
  std::map<uint32_t, uint32_t> ptrtypes;

  std::vector<Value *> values;
  std::vector<Type *> types;
  std::map<uint32_t, std::string> names;

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

  // we do two passes:
  // 1. Gather information:
  //    - Parse types and create corresponding LLVM types
  //    - Generate LLVM constants immediately
  //    - Store names and debug info locally
  //    - Gather decoration information locally
  //    - Gather types of globals locally, in order
  //
  // Generate global struct here.
  //
  // 2. Process and generate LLVM functions and globals

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
        if(blocks.find(pCode[3]) != blocks.end() && pCode[2] == spv::StorageClassUniform)
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

          if(pCode[3] <= spv::StorageClassOutput)
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
        sprintf_s(uniq_name, "%p_%d", ret, pCode[2]);

        if(types[pCode[4]] == NULL)
        {
          const FunctionTypeInfo &f = functypes[pCode[4]];
          types[pCode[4]] = FunctionType::get(f.ret, f.params, false);
        }

        function = Function::Create((llvm::FunctionType *)types[pCode[4]],
                                    llvm::GlobalValue::InternalLinkage, uniq_name, m);

        function->addFnAttr(Attribute::AlwaysInline);

        // ignore function return type pCode[1]
        // ignore function control pCode[3]
        functions.push_back(function);

        globalStruct = function->arg_begin();

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
        globalStruct = NULL;
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
        values[pCode[2]] = builder.CreateLoad(getvalue(values[pCode[3]]));
        // ignore result type pCode[1]
        // ignore memory access pCode[4]
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
        builder.CreateCall(
            m->getFunction("Float4x4TimesVec4"),
            {
                builder.CreateConstInBoundsGEP2_32(values[pCode[3]]->getType(), arrayptr, 0, 0),
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
          if(b == spv::BuiltInVertexIndex)
          {
            builder.CreateStore(vtxidx, val);
          }
          else
          {
            printf("Unsupported buitin input");
            assert(false);
          }
        }
        else if(ext.decoration.dec == spv::DecorationLocation)
        {
          // TODO populate input values from VBs
        }
        else if(ext.decoration.dec == spv::DecorationDescriptorSet)
        {
          descset[id] = ext.decoration.param;
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

      // TODO interpolate inputs

      // TODO populate globals from descriptor sets

      builder.CreateCall(f, {globalStruct});

      // TODO fetch outputs

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
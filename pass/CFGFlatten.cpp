#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/LowerSwitch.h"
#include "llvm/Transforms/Scalar/Reg2Mem.h"

using namespace llvm;

/**
* @brief Splits the entry block if it ends with a conditional branch
* to make it unconditionally jump to the dispatcher
*/
static void splitEntryBlock(BasicBlock *entryBlock) {
  Instruction *term = entryBlock->getTerminator();
  BranchInst *br = dyn_cast<BranchInst>(term);

  if (br && br->isConditional()) {
    Value *cond = br->getCondition();

    if (Instruction *inst = dyn_cast<Instruction>(cond)) { // may be a constant
      entryBlock->splitBasicBlockBefore(inst, "newEntry");
    }
  }
}

/**
* @brief Iterates over basic blocks and dispatches them in a switch statement
* @return LoadInst corresponding to the switchVar
*/
static LoadInst* flatten(Function &F, llvm::SmallVector<LoadInst *> oldLoads) {
  Type *i32_type = Type::getInt32Ty(F.getContext());

  // Create switch variable on the stack
  AllocaInst *switchVar = new AllocaInst(
    Type::getInt32Ty(F.getContext()),
    0,
    "switchVar",
    F.getEntryBlock().getFirstNonPHI() // PHI nodes must be at the beginning
  );

  // Create switch dispatcher block
  BasicBlock *dispatcher = BasicBlock::Create(F.getContext(), "dispatcher", &F);
  LoadInst *load = new LoadInst(i32_type, switchVar, "switchVar", dispatcher);
  SwitchInst *sw = SwitchInst::Create(load, dispatcher, 0, dispatcher);

  // Move older loadInsts to the new dispatcher block to avoid scope issues
  for (auto &oldLoad : oldLoads) {
    oldLoad->moveBefore(dispatcher->getFirstNonPHI());
  }

  // Add all non-terminating basic blocks to the switch
  splitEntryBlock(&F.getEntryBlock());

  int idx = 0;
  for (auto &B : F) {
    if (&B == &F.getEntryBlock() || &B == dispatcher) continue; // Skip entry and dispatcher blocks

    // Create case variable
    ConstantInt *swIdx = dyn_cast<ConstantInt>(ConstantInt::get(i32_type, idx));

    sw->addCase(swIdx, &B);
    idx++;
  }

  // Update branches to set switchVar conditionally
  // Unconditionally branch to the dispatcher block
  for (auto &B : F) {
    ConstantInt *caseValue = sw->findCaseDest(&B);
    if (!caseValue && &B != &F.getEntryBlock()) continue; // Skips blocks not added except entry

    Instruction *term = B.getTerminator();

    if (term->getNumSuccessors() == 0) continue; // Return block

    if (term->getNumSuccessors() == 1) { // Unconditional jump
      BasicBlock *succ = term->getSuccessor(0);

      ConstantInt *idx = sw->findCaseDest(succ);
      new StoreInst(idx, switchVar, term);
    }

    else if (term->getNumSuccessors() == 2) { // Conditional jump
      ConstantInt *idxTrue = sw->findCaseDest(term->getSuccessor(0));
      ConstantInt *idxFalse = sw->findCaseDest(term->getSuccessor(1));

      BranchInst *br = cast<BranchInst>(term);
      SelectInst *sl = SelectInst::Create(br->getCondition(), idxTrue, idxFalse, "", term);
      new StoreInst(sl, switchVar, term); // switchVar = cond ? idxTrue : idxFalse
    }

    term->eraseFromParent();
    BranchInst::Create(dispatcher, &B);
  }

  return load;
}

namespace {

struct FlattenCFGPass : public PassInfoMixin<FlattenCFGPass> {
PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

    // Create LowerSwitchPass and RegToMem instances
    LowerSwitchPass *lower = new LowerSwitchPass();
    RegToMemPass *reg = new RegToMemPass();
    FunctionAnalysisManager &FM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

    for (auto &F : M.functions()) {
      if (F.size() < 2) continue; // Function too small to be flattened

      // Load instructions from prev. iterations
      SmallVector<LoadInst *> oldLoads;

      for (int i=0; i < 2; i++) {
        errs() << "Running flatten on " << F.getName() << " (iteration: " << i << ")\n";

        lower->run(F, FM); // Remove switch statements
        reg->run(F, FM);   // Remove phi nodes

        LoadInst *load = flatten(F, oldLoads); // Run the pass logic
        oldLoads.push_back(load);
      }
    }

    return PreservedAnalyses::none();
  };
};

} // end namespace llvm

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
    .APIVersion = LLVM_PLUGIN_API_VERSION,
    .PluginName = "CFGPass",
    .PluginVersion = "v0.1",
    .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
      PB.registerPipelineStartEPCallback(
        [](ModulePassManager &MPM, OptimizationLevel Level) {
          MPM.addPass(FlattenCFGPass());
        });
    }
  };
}

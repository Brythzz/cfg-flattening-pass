#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/Transforms/Utils/LowerSwitch.h"

#define DEBUG_TYPE "flatten-cfg"
STATISTIC(Flattened, "Number of functions flattened");

using namespace llvm;

/**
* @brief Splits the entry block if it ends with a conditional branch
* to make it unconditionally jump to the dispatcher
*/
static BasicBlock* splitEntryBlock(BasicBlock *entryBlock) {
  Instruction *term = entryBlock->getTerminator();

  if (BranchInst *br = dyn_cast<BranchInst>(term)) {
    if (br->isConditional()) {
      Value *cond = br->getCondition();

      if (Instruction *inst = dyn_cast<Instruction>(cond)) { // may be a constant
        entryBlock = entryBlock->splitBasicBlockBefore(inst, "newEntry");
      }
    }
  }

  else if (SwitchInst *sw = dyn_cast<SwitchInst>(term)) {
    entryBlock = entryBlock->splitBasicBlockBefore(sw, "newEntry");
  }

  return entryBlock;
}

/**
* @brief Iterates over basic blocks and dispatches them in a switch statement
*/
static bool flatten(Function &F) {
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

  // Add all non-terminating basic blocks to the switch
  BasicBlock *EntryBlock = splitEntryBlock(&F.getEntryBlock());

  int idx = 0;
  for (auto &B : F) {
    if (&B == EntryBlock || &B == dispatcher) continue; // Skip entry and dispatcher blocks

    // Create case variable
    ConstantInt *swIdx = dyn_cast<ConstantInt>(ConstantInt::get(i32_type, idx));

    sw->addCase(swIdx, &B);
    idx++;
  }

  if (idx <= 1) return false; // Function too small to be flattened

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

  return true;
}

namespace {

struct FlattenCFGPass : public PassInfoMixin<FlattenCFGPass> {
PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    for (auto &F : M.functions()) {
      if (F.empty()) continue;

      errs() << "Trying to flatten " << F.getName() << "!\n";

      // Remove all switch statements
      LowerSwitchPass *lower = new LowerSwitchPass();
      FunctionAnalysisManager &FM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
      lower->run(F, FM);

      // Run our pass logic
      if (flatten(F)) Flattened++;
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
#include "OurLoopDeletionPass.h"

bool OurLoopDeletionPass::isUsedAfterLoop(Loop *L, const Value *Var) {
  BasicBlock *Preheader = L->getLoopPreheader();
  for (const User *U : Var->users()) {
    if (const Instruction *UserInstr = dyn_cast<Instruction>(U)) {
      const BasicBlock *UserBB = UserInstr->getParent();
      if (!L->contains(UserBB) && UserBB != Preheader) {
        return true;
      }
    }
  }
  return false;
}

bool OurLoopDeletionPass::isLoopDead(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *ExitBlock = L->getExitBlock();

  if (!Preheader || !ExitBlock) {
    errs() << "Loop does not have a preheader or a single exit block.\n";
    return false;
  }

  std::unordered_set<const Value *> VarsAlteredInLoop;
  for (const BasicBlock *BB : L->blocks()) {
    for (const Instruction &I : *BB) {
      if (isa<StoreInst>(&I)) {
        Value *Ptr = I.getOperand(1);
        VarsAlteredInLoop.insert(Ptr);
      } else if (I.mayHaveSideEffects()) {
        errs() << "INSTRUCTION WITH SIDE EFFECTS FOUND: " << I << "\n";
        return false;
      }
    }
  }

  for (const Value *Var : VarsAlteredInLoop) {
    if (isUsedAfterLoop(L, Var)) {
      errs() << "Variable " << *Var
             << " is altered in the loop and used after the loop.\n";
      return false;
    }
  }

  return true;
}

// NOTE: preheader and a single exit block are guaranteed to exist because of
// the checks we do in the runOnLoop function before calling this function
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();

  Preheader->getTerminator()->replaceUsesOfWith(Header, ExitBlock);

  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  // for multiple levels of nesting, we need to 'bubble up' the changes to all
  // parent loops
  Loop *ParentLoop = L->getParentLoop();
  while (ParentLoop) {
    for (BasicBlock *BB : Blocks) {
      if (ParentLoop->contains(BB)) {
        ParentLoop->removeBlockFromLoop(BB);
      }
    }
    if (ParentLoop->contains(L)) {
      ParentLoop->removeChildLoop(L);
    }
    ParentLoop = ParentLoop->getParentLoop();
  }

  for (BasicBlock *BB : Blocks) {
    BB->dropAllReferences();
  }

  for (BasicBlock *BB : Blocks) {
    BB->eraseFromParent();
  }
}

bool OurLoopDeletionPass::runOnLoop(Loop *L, LPPassManager &LPM) {
  errs() << "\n==============================\n";
  errs() << "Running on loop: " << *L << "\n";

  CurrentFunction = L->getHeader()->getParent();

  if (!L->isLoopSimplifyForm()) {
    errs() << "Loop is not in simplified form. Skipping.\n";
    return false;
  }

  if (!L->getSubLoops().empty()) {
    errs() << "Loop has nested loops. Skipping.\n";
    return false;
  }

  if (!isLoopDead(L)) {
    errs() << "Loop is not dead. Skipping.\n";
    return false;
  }

  errs() << "Loop is dead. Deleting loop.\n";
  deleteLoop(L);

  return true;
}

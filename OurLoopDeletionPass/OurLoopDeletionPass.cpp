#include "OurLoopDeletionPass.h"

bool OurLoopDeletionPass::isUsedAfterLoop(Loop *L, const Value *Var) {
  BasicBlock *ExitBlock = L->getExitBlock();
  BasicBlock *Preheader = L->getLoopPreheader();
  std::unordered_set<const BasicBlock *> BlocksAfterLoop;
  std::stack<const BasicBlock *> BlocksToVisit;
  BlocksAfterLoop.insert(ExitBlock);
  BlocksToVisit.push(ExitBlock);

  while (!BlocksToVisit.empty()) {
    const BasicBlock *CurrentBlock = BlocksToVisit.top();
    BlocksToVisit.pop();

    for (const BasicBlock *Succ : successors(CurrentBlock)) {
      if (Succ != Preheader && !BlocksAfterLoop.count(Succ)) {
        BlocksAfterLoop.insert(Succ);
        BlocksToVisit.push(Succ);
      }
    }
  }

  for (const User *U : Var->users()) {
    if (const Instruction *UserInstr = dyn_cast<Instruction>(U)) {
      const BasicBlock *UserBB = UserInstr->getParent();
      if (!L->contains(UserBB) && BlocksAfterLoop.count(UserBB)) {
        return true;
      }
    }
  }
  return false;
}

const ConstantInt *
OurLoopDeletionPass::resolveOperandToConstant(const Value *Op,
                                              BasicBlock *Preheader) {
  // if the operand is already a constant integer, return it
  if (const ConstantInt *CInt = dyn_cast<ConstantInt>(Op)) {
    return CInt;
  }
  // if the operand is a LoadInst, look for the last StoreInst to that pointer
  // in the preheader
  if (auto *LI = dyn_cast<LoadInst>(Op)) {
    const Value *Ptr = LI->getPointerOperand();

    for (auto RI = Preheader->rbegin(), RE = Preheader->rend(); RI != RE;
         ++RI) {
      if (auto *SI = dyn_cast<StoreInst>(&*RI)) {
        if (SI->getPointerOperand() == Ptr) {
          return dyn_cast<ConstantInt>(SI->getValueOperand());
        }
      }
    }
  }
  return nullptr;
}

bool OurLoopDeletionPass::isLoopInfinite(
    Loop *L, const std::unordered_set<const Value *> &VarsAlteredInLoop) {

  const BranchInst *BI = dyn_cast<BranchInst>(L->getHeader()->getTerminator());
  if (!BI || !BI->isConditional()) {
    return true;
  }

  Value *Cond = BI->getCondition();

  if (const ConstantInt *CI = dyn_cast<ConstantInt>(Cond)) {
    return !CI->isZero();
  }

  const ICmpInst *ICmp = dyn_cast<ICmpInst>(Cond);
  if (!ICmp) {
    return true;
  }

  bool loopCounterAlteredInLoop = false;
  for (const Value *Op : ICmp->operands()) {
    if (const LoadInst *LI = dyn_cast<LoadInst>(Op)) {
      const Value *Ptr = LI->getPointerOperand();
      if (VarsAlteredInLoop.count(Ptr)) {
        errs() << "Variable " << *Ptr
               << " is altered in the loop and used in the loop "
                  "condition.\n";
        loopCounterAlteredInLoop = true;
      }
    }
  }

  // if the loop counter is altered in the loop, we cannot determine if the loop
  // is infinite or not
  if (loopCounterAlteredInLoop) {
    return false;
  }

  // loop counter is not altered in the loop, checking if the loop condition
  // is statically determinable

  // no need to check if the preheader exists because we already checked that
  // the loop is in simplified form
  BasicBlock *Preheader = L->getLoopPreheader();

  const ConstantInt *LHS =
      resolveOperandToConstant(ICmp->getOperand(0), Preheader);
  const ConstantInt *RHS =
      resolveOperandToConstant(ICmp->getOperand(1), Preheader);

  // undeterminable loop condition
  if (!LHS || !RHS) {
    return true;
  }

  // mathematically evaluate the comparison based on the predicate
  // and the constant values of LHS and RHS
  bool LoopWillExecute =
      ICmpInst::compare(LHS->getValue(), RHS->getValue(), ICmp->getPredicate());

  // loop is either infinite or will not execute at all
  return LoopWillExecute;
}

bool OurLoopDeletionPass::isLoopDead(Loop *L) {
  if (!L->getSubLoops().empty()) {
    errs() << "Loop has nested loops. Skipping.\n";
    return false;
  }

  if (!L->isLoopSimplifyForm()) {
    errs() << "Loop is not in simplified form.\n";
    return false;
  }

  if (!L->getExitBlock()) {
    errs() << "Loop does not have a single exit block.\n";
    return false;
  }

  std::unordered_set<const Value *> VarsAlteredInLoop;
  for (const BasicBlock *BB : L->blocks()) {
    for (const Instruction &I : *BB) {
      if (isa<StoreInst>(&I)) {
        Value *Ptr = I.getOperand(1);
        VarsAlteredInLoop.insert(Ptr);
      } else if (I.mayHaveSideEffects()) {
        errs() << "Instruction with side effects found: " << I << "\n";
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

  if (isLoopInfinite(L, VarsAlteredInLoop)) {
    errs() << "Loop is infinite.\n";
    return false;
  }

  return true;
}

// NOTE: preheader and a single exit block are guaranteed to exist because of
// the checks we do in the runOnLoop function before calling this function
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();
  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  Preheader->getTerminator()->replaceUsesOfWith(Header, ExitBlock);

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

  if (!isLoopDead(L)) {
    errs() << "Loop is not dead. Skipping.\n";
    return false;
  }

  errs() << "Loop is dead. Deleting loop.\n";
  deleteLoop(L);
  LPM.markLoopAsDeleted(*L);

  return true;
}

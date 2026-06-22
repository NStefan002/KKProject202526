#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace {
struct OurLoopFusionPass : public LoopPass {
  std::vector<BasicBlock *> LoopBasicBlocks;

  static char ID;
  OurLoopFusionPass() : LoopPass(ID) {}

  std::unordered_map<BasicBlock *, BasicBlock *> copyLoop(BasicBlock *Header,
                                                          BasicBlock *InsertBefore) {
    std::unordered_map<Value *, Value *> Mapping;
    std::unordered_map<BasicBlock *, BasicBlock *> BasicBlocksMapping;
    Instruction *Clone;
    IRBuilder<> Builder(InsertBefore);
    BasicBlock *NewBasicBlock;

    for (BasicBlock *BB : LoopBasicBlocks) {
      if (BB == Header)
        continue;
      NewBasicBlock = BasicBlock::Create(InsertBefore->getContext(), "",
                                         InsertBefore->getParent(), InsertBefore);
      BasicBlocksMapping[BB] = NewBasicBlock;
    }

    for (BasicBlock *BB : LoopBasicBlocks) {
      if (BB == Header)
        continue;
      NewBasicBlock = BasicBlocksMapping[BB];
      Builder.SetInsertPoint(NewBasicBlock);
      for (Instruction &I : *BB) {
        Clone = I.clone();
        Mapping[&I] = Clone;
        Builder.Insert(Clone);

        for (unsigned i = 0; i < Clone->getNumOperands(); i++) {
          Value *Operand = Clone->getOperand(i);
          if (Mapping.find(Operand) != Mapping.end()) {
            Clone->setOperand(i, Mapping[Operand]);
          }
        }
      }
    }

    for (auto &Pair : BasicBlocksMapping) {
      BasicBlock *NewBB = Pair.second;
      Instruction *Term = NewBB->getTerminator();
      for (unsigned i = 0; i < Term->getNumSuccessors(); i++) {
        BasicBlock *Successor = Term->getSuccessor(i);
        if (BasicBlocksMapping.find(Successor) != BasicBlocksMapping.end()) {
          Term->setSuccessor(i, BasicBlocksMapping[Successor]);
        }
      }
    }

    return BasicBlocksMapping;
  }

  std::vector<BasicBlock *> collectLoopBlocks(BasicBlock *Header,
                                              BasicBlock *BodyEntry) {
    std::vector<BasicBlock *> Blocks;
    std::unordered_set<BasicBlock *> Visited;
    std::vector<BasicBlock *> Stack;

    Visited.insert(Header);
    Blocks.push_back(Header);
    Stack.push_back(BodyEntry);

    while (!Stack.empty()) {
      BasicBlock *BB = Stack.back();
      Stack.pop_back();
      if (!Visited.insert(BB).second)
        continue;
      Blocks.push_back(BB);

      Instruction *Term = BB->getTerminator();
      for (unsigned i = 0; i < Term->getNumSuccessors(); i++) {
        BasicBlock *Succ = Term->getSuccessor(i);
        if (Succ != Header)
          Stack.push_back(Succ);
      }
    }
    return Blocks;
  }

  BasicBlock *findLatch(BasicBlock *Header, BasicBlock *BodyEntry) {
    std::unordered_set<BasicBlock *> Visited;
    std::vector<BasicBlock *> Stack;
    Stack.push_back(BodyEntry);

    while (!Stack.empty()) {
      BasicBlock *BB = Stack.back();
      Stack.pop_back();

      if (BB == Header || !Visited.insert(BB).second)
        continue;

      Instruction *Term = BB->getTerminator();
      for (unsigned i = 0; i < Term->getNumSuccessors(); i++) {
        BasicBlock *Succ = Term->getSuccessor(i);
        if (Succ == Header)
          return BB;
        Stack.push_back(Succ);
      }
    }
    return nullptr;
  }

  BasicBlock *otherPredecessor(BasicBlock *BB, BasicBlock *Not) {
    for (BasicBlock *Pred : predecessors(BB))
      if (Pred != Not)
        return Pred;
    return nullptr;
  }

  BasicBlock *conditionalPredecessor(BasicBlock *BB) {
    for (BasicBlock *Pred : predecessors(BB)) {
      BranchInst *Br = dyn_cast<BranchInst>(Pred->getTerminator());
      if (Br && Br->isConditional())
        return Pred;
    }
    return nullptr;
  }

  void redirect(Instruction *Term, BasicBlock *From, BasicBlock *To) {
    for (unsigned i = 0; i < Term->getNumSuccessors(); i++)
      if (Term->getSuccessor(i) == From)
        Term->setSuccessor(i, To);
  }

  void hoistInstructions(BasicBlock *From, BasicBlock *Into) {
    Instruction *InsertPoint = Into->getTerminator();
    while (&From->front() != From->getTerminator()) {
      Instruction *I = &From->front();
      I->moveBefore(InsertPoint);
    }
  }

  ConstantInt *getLoopBound(BasicBlock *Header) {
    for (Instruction &I : *Header)
      if (ICmpInst *Cmp = dyn_cast<ICmpInst>(&I))
        return dyn_cast<ConstantInt>(Cmp->getOperand(1));
    return nullptr;
  }

  bool sameIterationCount(BasicBlock *PrevHeader, BasicBlock *CurHeader) {
    ConstantInt *PrevBound = getLoopBound(PrevHeader);
    ConstantInt *CurBound = getLoopBound(CurHeader);
    if (!PrevBound || !CurBound)
      return false;
    return PrevBound->getSExtValue() == CurBound->getSExtValue();
  }

  bool bodiesAreDependent(std::vector<BasicBlock *> &PrevBlocks,
                          std::vector<BasicBlock *> &CurBlocks) {
    std::unordered_set<Value *> WrittenByPrev;
    for (BasicBlock *BB : PrevBlocks)
      for (Instruction &I : *BB)
        if (StoreInst *Store = dyn_cast<StoreInst>(&I))
          WrittenByPrev.insert(Store->getPointerOperand());

    for (BasicBlock *BB : CurBlocks)
      for (Instruction &I : *BB) {
        if (LoadInst *Load = dyn_cast<LoadInst>(&I))
          if (WrittenByPrev.count(Load->getPointerOperand()))
            return true;
        if (StoreInst *Store = dyn_cast<StoreInst>(&I))
          if (WrittenByPrev.count(Store->getPointerOperand()))
            return true;
      }
    return false;
  }

  void deleteDeadLoop(BasicBlock *Preheader) {
    std::vector<BasicBlock *> Blocks = LoopBasicBlocks;
    Blocks.push_back(Preheader);

    for (BasicBlock *BB : Blocks)
      BB->dropAllReferences();

    for (BasicBlock *BB : Blocks)
      BB->eraseFromParent();
  }

  void loopFusion(BasicBlock *CurHeader, BasicBlock *CurBodyEntry,
                  BasicBlock *CurExit, BasicBlock *CurLatch,
                  BasicBlock *CurPreheader, BasicBlock *PrevHeader,
                  BasicBlock *PrevLatch, BasicBlock *PrevPreheader) {
    std::unordered_map<BasicBlock *, BasicBlock *> Mapping =
        copyLoop(CurHeader, CurExit);
    BasicBlock *CopyBodyEntry = Mapping[CurBodyEntry];
    BasicBlock *CopyLatch = Mapping[CurLatch];

    hoistInstructions(CurPreheader, PrevPreheader);

    redirect(PrevLatch->getTerminator(), PrevHeader, CopyBodyEntry);
    redirect(CopyLatch->getTerminator(), CurHeader, PrevHeader);
    redirect(PrevHeader->getTerminator(), CurPreheader, CurExit);

    deleteDeadLoop(CurPreheader);
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {
    BasicBlock *CurHeader = L->getHeader();
    BranchInst *CurBr = dyn_cast<BranchInst>(CurHeader->getTerminator());
    if (!CurBr || !CurBr->isConditional())
      return false;

    BasicBlock *CurBodyEntry = CurBr->getSuccessor(0);
    BasicBlock *CurExit = CurBr->getSuccessor(1);
    LoopBasicBlocks = collectLoopBlocks(CurHeader, CurBodyEntry);
    BasicBlock *CurLatch = findLatch(CurHeader, CurBodyEntry);
    if (!CurLatch)
      return false;
    BasicBlock *CurPreheader = otherPredecessor(CurHeader, CurLatch);
    if (!CurPreheader)
      return false;

    BasicBlock *PrevHeader = conditionalPredecessor(CurPreheader);
    if (!PrevHeader)
      return false;

    BranchInst *PrevBr = dyn_cast<BranchInst>(PrevHeader->getTerminator());
    if (!PrevBr || !PrevBr->isConditional())
      return false;
    BasicBlock *PrevBodyEntry = PrevBr->getSuccessor(0);
    BasicBlock *PrevLatch = findLatch(PrevHeader, PrevBodyEntry);
    if (!PrevLatch)
      return false;
    BasicBlock *PrevPreheader = otherPredecessor(PrevHeader, PrevLatch);
    if (!PrevPreheader)
      return false;

    if (!sameIterationCount(PrevHeader, CurHeader))
      return false;

    std::vector<BasicBlock *> PrevBlocks =
        collectLoopBlocks(PrevHeader, PrevBodyEntry);
    if (bodiesAreDependent(PrevBlocks, LoopBasicBlocks))
      return false;

    loopFusion(CurHeader, CurBodyEntry, CurExit, CurLatch, CurPreheader,
               PrevHeader, PrevLatch, PrevPreheader);
    return true;
  }
};
} // namespace

char OurLoopFusionPass::ID = 0;
static RegisterPass<OurLoopFusionPass>
    X("our-loop-fusion-pass", "Our Loop Fusion Pass", false, false);

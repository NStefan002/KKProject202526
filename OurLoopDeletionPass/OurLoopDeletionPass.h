#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace {
class OurLoopDeletionPass : public LoopPass {
private:
  bool isUsedAfterLoop(Loop *L, const Value *Var);
  bool isLoopDead(Loop *L);
  void deleteLoop(Loop *L);

public:
  static char ID;
  OurLoopDeletionPass() : LoopPass(ID) {}

  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
};
} // namespace

char OurLoopDeletionPass::ID = 0;

static RegisterPass<OurLoopDeletionPass>
    X("our-loop-deletion", "Our Loop Deletion Pass", false, false);

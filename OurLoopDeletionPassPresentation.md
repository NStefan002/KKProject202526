---
author: Stefan Novaković (43/2021), Vojkan Panić (138/2019)
date: MMMM dd, YYYY
paging: "%d / %d"
---

# Loop Deletion Pass

## Projekat iz predmeta `Konstrukcija kompilatora`

---

## Uvod

- Šta radi optimizacija?
- Detalji implementacije
- Test primeri

---

## Šta radi optimizacija?

- Pokreće se na svim pojedinacnim petljama u kodu
- Odredjuje da li je petlja 'mrtva' (dead) ili ne
- Uklanja mrtve petlje iz koda

---

## Šta radi optimizacija?

### Pre

```c
int main()
{
    for (int i = 0; i < 5; i++)
    {
        int x = i * 2; // x se ne koristi nakon petlje
    }
    return 0;
}
```

### Posle

```c
int main()
{
    return 0;
}
```

---

## Šta radi optimizacija?

### Šta čini petlju 'mrtvom'?

- Petlja nije beskonačna
- Petlja ne menja promenljive koje se koriste van petlje
- Petlja ne menja stanje programa kroz funkcije koje se pozivaju unutar petlje
- Petlja ne sadrži unutrašnje petlje koje nisu mrtve

---

## Šta radi optimizacija?

### Primer mrtve petlje

```c
int main() {
    int x = 0;
    for (int i = 0; i < 10; i++) {
        x = i; // promenljiva x se ne koristi nakon petlje
    }
}
```

1. Petlja nije beskonačna
2. Petlja ne menja stanje programa (promenljive `i` i `x` se ne koristi nakon petlje)
3. Petlja ne menja stanje programa kroz funkcije koje se pozivaju unutar petlje
4. Petlja ne sadrži unutrašnje petlje koje nisu mrtve

---

## Detalji implementacije

### Registrujemo optimizaciju

```cpp
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
```

---

## Detalji implementacije

### Optimizacija se pokreće na svim pojedinačnim petljama u kodu

```cpp
bool OurLoopDeletionPass::runOnLoop(Loop *L, LPPassManager &LPM) {
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
  LPM.markLoopAsDeleted(*L);

  return true;
}
```

---

## Detalji implementacije

### Provera da li je petlja mrtva

#### Pojednostavljena provera da li je petlja beskonačna

```cpp
bool OurLoopDeletionPass::isLoopDead(Loop *L) {
  if (!L->isLoopSimplifyForm()) {
    errs() << "Loop is not in simplified form.\n";
    return false;
  }

  if (!L->getExitBlock()) {
    errs() << "Loop does not have a single exit block.\n";
    return false;
  }
  // ...
}
```

---

## Detalji implementacije

### Provera da li je petlja mrtva

#### Provera da li petlja menja stanje programa (kroz funkcije koje se pozivaju unutar petlje)

```cpp
bool OurLoopDeletionPass::isLoopDead(Loop *L) {
  // ...
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
  // ...
}
```

---

## Detalji implementacije

### Provera da li je petlja mrtva

#### Provera da li petlja menja stanje programa (promenljive koje se koriste van petlje)

```cpp
bool OurLoopDeletionPass::isLoopDead(Loop *L) {
  // ...
  std::unordered_set<const Value *> VarsAlteredInLoop;
  // ...
  for (const Value *Var : VarsAlteredInLoop) {
    if (isUsedAfterLoop(L, Var)) {
      errs() << "Variable " << *Var
             << " is altered in the loop and used after the loop.\n";
      return false;
    }
  }
  return true;
}
```

---

## Detalji implementacije

### Provera da li je petlja mrtva

#### Pomoćna funkcija koja proverava da li se promenljiva koristi nakon petlje

```cpp
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
```

---

## Detalji implementacije

### Brisanje mrtve petlje

```cpp
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();
  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  // 1. Replace the terminator of the preheader to jump to the exit block
  Preheader->getTerminator()->replaceUsesOfWith(Header, ExitBlock);

  // ...
}
```

---

## Detalji implementacije

### Brisanje mrtve petlje

```cpp
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();
  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  // 1. ...

  // 2. Remove blocks from parent loops
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

  // ...
}
```

---

## Detalji implementacije

### Brisanje mrtve petlje

```cpp
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();
  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  // 1. ...

  // 2. ...

  // 3. Drop all references to the blocks
  for (BasicBlock *BB : Blocks) {
    BB->dropAllReferences();
  }

  // ...
}
```

---

## Detalji implementacije

### Brisanje mrtve petlje

```cpp
void OurLoopDeletionPass::deleteLoop(Loop *L) {
  BasicBlock *Preheader = L->getLoopPreheader();
  BasicBlock *Header = L->getHeader();
  BasicBlock *ExitBlock = L->getExitBlock();
  std::vector<BasicBlock *> Blocks(L->blocks().begin(), L->blocks().end());

  // 1. ...

  // 2. ...

  // 3. ...

  // 4. Erase the blocks from the function
  for (BasicBlock *BB : Blocks) {
    BB->eraseFromParent();
  }
}
```

---

## Test primeri

### Primer 1: Petlja sa promenljivom koja se ne koristi nakon petlje

```c
int main()
{
    for (int i = 0; i < 5; i++)
    {
        int x = i * 2;
    }
    return 0;
}
```

---

## Test primeri

### Primer 2: Ugnjezdena petlja koja je mrtva

```c
int main()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            int x = i + j;
        }
    }
    return 0;
}
```

**Napomena:** Nakon uklanjanja unutrašnje petlje, spoljašnja petlja postaje mrtva i takođe se uklanja.

---

## Test primeri

### Primer 3: Promenljiva iz petlje se koristi nakon petlje

```c
#include <stdio.h>

int main()
{
    int sum = 0;
    for (int i = 0; i < 5; i++)
    {
        sum += i;
    }
    printf("%d\n", sum); // sum escapes
    return 0;
}
```

---

## Test primeri

### Primer 4: Petlja (potencijalno) ima bočne efekte

```c
#include <stdio.h>

int main()
{
    for (int i = 0; i < 5; i++)
    {
        int x;
        scanf("%d", &x); // !!!
    }
    return 0;
}
```

---

## Test primeri

### Primer 5: Globalne promenljive

```c
int g = 0;
int h = 0;

void f() { g++; }

int main()
{
    for (int i = 0; i < 5; i++)
        g += i;

    for (int i = 0; i < 5; i++)
        h++;

    return 0;
}
```

Prva petlja nije mrtva jer menja globalnu promenljivu `g`, koja se koristi van
petlje. Druga petlja je mrtva jer menja globalnu promenljivu `h`, koja se ne
koristi nigde drugde u programu.

---

## Test primeri

### Primer 6: Beskonačna petlja

```c
int main()
{
    for (;;)
    {
    }
}
```

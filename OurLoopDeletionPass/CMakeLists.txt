add_llvm_library( LLVMOurLoopDeletionPass MODULE BUILDTREE_ONLY
  OurLoopDeletionPass.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )

# CPM Package Lock
# This file should be committed to version control

# Boost
CPMDeclarePackage(Boost
  NAME Boost
  VERSION 1.80.0
  GIT_TAG boost-1.80.0
  GITHUB_REPOSITORY boostorg/boost
)
# cpp-udecimal
CPMDeclarePackage(cpp-udecimal
  NAME cpp-udecimal
  VERSION 0.2.1
  GIT_TAG v0.2.1
  GITHUB_REPOSITORY geseq/cpp-udecimal
)
# cpp-fastchan
CPMDeclarePackage(cpp-fastchan
  VERSION 0.2.0
  GITHUB_REPOSITORY geseq/cpp-fastchan
  EXCLUDE_FROM_ALL YES
)

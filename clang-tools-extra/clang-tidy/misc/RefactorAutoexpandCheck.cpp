//===--- RefactorAutoexpandCheck.cpp - clang-tidy -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/* Example of clang refactoring:

Use clang-query to explore AST
  clang-query.exe src\parsing\C_structures\LE\ble_Data.cpp -p build-clang
The folowing settings may be helpful:
  set output dump
  set traversal IgnoreUnlessSpelledInSource

To setup new clang-tidy helper
  git clone https://github.com/llvm/llvm-project.git
  cd llvm-project\
  mkdir build
  cd build
  vcvars64
  cmake ../llvm -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DLLVM_INCLUDE_TESTS=OFF  -DLLVM_BUILD_TOOLS=OFF  -DLLVM_INCLUDE_UTILS=OFF  -DLLVM_TARGETS_TO_BUILD="" -DCLANG_ENABLE_STATIC_ANALYZER=OFF  -DCLANG_ENABLE_ARCMT=OFF
  cd ..\clang-tools-extra\clang-tidy\
  python add_new_check.py misc refactor-autoexpand
Copy over this source file to misc/
  cd ..\..\build
  ninja "bin\clang-tidy.exe"

to run:
  "bin\clang-tidy.exe"  "-checks=-\*,misc-refactor-autoexpand" -p ..\..\..\sniffer\build-clang\compile_commands.json --fix C:\Users\pat\Repos\sniffer\src\parsing\analysis\LE\ble_connections.cpp
or to run on everything:
  python ..\thirdparty\llvm-project\clang-tools-extra\clang-tidy\tool\run-clang-tidy.py -clang-tidy-binary ../thirdparty/llvm-project/build/bin/clang-tidy.exe -p build-clang -checks=-*,misc-refactor-autoexpand -format -j8 -fix src\\parsing

*/

#include "RefactorAutoexpandCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

#include <type_traits>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace misc {

auto matchPointerByName(std::string s) {
  return pointerType(pointee(hasUnqualifiedDesugaredType(recordType(
      hasDeclaration(cxxRecordDecl(isSameOrDerivedFrom(matchesName(s))))))));
}

void RefactorAutoexpandCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      traverse(
          TK_IgnoreUnlessSpelledInSource,
          cxxConstructExpr(
              hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
              hasAnyArgument(declRefExpr(to(varDecl(hasName("AutoExpand"))))
                                 .bind("ae_arg")))
              .bind("ae_call")),
      this);
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               cxxConstructExpr(
                   hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
                   hasAnyArgument(
                       expr(hasType(cxxRecordDecl(matchesName("Decoration"))))
                           .bind("dec_arg")))
                   .bind("dec_call")),
      this);
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               cxxConstructExpr(
                   hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
                   hasAnyArgument(
                       expr(hasType(cxxRecordDecl(matchesName("Icon"))))
                           .bind("dec_arg")))
                   .bind("dec_call")),
      this);
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               cxxConstructExpr(
                   hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
                   hasAnyArgument(
                       expr(hasType(enumDecl(matchesName("error_e"))))
                           .bind("err_arg")))
                   .bind("err_call")),
      this);
  Finder->addMatcher(
      traverse(TK_IgnoreUnlessSpelledInSource,
               cxxConstructExpr(
                   hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
                   hasAnyArgument(
                       expr(hasType(cxxRecordDecl(matchesName("timePoint"))))
                           .bind("tp_arg")))
                   .bind("tp_call")),
      this);
  Finder->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
          hasAnyArgument(
              expr(hasType(matchPointerByName("Connection"))).bind("conn_arg")))
          .bind("conn_call"),
      this);
  Finder->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
          hasAnyArgument(
              expr(hasType(matchPointerByName("Device"))).bind("dev_arg")))
          .bind("dev_call"),
      this);

  Finder->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
          hasAnyArgument(expr(hasType(classTemplateSpecializationDecl(
                                  matchesName("vector"),
                                  hasAnyTemplateArgument(refersToType(
                                      matchPointerByName("Device"))))))
                             .bind("vdev_arg")))
          .bind("vdev_call"),
      this);

  Finder->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(hasName("treeItem"))),
          hasAnyArgument(expr(hasType(classTemplateSpecializationDecl(
                                  matchesName("vector"),
                                  hasAnyTemplateArgument(refersToType(
                                      matchPointerByName("Connection"))))))
                             .bind("vconn_arg")))
          .bind("vconn_call"),
      this);
}

void RefactorAutoexpandCheck::check(const MatchFinder::MatchResult &Result) {
  if (const auto *MatchedArg = Result.Nodes.getNodeAs<DeclRefExpr>("ae_arg")) {
    const auto *MatchedCall =
        Result.Nodes.getNodeAs<CXXConstructExpr>("ae_call");
    auto End = MatchedArg->getEndLoc();
    if (*Result.SourceManager->getCharacterData(End) == ',')
      End = End.getLocWithOffset(1);
    if (*Result.SourceManager->getCharacterData(End) == ' ')
      End = End.getLocWithOffset(1);
    diag(MatchedArg->getLocation(), "move AutoExpand")
        << FixItHint::CreateRemoval(SourceRange(MatchedArg->getBeginLoc(), End))
        << FixItHint::CreateInsertion(
               MatchedCall->getEndLoc().getLocWithOffset(1),
               ".set_default_expanded()");
  }

  auto Replace = [&](auto Arg, auto Call, auto Set) {
    if (const auto *MatchedArg = Result.Nodes.getNodeAs<Expr>(Arg)) {
      const auto *MatchedCall = Result.Nodes.getNodeAs<CXXConstructExpr>(Call);
      auto ArgRange = MatchedArg->getSourceRange();
      std::string S =
          Lexer::getSourceText(CharSourceRange(ArgRange, true),
                               *Result.SourceManager, clang::LangOptions{})
              .str();
      auto End = ArgRange.getEnd();
      if (*Result.SourceManager->getCharacterData(End) == ',')
        End = End.getLocWithOffset(1);
      if (*Result.SourceManager->getCharacterData(End) == ' ')
        End = End.getLocWithOffset(1);
      diag(MatchedArg->getExprLoc(), "replace")
          << FixItHint::CreateRemoval(
                 SourceRange(MatchedArg->getBeginLoc(), End))
          << FixItHint::CreateInsertion(
                 MatchedCall->getEndLoc().getLocWithOffset(1), Set + S + ")");
    }
  };
  Replace("dec_arg", "dec_call", ".set_decoration(");
  Replace("err_arg", "err_call", ".set_error(");
  Replace("tp_arg", "tp_call", ".set_time(");
  Replace("dev_arg", "dev_call", ".add_device(");
  Replace("conn_arg", "conn_call", ".add_connection(");
  Replace("vdev_arg", "vdev_call", ".add_device(");
  Replace("vconn_arg", "vconn_call", ".add_connection(");
}

} // namespace misc
} // namespace tidy
} // namespace clang

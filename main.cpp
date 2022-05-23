#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

#include <iostream>
#include <sstream>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class CastCallBack : public MatchFinder::MatchCallback {
 private:
  Rewriter *rewriter;

 public:
  CastCallBack(Rewriter &_rewriter) {rewriter = &_rewriter;};

  void run(const MatchFinder::MatchResult &Result) override {
    const auto *styleCastExpr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");

    auto lParenLoc = styleCastExpr->getLParenLoc();
    rewriter->RemoveText(lParenLoc, 1);

    auto rParenLoc = styleCastExpr->getRParenLoc();
    rewriter->RemoveText(rParenLoc, 1);

    rewriter->InsertText(lParenLoc, "static_cast<");
    rewriter->InsertText(rParenLoc, ">(");

    auto endLoc = Lexer::getLocForEndOfToken(
        styleCastExpr->getEndLoc(), 0, (*Result.SourceManager), LangOptions());
    rewriter->InsertText(endLoc, ")");
  }
};

class MyASTConsumer : public ASTConsumer {
 public:
  MyASTConsumer(Rewriter &rewriter) : callback_(rewriter) {
    matcher_.addMatcher(
        cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast"),
        &callback_);
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    matcher_.matchAST(Context);
  }

 private:
  CastCallBack callback_;
  MatchFinder matcher_;
};

class CStyleCheckerFrontendAction : public ASTFrontendAction {
 public:
  CStyleCheckerFrontendAction() = default;

  void EndSourceFileAction() override {
    rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance &CI, StringRef /* file */) override {
    rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<MyASTConsumer>(rewriter_);
  }

 private:
  Rewriter rewriter_;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char **argv) {
  // Ubuntu 20
  auto Parser = llvm::ExitOnError()(
      CommonOptionsParser::create(argc, argv, CastMatcherCategory));
  // For Ubuntu 18, use the old Clang API to construct the CommonOptionsParser:
  // CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

  ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
  return Tool.run(
      newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}

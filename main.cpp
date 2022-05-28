#include <iostream>
#include <sstream>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class CastCallBack : public MatchFinder::MatchCallback {

private:
  Rewriter& _rewriter;

public:
  CastCallBack(Rewriter &rewriter) : _rewriter(rewriter){};
  void run(const MatchFinder::MatchResult &Result) override {

    const auto *castExpr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
    
    const auto range = CharSourceRange::getCharRange(
        castExpr->getLParenLoc(),
        castExpr->getSubExprAsWritten()->getBeginLoc());

    const auto &sourceManager = *Result.SourceManager;

    const auto type = Lexer::getSourceText(
        CharSourceRange::getTokenRange(
            castExpr->getLParenLoc().getLocWithOffset(1),
            castExpr->getRParenLoc().getLocWithOffset(-1)),
                             sourceManager, LangOptions());

    const auto *expr = castExpr->getSubExprAsWritten()->IgnoreImpCasts();
    auto newText = ("static_cast<" + type + ">").str();

    if (!isa<ParenExpr>(castExpr->getSubExprAsWritten()->IgnoreImpCasts())) {
      newText.append("(");
    }
    
    const auto endLoc = Lexer::getLocForEndOfToken(expr->getEndLoc(), 0,
                                               sourceManager, LangOptions());
    
    if (!isa<ParenExpr>(castExpr->getSubExprAsWritten()->IgnoreImpCasts())) {
      _rewriter.InsertText(endLoc, ")");
    }

    _rewriter.ReplaceText(range, newText);
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

  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef /* file */) override {
    rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<MyASTConsumer>(rewriter_);
  }

private:
  Rewriter rewriter_;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char **argv) {
  CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

  ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
  return Tool.run(
      newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
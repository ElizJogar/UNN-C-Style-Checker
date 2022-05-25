#include <iostream>
#include <sstream>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Frontend/CompilerInstance.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class CastCallBack : public MatchFinder::MatchCallback {
private:
  Rewriter& castRewriter;
public:
    CastCallBack(Rewriter& rewriter) : castRewriter(rewriter) {}

    void run(const MatchFinder::MatchResult &Result) override {
        const auto* expression = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
	SourceManager& sourceManager = *Result.SourceManager;
	if (expression->getExprLoc().isMacroID())
            return;
        if (expression->getCastKind() == CK_ToVoid)
            return;
      
    auto range = CharSourceRange::getCharRange(expression->getLParenLoc(),
    expression->getSubExprAsWritten()->getBeginLoc());
    
        auto sourceText = Lexer::getSourceText(CharSourceRange::getTokenRange(
                expression->getLParenLoc().getLocWithOffset(1),
                expression->getRParenLoc().getLocWithOffset(-1)),
                sourceManager, Result.Context->getLangOpts());
        auto replaceText = ("static_cast<" + sourceText + ">").str();
        auto castIgnore = expression->getSubExprAsWritten()->IgnoreImpCasts();
        if (!isa<ParenExpr>(castIgnore)) {
            replaceText.push_back('(');
            castRewriter.InsertText(Lexer::getLocForEndOfToken(castIgnore->getEndLoc(),
                0, *Result.SourceManager,
                Result.Context->getLangOpts()), ")");
        }
        castRewriter.ReplaceText(range, replaceText);
    }
};

class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(Rewriter &rewriter) : callback_(rewriter) {
        matcher_.addMatcher(
                cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast"), &callback_);
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

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef /* file */) override {
        rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MyASTConsumer>(rewriter_);
    }

private:
    Rewriter rewriter_;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char **argv) {
    // Ubuntu 20
    auto Parser = llvm::ExitOnError()(CommonOptionsParser::create(argc, argv, CastMatcherCategory));
    // For Ubuntu 18, use the old Clang API to construct the CommonOptionsParser:
    // CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}

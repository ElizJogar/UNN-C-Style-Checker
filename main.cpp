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
public:
    CastCallBack(Rewriter& _rewriter) : rewriter(_rewriter) {};

    void run(const MatchFinder::MatchResult &Result) override {
        if (auto *expr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")) {
            auto swap_range = CharSourceRange::getCharRange(
                expr->getLParenLoc(), expr->getSubExprAsWritten()->getBeginLoc());

            auto type = Lexer::getSourceText(CharSourceRange::getTokenRange(expr->getLParenLoc().getLocWithOffset(1),
                expr->getRParenLoc().getLocWithOffset(-1)), *Result.SourceManager, Result.Context->getLangOpts());

            std::string true_str = ("static_cast<" + type + ">").str();
            if (!isa<ParenExpr>(expr->getSubExprAsWritten())) {
              true_str = ("static_cast<" + type + ">(").str();
              rewriter.InsertText(Lexer::getLocForEndOfToken(expr->getEndLoc(), 0,
                                  *Result.SourceManager, Result.Context->getLangOpts()), ")");
            }
            rewriter.ReplaceText(swap_range, true_str);
        }
    }

private:
    Rewriter& rewriter;
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

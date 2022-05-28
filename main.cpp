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
    CastCallBack(Rewriter& rewriter) : _rewriter(rewriter) {};

    void run(const MatchFinder::MatchResult &Result) override {
        const auto* CExp = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");

        SourceManager& SourMan = *Result.SourceManager;

        auto ReplaceRange = CharSourceRange::getCharRange(
            CExp->getLParenLoc(),
            CExp->getSubExprAsWritten()->getBeginLoc());

        StringRef DestTypeString = Lexer::getSourceText(CharSourceRange::getTokenRange(
            CExp->getLParenLoc().getLocWithOffset(1),
            CExp->getRParenLoc().getLocWithOffset(-1)),
            SourMan, Result.Context->getLangOpts());

        const Expr* Expression = CExp->getSubExprAsWritten()->IgnoreImpCasts();

        std::string CastReplaceText = ("static_cast<" + DestTypeString + ">").str();

        if (!isa<ParenExpr>(Expression)) {
            CastReplaceText.push_back('(');

            _rewriter.InsertText(Lexer::getLocForEndOfToken(Expression->getEndLoc(),
                0,
                *Result.SourceManager,
                Result.Context->getLangOpts()),
                ")");
        }
        _rewriter.ReplaceText(ReplaceRange, CastReplaceText);
    }
 private:
    Rewriter& _rewriter;	
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

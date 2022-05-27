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
    CastCallBack(Rewriter& rewriter) : rewriter_(rewriter) {}

    void run(const MatchFinder::MatchResult &Result) override {
        // The matched c-cast statement was bound to 'cast'.
        if (const CStyleCastExpr *c_cast = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")) {
            // Get source location of cast type's parenthesis
            auto LLoc = c_cast->getLParenLoc();
            auto RLoc = c_cast->getRParenLoc();
            // Transform (type) -> static_cast<type>
            rewriter_.ReplaceText(LLoc, 1, "static_cast<");
            rewriter_.ReplaceText(RLoc, 1, ">");

            // Cover a case when an expression to cast is not enclosed in parentheses
            auto cust_exp_beg = c_cast->getSubExprAsWritten()->getBeginLoc();
            auto cust_exp_end = c_cast->getSubExprAsWritten()->getEndLoc();
            if (Result.SourceManager->getCharacterData(cust_exp_beg)[0] != '(') {
                rewriter_.InsertText(cust_exp_beg, "(");
                rewriter_.InsertTextAfterToken(cust_exp_end, ")");
            }
        }
    }

private:
    Rewriter& rewriter_;
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

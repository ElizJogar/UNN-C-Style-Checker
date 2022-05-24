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
    Rewriter& mRewriter;

public:
    CastCallBack(Rewriter& rewriter) : mRewriter(rewriter) {

    };

    void run(const MatchFinder::MatchResult& Result) override {
        auto castNode = Result.Nodes.getNodeAs<clang::CStyleCastExpr>("cast"); // Get AST node bind to id = "cast"
        auto leftSL = castNode->getLParenLoc(); // Get left SourceLocation
        auto rightSL = castNode->getRParenLoc(); // Get right SourceLocation
        auto endSL = castNode->getEndLoc(); // Get SourceLocation on end
        
        // Initial source = "(int)f"

        mRewriter.RemoveText(leftSL, 1); // Remove '(' --> "int)f"
        mRewriter.InsertText(leftSL, "static_cast<"); // Insert "static_cast<" --> "static_cast<int)f"
        mRewriter.RemoveText(rightSL, 1); // Remove ')' --> "static_cast<intf"
        mRewriter.InsertText(rightSL, ">("); // Insert ">(" --> "static_cast<int>(f"
        mRewriter.InsertText(endSL.getLocWithOffset(1), ")"); // Insert ")" behind "f" --> "static_cast<int>(f)"
    }
};

class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(Rewriter& rewriter) : callback_(rewriter) {
        matcher_.addMatcher(
            cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast"),
            &callback_);
    }

    void HandleTranslationUnit(ASTContext& Context) override {
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
        CompilerInstance& CI,
        StringRef /* file */) override {
        rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MyASTConsumer>(rewriter_);
    }

private:
    Rewriter rewriter_;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char** argv) {
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}

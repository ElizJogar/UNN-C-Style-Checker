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
      // Get C style cast expression pointer and check for non-nullptr
      auto c_style_cast_expr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
      if (c_style_cast_expr) {
        // Get source location of cast type's left parenthesis
        auto left_paren = c_style_cast_expr->getLParenLoc();
        // Get source location of cast type's right parenthesis
        auto right_paren = c_style_cast_expr->getRParenLoc();
        // Replace left parenthesis with `static_cast<`
        rewriter_.ReplaceText(left_paren, 1, "static_cast<");
        // Replace right parenthesis with `>`
        rewriter_.ReplaceText(right_paren, 1, ">");
        // Get location of first end last token
        // in cast subexpression
        auto first = c_style_cast_expr->getSubExprAsWritten()->getBeginLoc();
        auto last = c_style_cast_expr->getSubExprAsWritten()->getEndLoc();
        // If needed, add parenthesis before first
        // and after last token in cast subexpression
        if ('(' != Result.SourceManager->getCharacterData(first)[0]
          || ')' != Result.SourceManager->getCharacterData(last)[0]) {
          rewriter_.InsertText(first, "(");
          rewriter_.InsertTextAfterToken(last, ")");
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

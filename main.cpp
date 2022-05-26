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
    CastCallBack(Rewriter& _rewriter):rewriter(_rewriter) {
        // Your code goes here
    };

    void run(const MatchFinder::MatchResult &Result) override {
        auto csce = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
	if((csce!= nullptr)&&(!csce->getExprLoc().isMacroID())){
	auto  csce_gs = csce->getSubExprAsWritten()->IgnoreImpCasts();
	auto var_end = Lexer::getLocForEndOfToken(csce_gs->getEndLoc(), 0, *Result.SourceManager, LangOptions());
	auto var_start = csce->getSubExprAsWritten()->getBeginLoc();
	auto l_brace = csce->getLParenLoc();
	auto r_brace = csce->getRParenLoc();
	rewriter.ReplaceText(l_brace,1,"static_cast<");
	rewriter.ReplaceText(CharSourceRange::getCharRange(r_brace,var_start),">");
	if(!isa<ParenExpr>(csce_gs)){
	rewriter.InsertText(var_start, "(");
	rewriter.InsertText(var_end, ")");
	}
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
	std::error_code e_r;
	llvm::raw_fd_ostream outputFile("../test/afterASTversion.cpp",e_r,llvm::sys::fs::OF_None);
        rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID())
            .write(outputFile);
	outputFile.close();
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

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
    CastCallBack(Rewriter& rewriter) : rewriter(rewriter) {}

    void run(const MatchFinder::MatchResult &Result) override {
		if (const auto *cSCExpr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")) {
			const auto& SM = *Result.SourceManager;
			auto& FM = SM.getFileManager();
			
			const auto& LParenLoc = cSCExpr->getLParenLoc();
			const auto& RParenLoc = cSCExpr->getRParenLoc();
			
			// change to static_cast
			rewriter.ReplaceText(LParenLoc, 1, "static_cast<");
			rewriter.ReplaceText(RParenLoc, 1, ">");
			
			// go to DeclRefExpr
			const auto implicitFirst = cSCExpr->child_begin();
			const auto implicitSecond = implicitFirst->child_begin();
			const auto declExpIter = implicitSecond->child_begin();
			const DeclRefExpr* declExp = (DeclRefExpr*)(*declExpIter);
			
			const auto& declExpLoc = declExp->getBeginLoc();
			rewriter.InsertText(declExpLoc, "(", false, false);
			
			// get name of variable
			const auto varName = declExp->getNameInfo().getAsString();
			
			// create new SourceLocation with offset = varName.length()
			const FileEntry* fileEntry = *(FM.getFile(SM.getFilename(declExpLoc)));
			SourceLocation declExpEndLoc = SM.translateFileLineCol(fileEntry, SM.getSpellingLineNumber(declExpLoc), SM.getSpellingColumnNumber(declExpLoc) + varName.length());
			
			rewriter.InsertText(declExpEndLoc, ")", false, false);
			//llvm::outs() << SM.getFilename(loc) << ":" << SM.getSpellingLineNumber(loc) << ":" << SM.getSpellingColumnNumber(loc);
		}
    }
private:
	Rewriter &rewriter;
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
    auto Parser = llvm::ExitOnError()(CommonOptionsParser::create(argc, argv, CastMatcherCategory, llvm::cl::OneOrMore)); // worked for me
    // For Ubuntu 18, use the old Clang API to construct the CommonOptionsParser:
    // CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}

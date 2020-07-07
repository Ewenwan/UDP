#include <UDP/tools/AutoRefl.h>

#include <UANTLR/ParserCpp14/CPP14Lexer.h>
#include <UANTLR/ParserCpp14/CPP14Parser.h>

#include <iostream>
#include <sstream>

using namespace Ubpa;
using namespace std;
using namespace antlr4;

string AutoRefl::Parse(string_view code) {
	// [ 1. clear ]
	typeInfos.clear();
	curNamespace.clear();
	curMetas = nullptr;
	curTypeInfo = nullptr;
	curVarInfo = nullptr;
	curAccessSpecifier = AccessSpecifier::PRIVATE;

	// [2. parse]
	ANTLRInputStream input(code.data());
	CPP14Lexer lexer(&input);
	CommonTokenStream tokens(&lexer);

	CPP14Parser parser(&tokens);
	tree::ParseTree* tree = parser.translationunit();

	tree->accept(this);

	// [3. generate]
	stringstream ss;
	const string indent = "\t";
	
	ss
		<< "// This file is generated by AutoRefl" << endl
		<< endl
		<< "#pragma once" << endl
		<< endl
		<< "#include <UDP/Reflection/Reflection.h>" << endl
		<< endl
		<< "namespace Ubpa::AutoRefl {" << endl;

	for (const auto& typeinfo : typeInfos) {
		string ns;
		for (auto a_ns : typeinfo.ns)
			ns += a_ns + "::";
		string type = ns + typeinfo.name;
		string codetype;
		for (size_t i = 0; i < type.size(); i++) {
			if (type[i] == ':') {
				codetype += "_";
				i++;
			}
			else
				codetype += type[i];
		}

		ss
			<< indent << "void Register_" << codetype << "() {" << endl
			<< indent << indent << "Reflection<" << type << ">::Instance()" << endl;

		for (const auto& [key, value] : typeinfo.metas) {
			ss << indent << indent << indent
				<< ".Register(\""
				<< key << "\", "
				<< (value.empty() ? "\"\"" : value) << ")" << endl;
		}

		for (const auto& varInfo : typeinfo.varInfos) {
			if(varInfo.access != AccessSpecifier::PUBLIC)
				continue;

			ss << indent << indent << indent
				<< ".Register(&"
				<< type << "::"<< varInfo.name << ", \""
				<< varInfo.name << "\")" << endl;

			for (const auto& [key, value] : varInfo.metas) {
				ss << indent << indent << indent
					<< ".Register(\"" << varInfo.name << "\", \""
					<< key << "\", "
					<< (value.empty() ? "\"\"" : value) << ")" << endl;
			}
		}
		ss << indent << indent << indent << ";" << endl;

		ss
			<< indent << "}" << endl;
	}

	ss
		<< "}" << endl; // namespace Ubpa::AutoRefl

	return ss.str();
}

antlrcpp::Any AutoRefl::visitOriginalnamespacedefinition(CPP14Parser::OriginalnamespacedefinitionContext* ctx) {
	curNamespace.push_back(ctx->Identifier()->getText());
	auto result = visitChildren(ctx);
	curNamespace.pop_back();
	return result;
}

antlrcpp::Any AutoRefl::visitClassspecifier(CPP14Parser::ClassspecifierContext* ctx) {
	typeInfos.emplace_back();
	curTypeInfo = &typeInfos.back();
	curTypeInfo->ns = curNamespace;
	auto result = visitChildren(ctx);
	curTypeInfo = nullptr;
	return result;
}

antlrcpp::Any AutoRefl::visitClasshead(CPP14Parser::ClassheadContext* ctx) {
	curMetas = &curTypeInfo->metas;
	auto result = visitChildren(ctx);
	curMetas = nullptr;
	return result;
}

antlrcpp::Any AutoRefl::visitClasskey(CPP14Parser::ClasskeyContext* ctx) {
	curTypeInfo->classkey = ctx->getText();
	if(curTypeInfo->classkey == "struct")
		curAccessSpecifier = AccessSpecifier::PUBLIC;
	else
		curAccessSpecifier = AccessSpecifier::PRIVATE;

	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitClassname(CPP14Parser::ClassnameContext* ctx) {
	curTypeInfo->name = ctx->getText();
	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitAttribute(CPP14Parser::AttributeContext* ctx) {
	if (curMetas) {
		auto argCtx = ctx->attributeargumentclause();
		string arg = argCtx ? argCtx->balancedtokenseq()->getText() : "";
		curMetas->emplace(ctx->attributetoken()->getText(), move(arg));
	}
	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitAccessspecifier(CPP14Parser::AccessspecifierContext* ctx) {
	auto accessStr = ctx->getText();
	if (accessStr == "public")
		curAccessSpecifier = AccessSpecifier::PUBLIC;
	else if (accessStr == "protected")
		curAccessSpecifier = AccessSpecifier::PROTECTED;
	else
		curAccessSpecifier = AccessSpecifier::PRIVATE;
	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitMemberdeclaration(CPP14Parser::MemberdeclarationContext* ctx) {
	curTypeInfo->varInfos.emplace_back();
	curVarInfo = &curTypeInfo->varInfos.back();
	curVarInfo->access = curAccessSpecifier;
	curMetas = &curVarInfo->metas;
	auto result = visitChildren(ctx);
	curVarInfo = nullptr;
	curMetas = nullptr;
	return result;
}

antlrcpp::Any AutoRefl::visitNoptrdeclarator(CPP14Parser::NoptrdeclaratorContext* ctx) {
	if (!curVarInfo)
		return {};

	auto paramCtx = ctx->parametersandqualifiers();
	if (paramCtx) { // function
		typeInfos.back().varInfos.pop_back();
		return {};
	}

	// var
	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitDeclspecifier(CPP14Parser::DeclspecifierContext* ctx) {
	if (!curVarInfo)
		return visitChildren(ctx);

	if (!ctx->typespecifier())
		curVarInfo->specifiers.push_back(ctx->getText());

	return visitChildren(ctx);
}

antlrcpp::Any AutoRefl::visitUnqualifiedid(CPP14Parser::UnqualifiedidContext* ctx) {
	if (!curVarInfo)
		return {};
	curVarInfo->name = ctx->getText();
	return visitChildren(ctx);
}

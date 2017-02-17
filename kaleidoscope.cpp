#include <iostream>
#include <cctype>
#include <vector>
#include <map>
/************************************************************************/
/*      Simple Kaleidoscope Programming Language Compiler 
/************************************************************************/


//-------------------------
//lexical analysis
//--------------------------


/* define Token enum type */
enum Token
{
	tok_eof = -1,
	
	tok_def = -2,			//def
	tok_extern = -3,		//extern 

	tok_identifier = -4,	//identifier 
	tok_number = -5			//numeric literal
};


static std::string IdentifierStr;		//current identifier string 
static double NumVal;					//current number 


/* return next token from standard input */
static int gettok() 
{
	static char LastChar = ' ';
	
	//skip the blank space 
	while (std::isspace(LastChar)) {
		LastChar = std::getchar();
	}

	if (std::isalpha(LastChar)) {		// identifier
		IdentifierStr = LastChar;		
		while (std::isalnum(LastChar = std::getchar())) {		//eat alpha and number to store in  identifierStr
			IdentifierStr += LastChar;
		}

		if (IdentifierStr == "def") {
			return tok_def;
		}
		if (IdentifierStr == "extern") {
			return tok_extern;
		}
		
		return tok_identifier;								
	}

	if (std::isalnum(LastChar) || LastChar == '.') {		//number 
		std::string NumStr;
		while (std::isalnum(LastChar) || LastChar == '.') {
			NumStr += LastChar;
			LastChar = std::getchar();
		}

		NumVal = std::strtod(NumStr.c_str(), 0);			//NumStr to NumVal 
		return tok_number;
	}

	if (LastChar == '#') {						//comment
		while (LastChar != EOF && LastChar != '\n') {
			LastChar = std::getchar();			//eat comment char until \n and eof
		}

		if (LastChar != EOF) {
			return gettok();					//skip comment and get next token
		}
	}

	if (LastChar == EOF) {						//end of file flag
		return tok_eof;
	}

	char ThisChar = LastChar;
	LastChar = std::getchar();					//update LastChar
	return ThisChar;
}



//-------------------------------
// Abstract Syntax Tree && Parser
//-------------------------------


/* base expression AST node class */
class ExprAST 
{
public:
	virtual ~ExprAST() {}
};


/* number node   --  subclass of ExprAST */
class NumberExprAST : public ExprAST 
{
private:
	double Val;
public:
	NumberExprAST(double val) : Val(val) {};
};


/* variable node -- subclass of ExprAST */
class VariableExprAST : public ExprAST 
{
private:
	std::string Name;
public:
	VariableExprAST(std::string &name) : Name(name) {};
};


/* binary operator expression -- subclass od ExprAST */
class BinaryExprAST : public ExprAST
{
private:
	char Op;				//operator 
	ExprAST* LHS;			//left side operand
	ExprAST* RHS;			//right side operand 
public:
	BinaryExprAST(char op, ExprAST* lhs, ExprAST* rhs) 
		: Op(op), LHS(lhs), RHS(rhs) {};
};


/* function call expression -- subclass of ExprAST */
class CallExprAST : public ExprAST
{
private:
	std::string Callee;
	std::vector<ExprAST*> Args;
public:
	CallExprAST(std::string &callee, std::vector<ExprAST*> &args)
		: Callee(callee), Args(args) {};
};


/* function prototype node */
class PrototypeAST 
{
private:
	std::string Name;
	std::vector<std::string> Args;
public:
	PrototypeAST(const std::string &name, const std::vector<std::string> &args)
		: Name(name), Args(args) {};
};


/* whole function definition node */
class FunctionAST
{
private:
	PrototypeAST* Proto;					// prototype
	ExprAST* Body;							// body
public:
	FunctionAST(PrototypeAST* proto, ExprAST* body)
		: Proto(proto), Body(body) {};
};


static int Curtok;							//current token
static int getNextToken()
{
	return Curtok = gettok();
}


/* error output */
ExprAST* Error(const char* Str)
{
	std::fprintf(stderr, "Error: %s\n", Str);
	return 0;
}

//prototype error
PrototypeAST* Errorp(const char* Str)
{
	Error(Str);
	return 0;
}


//funtcion error
FunctionAST* Errorf(const char* Str)
{
	Error(Str);
	return 0;
}


 
/* parsing number expression */
static ExprAST* ParseNumberExpr()
{
	ExprAST* result = new NumberExprAST(NumVal);
	getNextToken();						//update current token
	return result;
}

ExprAST* ParseExpression();

/* parsing paren expression */
static ExprAST* ParseParenExpr()
{
	getNextToken();			//eat (
	ExprAST* V =  ParseExpression();
	if (!V) {
		return 0;
	}

	if (Curtok != ')') {
		return Error(" expected ')' ");
	}	
	getNextToken();			//eat )
	return  V;
}


/* id -- single variable and function call */
static ExprAST* ParseIdentifierExpr()
{
	std::string IdName = IdentifierStr;

	getNextToken();		
	if (Curtok != '(') {				//variable expression
		return new VariableExprAST(IdName);
	}

	//function call
	getNextToken();						//ear '(' of args
	std::vector<ExprAST*> Args;
	if (Curtok != ')') {
		while (1) {
			ExprAST* Arg = ParseExpression();		//arg is expression
			if (!Arg) return 0;
			Args.push_back(Arg);

			if (Curtok == ')') break;

			if (Curtok != ',') {
				return Error("expected ')' or ',' in argument list");
			}

			getNextToken();

		}
	}
	
	getNextToken();					//eat ')'

	return new CallExprAST(IdName, Args);
}


//Primary Expression Entrance
static ExprAST* ParsePrimary() {
	switch (Curtok)
	{
	case tok_identifier:
		return ParseIdentifierExpr();
		break;
	case tok_number:
		return ParseNumberExpr();
		break;
	case '(':
		return ParseParenExpr();
		break;
	default:
		return Error("unknow token!");
		break;
	}
}

/* precedence map */
static std::map<char, int> BinopPrecedence;

/* get current token precedence */
static int getTokPrecedence()
{
	if (!isascii(Curtok)) {
		return -1;
	}

	int TokPrec = BinopPrecedence[Curtok];
	if (TokPrec <= 0) {
		return -1;
	}
	return TokPrec;
}


static ExprAST* ParseBinOpRHS(int ExprPrec, ExprAST* LHS)
{
	while (1) {
		int TokPrec = getTokPrecedence();

		if (TokPrec < ExprPrec) {			
			return LHS;
		}

		int BinOp = Curtok;
		getNextToken();						//eat binOp

		ExprAST* RHS = ParsePrimary();		//RHS
		if (!RHS) return 0;

		int NextPrec = getTokPrecedence();	//next binop
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, RHS);
			if (!RHS) return 0;
		}

		LHS = new BinaryExprAST(BinOp, LHS, RHS);
	}
}

static ExprAST* ParseExpression()
{
	ExprAST* LHS = ParsePrimary();			//left side
	if (!LHS) return 0;
	
	return ParseBinOpRHS(0, LHS);			
}


static PrototypeAST* ParsePrototype()
{
	if (Curtok != tok_identifier) {
		return Errorp("Expected function name in prototype");
	}

	std::string FnName = IdentifierStr;
	getNextToken();

	if (Curtok != '(') {
		return Errorp("Expected '(' in prototype");
	}

	std::vector<std::string> ArgNames;
	while (getNextToken() == tok_identifier) {
		ArgNames.push_back(IdentifierStr);
	}

	if (Curtok != ')') {
		return Errorp("Expected ')' in prototype");
	}

	getNextToken();
	
	return new PrototypeAST(FnName, ArgNames);
}


static FunctionAST* ParseDefinition()
{
	getNextToken();			//eat def 
	PrototypeAST* Proto = ParsePrototype();	
	if (!Proto) {
		return 0;
	}

	if (ExprAST* E = ParseExpression()) {
		return new FunctionAST(Proto, E);		//function (prototype , expression body)
	}
	return 0;
}

/* extern prototype */
static PrototypeAST* ParseExtern() 
{
	getNextToken();		//eat 'extern'
	return ParsePrototype();
}

/* Top level expression == uname function definition */
static FunctionAST* ParseTopLevelExpr()
{
	if (ExprAST* E = ParseExpression()) {
		PrototypeAST* proto = new PrototypeAST("", std::vector<std::string>());
		return new FunctionAST(proto, E);
	}

	return 0;
}


static void HandleDefinition()
{
	if (ParseDefinition()) {
		fprintf(stderr, "Parsed a function definition.\n");
	} else {
		getNextToken();		//skip error
	}
}

static void HandleExtern()
{
	if (ParseExtern()) {
		fprintf(stderr, "Parsed a extern.\n");
	} else {
		getNextToken();
	}
}

static void HandleTopLevelExpression()
{
	if (ParseTopLevelExpr()) {
		fprintf(stderr, "Parsed a top level expression.\n");
	} else {
		getNextToken();
	}
}


static void MainLoop()
{
	while (1) {
		if (Curtok == ';')
			fprintf(stderr, "Input<< ");
		else
			fprintf(stderr, "Output>> ");
		
		switch (Curtok)
		{
		case tok_eof:
			return;
		case ';':
			getNextToken();			//eat ;	
			break;
		case tok_def:
			HandleDefinition();
			break;
		case tok_extern:
			HandleExtern();
			break;
		default:
			HandleTopLevelExpression();
			break;
		}
	}
}


int main() {
	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;  // highest.

								// Prime the first token.
	fprintf(stderr, "Input<< ");
	getNextToken();

	// Run the main "interpreter loop" now.
	MainLoop();

	return 0;
}






















































































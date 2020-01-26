/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "precompiled.h"
#include "../../Include/RmlUi/Core/DataView.h"
#include "../../Include/RmlUi/Core/DataModel.h"
#include <stack>

namespace Rml {
namespace Core {


namespace Parser {

#ifdef _MSC_VER
#pragma warning(default : 4061)
#pragma warning(default : 4062)
#endif

enum class Type {
	Expression,
	Factor,
	Term,
	StringLiteral,
	NumberLiteral,
	Variable,
	Add,
	Subtract,
	Multiply,
	Divide,
	Not,
	And,
	Or,
	Equal,
	NotEqual,
	Less,
	Greater,
	Ternary,
	Function,
};

/*
	The abstract machine for RmlUi data scripts.

	The machine can execute a program which contains a list of instructions listed below.

	The abstract machine has three registers:
		R  Typically results and right-hand side arguments.
		L  Typically left-hand side arguments.
		C  Typically center arguments (eg. in ternary operator).

	And two stacks:
		S  The main program stack.
		A  The arguments stack, only used to pass arguments to an external transform function.

	In addition, each instruction has an optional payload:
		D  Instruction data (payload).

	Notation used in the instruction list below:
		S+  Push to stack S.
		S-  Pop stack S (returns the popped value).
*/
enum class Instruction { // Assignment (register/stack) = Read (register R/L/C, instruction data D, or stack)
	Push      = 'P',     //      S+ = R
	Pop       = 'o',     // <R/L/C> = S-  (D determines R/L/C)
	Literal   = 'D',     //       R = D
	Variable  = 'V',     //       R = DataModel.GetVariable(D)
	Add       = '+',     //       R = L + R
	Subtract  = '-',     //       R = L - R
	Multiply  = '*',     //       R = L * R
	Divide    = '/',     //       R = L / R
	Not       = '!',     //       R = !R
	And       = '&',     //       R = L && R
	Or        = '|',     //       R = L || R
	Equal     = '=',     //       R = L == R
	NotEqual  = 'N',     //       R = L != R
	Less      = '<',     //       R = L < R
	LessEq    = 'L',     //       R = L <= R
	Greater   = '>',     //       R = L > R
	GreaterEq = 'G',     //       R = L >= R
	Ternary   = '?',     //       R = L ? C : R
	Arguments = 'a',     //      A+ = S-  (Repeated D times, where D gives the num. arguments)
	Function  = 'F',     //       R = DataModel.Execute( D, R, A ); A.Clear();  (D determines function name, R the input value, A the arguments)
};
enum class Register {
	R,
	L,
	C
};

struct InstructionData {
	Instruction instruction;
	Variant data;
};
using Program = std::vector<InstructionData>;


class ParserContext {
public:
	ParserContext(String expression) : expression(std::move(expression)) {}

	char Look() const {
		if (index >= expression.size())
			return '\0';
		return expression[index];
	}

	bool Match(char c, bool skip_whitespace = true) {
		if (c == Look()) {
			Next();
			if (skip_whitespace)
				SkipWhitespace();
			return true;
		}
		Expected(c);
		return false;
	}

	char Next() {
		++index;
		return Look();
	}

	void SkipWhitespace() {
		char c = Look();
		while (StringUtilities::IsWhitespace(c))
			c = Next();
	}

	void Enter(Type type);

	void Error(String message)
	{
		parse_error = true;
		message = CreateString(message.size() + expression.size() + 50, "Error in expression '%s' at %d. %s", expression.c_str(), index, message.c_str());
		Log::Message(Log::LT_WARNING, message.c_str());
		
		const size_t cursor_offset = size_t(index) + sizeof("Error in expression ");
		const String cursor_string = String(cursor_offset, ' ') + '^';
		Log::Message(Log::LT_WARNING, cursor_string.c_str());
	}
	void Expected(char expected) {
		char c = Look();
		if (c == '\0')
			Error(CreateString(50, "Expected '%c' but found end of string.", expected));
		else
			Error(CreateString(50, "Expected '%c' but found '%c'.", expected, c));
	}
	void Expected(String expected_symbols) {
		Error(CreateString(expected_symbols.size() + 50, "Expected %s but found character '%c'.", expected_symbols.c_str(), Look()));
	}

	ParserContext& Parse() 
	{
		Log::Message(Log::LT_DEBUG, "Parsing expression: %s", expression.c_str());
		index = 0;
		parse_depth = 0;
		reached_end = false;
		parse_error = false;
		
		Enter(Type::Expression);
		
		if (!reached_end) {
			parse_error = true;
			Error(CreateString(50, "Unexpected character '%c' encountered.", Look()));
		}
		if (!parse_error)
			Log::Message(Log::LT_DEBUG, "Finished parsing expression! Instructions: %d   Parse depth: %d   Stack depth: %d", program.size(), parse_depth, program_stack_size);

		return *this;
	}

	String Execute();

	void ReachedEnd() {
		reached_end = true;
	}

	void Emit(Instruction instruction, Variant data = Variant())
	{
		RMLUI_ASSERTMSG(instruction != Instruction::Push && instruction != Instruction::Pop && instruction != Instruction::Arguments, 
			"Use the Push(), Pop(), or Arguments() procedures for stack manipulating instructions.");
		program.push_back(InstructionData{ instruction, std::move(data) });
	}
	void Push() {
		program_stack_size += 1;
		program.push_back(InstructionData{ Instruction::Push, Variant() });
	}
	void Pop(Register destination) {
		if (program_stack_size <= 0) {
			Error("Internal parser error: Tried to pop an empty stack.");
			return;
		}
		program_stack_size -= 1;
		program.push_back(InstructionData{ Instruction::Pop, Variant(int(destination)) });
	}
	void Arguments(int num_arguments) {
		if (program_stack_size < num_arguments) {
			Error(CreateString(128, "Internal parser error: Popping %d arguments, but the stack contains only %d elements.", num_arguments, program_stack_size));
			return;
		}
		program_stack_size -= num_arguments;
		program.push_back(InstructionData{ Instruction::Arguments, Variant(int(num_arguments)) });
	}

private:
	const String expression;

	size_t index = 0;
	int parse_depth = 0;
	bool reached_end = false;
	bool parse_error = false;

	int program_stack_size = 0;

	Program program;
};


class ExecutionContext {
public:
	ExecutionContext(const Program& program) : program(program) {}

	bool Error(String message) const
	{
		message = "Error during execution. " + message;
		Log::Message(Log::LT_WARNING, message.c_str());
		RMLUI_ERROR;
		return false;
	}

	bool Run() 
	{
		Log::Message(Log::LT_DEBUG, "Executing program");
		DumpProgram();
		bool success = true;
		for (size_t i = 0; i < program.size(); i++)
		{
			if (!Execute(program[i].instruction, program[i].data))
			{
				success = false;
				break;
			}
		}

		if (success)
			Log::Message(Log::LT_DEBUG, "Succesfully finished execution of program with %d instructions.", program.size());
		else
			Log::Message(Log::LT_WARNING, "Failed executing program with %d instructions.", program.size());

		Log::Message(Log::LT_DEBUG, "R: %s", R.Get<String>().c_str());
		Log::Message(Log::LT_DEBUG, "L: %s", L.Get<String>().c_str());
		Log::Message(Log::LT_DEBUG, "C: %s", C.Get<String>().c_str());
		Log::Message(Log::LT_DEBUG, "Stack #: %d", stack.size());

		return success;
	}

	String Result() const {
		return R.Get<String>();
	}

	void DumpProgram()
	{
		int i = 0;
		for (auto& instruction : program)
		{
			Log::Message(Log::LT_DEBUG, "  %4d  '%c'  %s", i, char(instruction.instruction), instruction.data.Get<String>().c_str());
			i++;
		}
	}

private:
	Variant R, L, C;
	std::stack<Variant> stack;
	std::vector<Variant> arguments;

	const Program& program;

	bool Execute(Instruction instruction, const Variant& data)
	{
		auto AnyString = [](const Variant& v1, const Variant& v2) {
			return v1.GetType() == Variant::STRING || v2.GetType() == Variant::STRING;
		};

		switch (instruction)
		{
		case Instruction::Push:
		{
			stack.push(std::move(R));
			R.Clear();
		}
		break;
		case Instruction::Pop:
		{
			if (stack.empty())
				return Error("Cannot pop stack, it is empty.");

			Register reg = Register(data.Get<int>(-1));
			switch (reg) {
			case Register::R:  R = stack.top(); stack.pop(); break;
			case Register::L:  L = stack.top(); stack.pop(); break;
			case Register::C:  C = stack.top(); stack.pop(); break;
			default:
				return Error(CreateString(50, "Invalid register %d", int(reg)));
			}
		}
		break;
		case Instruction::Literal:
		{
			R = data;
		}
		break;
		case Instruction::Variable:
		{
			// TODO: fetch from data model (data is model address), write into R.
			R = data;
		}
		break;
		case Instruction::Add:
		{
			if (AnyString(L, R))
				R = Variant(L.Get<String>() + R.Get<String>());
			else
				R = Variant(L.Get<float>() + R.Get<float>());
		}
		break;
		case Instruction::Subtract: R = Variant(L.Get<float>() - R.Get<float>()); break;
		case Instruction::Multiply: R = Variant(L.Get<float>() * R.Get<float>()); break;
		case Instruction::Divide:   R = Variant(L.Get<float>() / R.Get<float>()); break;
		case Instruction::Not:      R = Variant(!R.Get<bool>()); break;
		case Instruction::And:      R = Variant(L.Get<bool>() && R.Get<bool>());  break;
		case Instruction::Or:       R = Variant(L.Get<bool>() || R.Get<bool>());  break;
		case Instruction::Equal:
		{
			if(AnyString(L,R))
				R = Variant(L.Get<String>() == R.Get<String>());
			else
				R = Variant(L.Get<float>() == R.Get<float>());
		}
		break;
		case Instruction::NotEqual:
		{
			if (AnyString(L, R))
				R = Variant(L.Get<String>() != R.Get<String>());
			else
				R = Variant(L.Get<float>() != R.Get<float>());
		}
		break;
		case Instruction::Less:       R = Variant(L.Get<float>() < R.Get<float>());  break;
		case Instruction::LessEq:     R = Variant(L.Get<float>() <= R.Get<float>()); break;
		case Instruction::Greater:    R = Variant(L.Get<float>() > R.Get<float>());  break;
		case Instruction::GreaterEq:  R = Variant(L.Get<float>() >= R.Get<float>()); break;
		case Instruction::Arguments:
		{
			if (!arguments.empty())
				return Error("Invalid program: Argument stack is not empty.");

			int num_arguments = data.Get<int>(-1);
			if (num_arguments < 0)
				return Error("Invalid number of arguments.");
			if (stack.size() < size_t(num_arguments))
				return Error(CreateString(100, "Cannot pop %d arguments, stack contains only %d elements.", num_arguments, stack.size()));

			arguments.resize(num_arguments);
			for (int i = num_arguments - 1; i >= 0; i--)
			{
				arguments[i] = std::move(stack.top());
				stack.pop();
			}
		}
		break;
		case Instruction::Ternary:
		{
			if (L.Get<bool>())
				R = C;
		}
		break;
		case Instruction::Function:
		{
			const String function_name = data.Get<String>();

			String arguments_str;
			for (size_t i = 0; i < arguments.size(); i++)
			{
				arguments_str += arguments[i].Get<String>();
				if (i < arguments.size() - 1)
					arguments_str += ", ";
			}
			// TODO: execute function
			Log::Message(Log::LT_DEBUG, "Executing '%s' with %d argument(s): %s(%s)", function_name.c_str(), arguments.size(), function_name.c_str(), arguments_str.c_str());
			arguments.clear();
		}
		break;
		default:
			RMLUI_ERRORMSG("Instruction not yet implemented"); break;
		}
		return true;
	}
};



void StringLiteral(ParserContext& context)
{
	String str;

	char c = context.Look();
	bool previous_character_is_escape = false;

	while (c != '\0' && ( c != '\'' || previous_character_is_escape))
	{
		previous_character_is_escape = (c == '\\');
		str += c;
		c = context.Next();
	}

	context.Emit(Instruction::Literal, Variant(str));
}

void NumberLiteral(ParserContext& context)
{
	String str;

	bool first_match = false;
	bool has_dot = false;
	char c = context.Look();
	if (c == '-') 
	{
		str += c;
		c = context.Next();
	}

	while ((c >= '0' && c <= '9' ) || (c == '.' && !has_dot))
	{
		first_match = true;
		str += c;
		if (c == '.')
			has_dot = true;
		c = context.Next();
	}

	if (!first_match)
	{
		context.Error(CreateString(100, "Invalid number literal. Expected '0-9' or '.' but found '%c'.", c));
		return;
	}

	float number = FromString(str, 0.0f);

	context.Emit(Instruction::Literal, Variant(number));
}

bool IsVariableCharacter(char c, bool is_first_character)
{
	const bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');

	if (is_first_character)
		return is_alpha;

	if (is_alpha || (c >= '0' && c <= '9'))
		return true;

	for (char valid_char : "_.[] ")
	{
		if (c == valid_char && valid_char != '\0')
			return true;
	}

	return false;
}

String VariableName(ParserContext& context)
{
	String name;

	bool is_first_character = true;
	char c = context.Look();

	while (IsVariableCharacter(c, is_first_character))
	{
		name += c;
		c = context.Next();
		is_first_character = false;
	}

	// Right trim spaces in name
	size_t new_size = String::npos;
	for (int i = int(name.size()) - 1; i >= 1; i--)
	{
		if (name[i] == ' ')
			new_size = size_t(i);
		else
			break;
	}
	if (new_size != String::npos)
		name.resize(new_size);

	return name;
}

void DoVariable(ParserContext& context)
{
	String name = VariableName(context);
	if (name.empty()) {
		context.Error("Expected a variable but got an empty name.");
		return;
	}

	// Keywords are parsed like variables, but are really literals.
	// Check for them here.
	if(name == "true")
		context.Emit(Instruction::Literal, Variant(true));
	else if(name == "false")
		context.Emit(Instruction::Literal, Variant(false));
	else
		context.Emit(Instruction::Variable, Variant(name));
}

void Add(ParserContext& context)
{
	context.Match('+');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::Add);
}
void Subtract(ParserContext& context)
{
	context.Match('-');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::Subtract);
}
void Multiply(ParserContext& context)
{
	context.Match('*');
	context.Push();
	context.Enter(Type::Factor);
	context.Pop(Register::L);
	context.Emit(Instruction::Multiply);
}
void Divide(ParserContext& context)
{
	context.Match('/');
	context.Push();
	context.Enter(Type::Factor);
	context.Pop(Register::L);
	context.Emit(Instruction::Divide);
}
void Not(ParserContext& context)
{
	context.Match('!');
	context.Enter(Type::Factor);
	context.Emit(Instruction::Not);
}
void Or(ParserContext& context)
{
	// We already skipped the first '|' during expression
	context.Match('|');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::Or);
}
void And(ParserContext& context)
{
	context.Match('&', false);
	context.Match('&');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::And);
}
void Equal(ParserContext& context)
{
	context.Match('=', false);
	context.Match('=');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::Equal);
}
void NotEqual(ParserContext& context)
{
	context.Match('!', false);
	context.Match('=');
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(Instruction::NotEqual);
}
void Less(ParserContext& context)
{
	Instruction instruction = Instruction::Less;
	context.Match('<',false);
	if (context.Look() == '=') {
		context.Match('=');
		instruction = Instruction::LessEq;
	}
	else {
		context.SkipWhitespace();
	}
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(instruction);
}
void Greater(ParserContext& context)
{
	Instruction instruction = Instruction::Greater;
	context.Match('>', false);
	if (context.Look() == '=') {
		context.Match('=');
		instruction = Instruction::GreaterEq;
	}
	else {
		context.SkipWhitespace();
	}
	context.Push();
	context.Enter(Type::Term);
	context.Pop(Register::L);
	context.Emit(instruction);
}
void Ternary(ParserContext& context)
{
	context.Match('?');
	context.Push();
	context.Enter(Type::Expression);
	context.Push();
	context.Match(':');
	context.Enter(Type::Expression);
	context.Pop(Register::C);
	context.Pop(Register::L);
	context.Emit(Instruction::Ternary);
}
void Function(ParserContext& context)
{
	// We already matched '|' during expression
	String name = VariableName(context);
	if (name.empty()) {
		context.Error("Expected a transform name but got an empty name.");
		return;
	}

	if (context.Look() == '(')
	{
		int num_arguments = 0;
		bool looping = true;

		context.Match('(');
		if (context.Look() == ')') {
			context.Match(')');
			looping = false;
		}
		else
			context.Push();

		while (looping)
		{
			num_arguments += 1;
			context.Enter(Type::Expression);
			context.Push();

			switch (context.Look()) {
			case ')': context.Match(')'); looping = false; break;
			case ',': context.Match(','); break;
			default:
				context.Expected("one of ')' or ','");
				looping = false;
			}
		}

		if (num_arguments > 0) {
			context.Arguments(num_arguments);
			context.Pop(Register::R);
		}
	}
	else {
		context.SkipWhitespace();
	}

	context.Emit(Instruction::Function, Variant(name));
}


void Factor(ParserContext& context)
{
	const char c = context.Look();

	if (c == '(')
	{
		context.Match('(');
		context.Enter(Type::Expression);
		context.Match(')');
	}
	else if (c == '\'')
	{
		context.Match('\'', false);
		context.Enter(Type::StringLiteral);
		context.Match('\'');
	}
	else if (c == '!')
	{
		context.Enter(Type::Not);
		context.SkipWhitespace();
	}
	else if (c == '-' || (c >= '0' && c <= '9'))
	{
		context.Enter(Type::NumberLiteral);
		context.SkipWhitespace();
	}
	else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
	{
		context.Enter(Type::Variable);
		context.SkipWhitespace();
	}
	else
		context.Expected("literal, variable name, parenthesis, or '!'");
}

void Term(ParserContext& context)
{
	context.Enter(Type::Factor);

	bool looping = true;
	while (looping)
	{
		switch (const char c = context.Look())
		{
		case '*': context.Enter(Type::Multiply); break;
		case '/': context.Enter(Type::Divide); break;
		default:
			looping = false;
		}
	}
}

void Expression(ParserContext& context)
{
	context.Enter(Type::Term);

	bool looping = true;
	while (looping)
	{
		switch (char c = context.Look())
		{
		case '+': context.Enter(Type::Add); break;
		case '-': context.Enter(Type::Subtract); break;
		case '?': context.Enter(Type::Ternary); break;
		case '|':
		{
			context.Match('|', false);
			if(context.Look() == '|')
				context.Enter(Type::Or);
			else
			{
				context.SkipWhitespace();
				context.Enter(Type::Function);
			}
		}
		break;
		case '&': context.Enter(Type::And); break;
		case '=': context.Enter(Type::Equal); break;
		case '!': context.Enter(Type::NotEqual); break;
		case '<': context.Enter(Type::Less); break;
		case '>': context.Enter(Type::Greater); break;
		case '\0': 
			context.ReachedEnd();
			looping = false;
			break;
		default:
			looping = false;
		}
	}
}



void ParserContext::Enter(Type type)
{
	parse_depth += 1;

	switch (type)
	{
	case Type::Expression:    Expression(*this); break;
	case Type::Factor:        Factor(*this); break;
	case Type::Term:          Term(*this); break;
	case Type::StringLiteral: StringLiteral(*this); break;
	case Type::NumberLiteral: NumberLiteral(*this); break;
	case Type::Variable:      DoVariable(*this); break;
	case Type::Add:           Parser::Add(*this); break;
	case Type::Subtract:      Subtract(*this); break;
	case Type::Multiply:      Multiply(*this); break;
	case Type::Divide:        Divide(*this); break;
	case Type::Not:           Not(*this); break;
	case Type::And:           And(*this); break;
	case Type::Or:            Or(*this); break;
	case Type::Equal:         Equal(*this); break;
	case Type::NotEqual:      NotEqual(*this); break;
	case Type::Less:          Less(*this); break;
	case Type::Greater:       Greater(*this); break;
	case Type::Ternary:       Ternary(*this); break;
	case Type::Function:      Function(*this); break;
	default:
		RMLUI_ERRORMSG("Unhandled parser type"); break;
	}

	parse_depth -= 1;
}

String ParserContext::Execute() {
	if (parse_error) {
		Log::Message(Log::LT_ERROR, "Can not execute program, parsing was not succesful.");
		return String();
	}
	ExecutionContext execution(program);
	if (execution.Run())
		return execution.Result();
	return String();
}



struct TestParser {
	TestParser() {
		//ParserContext("'hello' + ' ' + 'world'").Parse();
		//ParserContext("5+(1+2)").Parse();
		//ParserContext("5.2 + 19 + 'test'").Parse();
		//ParserContext("(color_name) + (': rgba(' + color_value + ')')").Parse();
		//ParserContext("!!10 - 1 ? 'hello' : 'world'").Parse();
		//int test = 1 + (true ? 0-5 : 10 + 5);
		//ParserContext("1 + (true ? 0-5 : 10 + 5)").Parse();
		String result = ParserContext("'hello world' | uppercase(5 + 12 == 17 ? 'yes' : 'no', 9*2)").Parse().Execute();

		bool finished = true;
	}
};




} // </namespace Parser>









DataView::~DataView() {}

Element* DataView::GetElement() const
{
	// TODO Remove, testing only
	static Parser::TestParser test_parser;

	Element* result = attached_element.get();
	if (!result)
		Log::Message(Log::LT_WARNING, "Could not retrieve element in view, was it destroyed?");
	return result;
}

DataView::DataView(Element* element) : attached_element(element->GetObserverPtr()), element_depth(0) {
	if (element)
	{
		for (Element* parent = element->GetParentNode(); parent; parent = parent->GetParentNode())
			element_depth += 1;
	}
}


DataViewText::DataViewText(DataModel& model, ElementText* in_parent_element, const String& in_text, const size_t index_begin_search) : DataView(in_parent_element)
{
	text.reserve(in_text.size());

	bool success = true;

	size_t previous_close_brackets = 0;
	size_t begin_brackets = index_begin_search;
	while ((begin_brackets = in_text.find("{{", begin_brackets)) != String::npos)
	{
		text.insert(text.end(), in_text.begin() + previous_close_brackets, in_text.begin() + begin_brackets);

		const size_t begin_name = begin_brackets + 2;
		const size_t end_name = in_text.find("}}", begin_name);

		if (end_name == String::npos)
		{
			success = false;
			break;
		}

		DataEntry entry;
		entry.index = text.size();
		String address_str = StringUtilities::StripWhitespace(StringView(in_text.data() + begin_name, in_text.data() + end_name));
		entry.variable_address = model.ResolveAddress(address_str, in_parent_element);

		data_entries.push_back(std::move(entry));

		previous_close_brackets = end_name + 2;
		begin_brackets = previous_close_brackets;
	}

	if (data_entries.empty())
		success = false;

	if (success && previous_close_brackets < in_text.size())
		text.insert(text.end(), in_text.begin() + previous_close_brackets, in_text.end());

	if (!success)
	{
		text.clear();
		data_entries.clear();
		InvalidateView();
	}
}

bool DataViewText::Update(DataModel& model)
{
	bool entries_modified = false;

	for (DataEntry& entry : data_entries)
	{
		String value;
		bool result = model.GetValue(entry.variable_address, value);
		if (result && entry.value != value)
		{
			entry.value = value;
			entries_modified = true;
		}
	}

	if (entries_modified)
	{
		if (Element* element = GetElement())
		{
			RMLUI_ASSERTMSG(rmlui_dynamic_cast<ElementText*>(element), "Somehow the element type was changed from ElementText since construction of the view. Should not be possible?");

			if(auto text_element = static_cast<ElementText*>(element))
			{
				String new_text = BuildText();
				text_element->SetText(new_text);
			}
		}
		else
		{
			Log::Message(Log::LT_WARNING, "Could not update data view text, element no longer valid. Was it destroyed?");
		}
	}

	return entries_modified;
}

String DataViewText::BuildText() const
{
	size_t reserve_size = text.size();

	for (const DataEntry& entry : data_entries)
		reserve_size += entry.value.size();

	String result;
	result.reserve(reserve_size);

	size_t previous_index = 0;
	for (const DataEntry& entry : data_entries)
	{
		result += text.substr(previous_index, entry.index - previous_index);
		result += entry.value;
		previous_index = entry.index;
	}

	if (previous_index < text.size())
		result += text.substr(previous_index);

	return result;
}


DataViewAttribute::DataViewAttribute(DataModel& model, Element* element, const String& binding_name, const String& attribute_name)
	: DataView(element), attribute_name(attribute_name)
{
	variable_address = model.ResolveAddress(binding_name, element);

	if (attribute_name.empty())
		InvalidateView();
}

bool DataViewAttribute::Update(DataModel& model)
{
	bool result = false;
	String value;
	Element* element = GetElement();

	if (model.GetValue(variable_address, value) && element)
	{
		Variant* attribute = element->GetAttribute(attribute_name);

		if (!attribute || (attribute && attribute->Get<String>() != value))
		{
			element->SetAttribute(attribute_name, value);
			result = true;
		}
	}
	return result;
}



DataViewStyle::DataViewStyle(DataModel& model, Element* element, const String& binding_name, const String& property_name)
	: DataView(element), property_name(property_name)
{
	variable_address = model.ResolveAddress(binding_name, element);
	
	if (variable_address.empty() || property_name.empty())
		InvalidateView();
}


bool DataViewStyle::Update(DataModel& model)
{
	bool result = false;
	String value;
	Element* element = GetElement();

	if (model.GetValue(variable_address, value) && element)
	{
		const Property* p = element->GetLocalProperty(property_name);
		if (!p || p->Get<String>() != value)
		{
			element->SetProperty(property_name, value);
			result = true;
		}
	}
	return result;
}




DataViewIf::DataViewIf(DataModel& model, Element* element, const String& binding_name) : DataView(element)
{
	variable_address = model.ResolveAddress(binding_name, element);
	if (variable_address.empty())
		InvalidateView();
}


bool DataViewIf::Update(DataModel& model)
{
	bool result = false;
	bool value = false;
	Element* element = GetElement();

	if (model.GetValue(variable_address, value) && element)
	{
		bool is_visible = (element->GetLocalStyleProperties().count(PropertyId::Display) == 0);
		if(is_visible != value)
		{
			if (value)
				element->RemoveProperty(PropertyId::Display);
			else
				element->SetProperty(PropertyId::Display, Property(Style::Display::None));
			result = true;
		}
	}
	return result;
}



DataViewFor::DataViewFor(DataModel& model, Element* element, const String& in_binding_name, const String& in_rml_content)
	: DataView(element), rml_contents(in_rml_content)
{
	StringList binding_list;
	StringUtilities::ExpandString(binding_list, in_binding_name, ':');

	if (binding_list.empty() || binding_list.size() > 2 || binding_list.front().empty() || binding_list.back().empty())
	{
		Log::Message(Log::LT_WARNING, "Invalid syntax in data-for '%s'", in_binding_name.c_str());
		InvalidateView();
		return;
	}

	if (binding_list.size() == 2)
		alias_name = binding_list.front();
	else
		alias_name = "it";

	const String& binding_name = binding_list.back();

	variable_address = model.ResolveAddress(binding_name, element);
	if (variable_address.empty())
	{
		InvalidateView();
		return;
	}

	attributes = element->GetAttributes();
	attributes.erase("data-for");
	element->SetProperty(PropertyId::Display, Property(Style::Display::None));
}



bool DataViewFor::Update(DataModel& model)
{
	Variable variable = model.GetVariable(variable_address);
	if (!variable)
		return false;

	bool result = false;
	const int size = variable.Size();
	const int num_elements = (int)elements.size();
	Element* element = GetElement();

	for (int i = 0; i < Math::Max(size, num_elements); i++)
	{
		if (i >= num_elements)
		{
			ElementPtr new_element_ptr = Factory::InstanceElement(nullptr, element->GetTagName(), element->GetTagName(), attributes);

			Address replacement_address;
			replacement_address.reserve(variable_address.size() + 1);
			replacement_address = variable_address;
			replacement_address.push_back(AddressEntry(i));

			model.InsertAlias(new_element_ptr.get(), alias_name, replacement_address);

			Element* new_element = element->GetParentNode()->InsertBefore(std::move(new_element_ptr), element);
			elements.push_back(new_element);

			elements[i]->SetInnerRML(rml_contents);

			RMLUI_ASSERT(i < (int)elements.size());
		}
		if (i >= size)
		{
			model.EraseAliases(elements[i]);
			elements[i]->GetParentNode()->RemoveChild(elements[i]).reset();
			elements[i] = nullptr;
		}
	}

	if (num_elements > size)
		elements.resize(size);

	return result;
}

DataViews::DataViews()
{}

DataViews::~DataViews()
{}

void DataViews::Add(UniquePtr<DataView> view) {
	views_to_add.push_back(std::move(view));
}

void DataViews::OnElementRemove(Element* element) 
{
	for (auto it = views.begin(); it != views.end();)
	{
		auto& view = *it;
		if (view && view->GetElement() == element)
		{
			views_to_remove.push_back(std::move(view));
			it = views.erase(it);
		}
		else
			++it;
	}
}

bool DataViews::Update(DataModel & model, const SmallUnorderedSet< String >& dirty_variables)
{
	bool result = false;

	// View updates may result in newly added views, thus we do it recursively but with an upper limit.
	//   Without the loop, newly added views won't be updated until the next Update() call.
	for(int i = 0; i == 0 || (!views_to_add.empty() && i < 10); i++)
	{
		std::vector<DataView*> dirty_views;

		if (!views_to_add.empty())
		{
			views.reserve(views.size() + views_to_add.size());
			for (auto&& view : views_to_add)
			{
				dirty_views.push_back(view.get());
				for (const String& variable_name : view->GetVariableNameList())
					name_view_map.emplace(variable_name, view.get());

				views.push_back(std::move(view));
			}
			views_to_add.clear();
		}

		for (const String& variable_name : dirty_variables)
		{
			auto pair = name_view_map.equal_range(variable_name);
			for (auto it = pair.first; it != pair.second; ++it)
				dirty_views.push_back(it->second);
		}

		// Remove duplicate entries
		std::sort(dirty_views.begin(), dirty_views.end());
		auto it_remove = std::unique(dirty_views.begin(), dirty_views.end());
		dirty_views.erase(it_remove, dirty_views.end());

		// Sort by the element's depth in the document tree so that any structural changes due to a changed variable are reflected in the element's children.
		// Eg. the 'data-for' view will remove children if any of its data variable array size is reduced.
		std::sort(dirty_views.begin(), dirty_views.end(), [](auto&& left, auto&& right) { return left->GetElementDepth() < right->GetElementDepth(); });

		for (DataView* view : dirty_views)
		{
			RMLUI_ASSERT(view);
			if (!view)
				continue;

			if (view->IsValid())
				result |= view->Update(model);
		}

		// Destroy views marked for destruction
		// @performance: Horrible...
		if (!views_to_remove.empty())
		{
			for (const auto& view : views_to_remove)
			{
				for (auto it = name_view_map.begin(); it != name_view_map.end(); )
				{
					if (it->second == view.get())
						it = name_view_map.erase(it);
					else
						++it;
				}
			}

			views_to_remove.clear();
		}
	}

	return result;
}

}
}

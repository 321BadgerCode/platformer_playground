#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <variant>
#include <stdexcept>

using namespace std;

enum TokenType {
	LEFT_BRACE,	// {
	RIGHT_BRACE,	// }
	LEFT_BRACKET,	// [
	RIGHT_BRACKET,	// ]
	COMMA,		// ,
	COLON,		// :
	STRING,		// "abc"
	NUMBER,		// 123
	BOOLEAN,	// true or false
	NUL,		// null
	END_OF_FILE	// end of file
};

struct Token {
	TokenType type;
	string value;
};

class Lexer {
public:
	Lexer(const string& input) : input(input), position(0) {}

	Token nextToken() {
		skipWhitespace();
		if (position >= input.size()) return {END_OF_FILE, ""};

		char currentChar = input[position];
		if (currentChar == '{') return advanceToken(LEFT_BRACE, "{");
		if (currentChar == '}') return advanceToken(RIGHT_BRACE, "}");
		if (currentChar == '[') return advanceToken(LEFT_BRACKET, "[");
		if (currentChar == ']') return advanceToken(RIGHT_BRACKET, "]");
		if (currentChar == ',') return advanceToken(COMMA, ",");
		if (currentChar == ':') return advanceToken(COLON, ":");

		if (currentChar == '"') return stringToken();
		if (isdigit(currentChar) || currentChar == '-') return numberToken();
		if (isalpha(currentChar)) return keywordToken();

		throw runtime_error("Unexpected character in JSON input");
	}

private:
	string input;
	size_t position;

	Token advanceToken(TokenType type, const string& value) {
		position++;
		return {type, value};
	}

	void skipWhitespace() {
		while (position < input.size() && isspace(input[position])) {
			position++;
		}
	}

	Token stringToken() {
		position++;  // skip the opening quote
		size_t start = position;
		while (position < input.size() && input[position] != '"') {
			position++;
		}
		if (position >= input.size()) throw runtime_error("Unterminated string in JSON input");
		string value = input.substr(start, position - start);
		position++;  // skip the closing quote
		return {STRING, value};
	}

	Token numberToken() {
		size_t start = position;
		while (position < input.size() && (isdigit(input[position]) || input[position] == '.' || input[position] == '-')) {
			position++;
		}
		return {NUMBER, input.substr(start, position - start)};
	}

	Token keywordToken() {
		size_t start = position;
		while (position < input.size() && isalpha(input[position])) {
			position++;
		}
		string value = input.substr(start, position - start);
		if (value == "true" || value == "false") return {BOOLEAN, value};
		if (value == "null") return {NUL, value};
		throw runtime_error("Unexpected keyword in JSON input");
	}
};

class JsonNode {
public:
	using JsonObject = map<string, shared_ptr<JsonNode>>;
	using JsonArray = vector<shared_ptr<JsonNode>>;
	using JsonValue = variant<JsonObject, JsonArray, string, double, bool, nullptr_t>;

	JsonNode() = default;
	explicit JsonNode(JsonValue value) : value(move(value)) {}

	shared_ptr<JsonNode> operator[](const string& key) const {
		if (auto obj = get_if<JsonObject>(&value)) {
			auto it = obj->find(key);
			if (it != obj->end()) {
				return it->second;
			}
		}
		throw runtime_error("Key not found in JSON object");
	}

	shared_ptr<JsonNode> operator[](size_t index) const {
		if (auto arr = get_if<JsonArray>(&value)) {
			if (index < arr->size()) {
				return (*arr)[index];
			}
		}
		throw runtime_error("Index out of bounds in JSON array");
	}

	const JsonValue& getValue() const {
		return value;
	}

private:
	JsonValue value;
};

class Parser {
public:
	Parser(const string& input) : lexer(input) {}

	shared_ptr<JsonNode> parse() {
		currentToken = lexer.nextToken();
		return parseValue();
	}

private:
	Lexer lexer;
	Token currentToken;

	shared_ptr<JsonNode> parseValue() {
		switch (currentToken.type) {
			case STRING: return parseString();
			case NUMBER: return parseNumber();
			case BOOLEAN: return parseBoolean();
			case NUL: return parseNull();
			case LEFT_BRACE: return parseObject();
			case LEFT_BRACKET: return parseArray();
			default: throw runtime_error("Unexpected value in JSON input");
		}
	}

	shared_ptr<JsonNode> parseString() {
		if (currentToken.type != STRING) throw runtime_error("Expected a string");
		auto node = make_shared<JsonNode>(currentToken.value);
		currentToken = lexer.nextToken();
		return node;
	}

	shared_ptr<JsonNode> parseNumber() {
		if (currentToken.type != NUMBER) throw runtime_error("Expected a number");
		auto node = make_shared<JsonNode>(stod(currentToken.value));
		currentToken = lexer.nextToken();
		return node;
	}

	shared_ptr<JsonNode> parseBoolean() {
		if (currentToken.type != BOOLEAN) throw runtime_error("Expected a boolean");
		auto node = make_shared<JsonNode>(currentToken.value == "true");
		currentToken = lexer.nextToken();
		return node;
	}

	shared_ptr<JsonNode> parseNull() {
		if (currentToken.type != NUL) throw runtime_error("Expected null");
		auto node = make_shared<JsonNode>(nullptr);
		currentToken = lexer.nextToken();
		return node;
	}

	shared_ptr<JsonNode> parseObject() {
		map<string, shared_ptr<JsonNode>> jsonObject;
		consumeToken(LEFT_BRACE);
		while (currentToken.type != RIGHT_BRACE) {
		auto keyNode = parseString();
		if (keyNode->getValue().index() != 2) {
		throw runtime_error("Expected a string as key");
		}
		string key = get<string>(keyNode->getValue());
			consumeToken(COLON);
			auto value = parseValue();
			jsonObject[key] = value;
			if (currentToken.type == COMMA) consumeToken(COMMA);
		}
		consumeToken(RIGHT_BRACE);
		return make_shared<JsonNode>(jsonObject);
	}

	shared_ptr<JsonNode> parseArray() {
		vector<shared_ptr<JsonNode>> jsonArray;
		consumeToken(LEFT_BRACKET);
		while (currentToken.type != RIGHT_BRACKET) {
			jsonArray.push_back(parseValue());
			if (currentToken.type == COMMA) consumeToken(COMMA);
		}
		consumeToken(RIGHT_BRACKET);
		return make_shared<JsonNode>(jsonArray);
	}

	void consumeToken(TokenType expectedType) {
		if (currentToken.type != expectedType) {
			throw runtime_error("Unexpected token in JSON input");
		}
		currentToken = lexer.nextToken();
	}
};

void printJson(shared_ptr<JsonNode> node, int indent = 0) {
	const auto& value = node->getValue();
	if (auto obj = get_if<JsonNode::JsonObject>(&value)) {
		cout << string(indent, ' ') << "{\n";
		for (const auto& [key, val] : *obj) {
			cout << string(indent + 2, ' ') << "\"" << key << "\": ";
			printJson(val, indent + 2);
		}
		cout << string(indent, ' ') << "}\n";
	} else if (auto arr = get_if<JsonNode::JsonArray>(&value)) {
		cout << string(indent, ' ') << "[\n";
		for (const auto& val : *arr) {
			printJson(val, indent + 2);
		}
		cout << string(indent, ' ') << "]\n";
	} else if (auto str = get_if<string>(&value)) {
		cout << "\"" << *str << "\"\n";
	} else if (auto num = get_if<double>(&value)) {
		cout << *num << "\n";
	} else if (auto boolean = get_if<bool>(&value)) {
		cout << (*boolean ? "true" : "false") << "\n";
	} else if (holds_alternative<nullptr_t>(value)) {
		cout << "null\n";
	}
}
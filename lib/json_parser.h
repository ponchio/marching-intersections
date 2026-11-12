#ifndef MI_JSON_PARSER_H
#define MI_JSON_PARSER_H

#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mi {
namespace json {

struct Value {
	enum class Type { Null, Number, String, Array, Object, Boolean } type = Type::Null;
	double number = 0.0;
	std::string string;
	std::vector<Value> array;
	std::map<std::string, Value> object;
};

class Parser {
public:
	explicit Parser(const std::string &input): input(input) {}

	bool parse(Value &value) {
		if(!parseValue(value))
			return false;
		skipWhitespace();
		return position == input.size();
	}

private:
	const std::string &input;
	size_t position = 0;

	void skipWhitespace() {
		while(position < input.size() && (input[position] == ' ' || input[position] == '\t' ||
			input[position] == '\r' || input[position] == '\n'))
			++position;
	}

	bool parseString(std::string &result) {
		if(position >= input.size() || input[position++] != '"')
			return false;
		while(position < input.size()) {
			const char c = input[position++];
			if(c == '"')
				return true;
			if(static_cast<unsigned char>(c) < 0x20)
				return false;
			if(c != '\\') {
				result.push_back(c);
				continue;
			}
			if(position >= input.size())
				return false;
			switch(input[position++]) {
			case '"': result.push_back('"'); break;
			case '\\': result.push_back('\\'); break;
			case '/': result.push_back('/'); break;
			case 'b': result.push_back('\b'); break;
			case 'f': result.push_back('\f'); break;
			case 'n': result.push_back('\n'); break;
			case 'r': result.push_back('\r'); break;
			case 't': result.push_back('\t'); break;
			case 'u': {
				unsigned int code;
				if(!parseHexCodeUnit(code))
					return false;
				if(code >= 0xd800 && code <= 0xdbff) {
					if(input.size() - position < 6 || input[position] != '\\' ||
					   input[position + 1] != 'u')
						return false;
					position += 2;
					unsigned int low;
					if(!parseHexCodeUnit(low) || low < 0xdc00 || low > 0xdfff)
						return false;
					code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
				} else if(code >= 0xdc00 && code <= 0xdfff) {
					return false;
				}
				appendUtf8(result, code);
				break;
			}
			default: return false;
			}
		}
		return false;
	}

	bool parseHexCodeUnit(unsigned int &code) {
		if(input.size() - position < 4)
			return false;
		code = 0;
		for(int i = 0; i < 4; ++i) {
			const char c = input[position++];
			code <<= 4;
			if(c >= '0' && c <= '9') code |= static_cast<unsigned int>(c - '0');
			else if(c >= 'a' && c <= 'f') code |= static_cast<unsigned int>(c - 'a' + 10);
			else if(c >= 'A' && c <= 'F') code |= static_cast<unsigned int>(c - 'A' + 10);
			else return false;
		}
		return true;
	}

	static void appendUtf8(std::string &result, unsigned int code) {
		if(code < 0x80) {
			result.push_back(static_cast<char>(code));
		} else if(code < 0x800) {
			result.push_back(static_cast<char>(0xc0 | (code >> 6)));
			result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
		} else if(code < 0x10000) {
			result.push_back(static_cast<char>(0xe0 | (code >> 12)));
			result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
			result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
		} else {
			result.push_back(static_cast<char>(0xf0 | (code >> 18)));
			result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
			result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
			result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
		}
	}

	bool parseNumber(Value &value) {
		const size_t start = position;
		if(position < input.size() && input[position] == '-')
			++position;
		if(position >= input.size())
			return false;
		if(input[position] == '0') {
			++position;
		} else {
			if(input[position] < '1' || input[position] > '9')
				return false;
			while(position < input.size() && input[position] >= '0' && input[position] <= '9')
				++position;
		}
		if(position < input.size() && input[position] == '.') {
			++position;
			const size_t fraction = position;
			while(position < input.size() && input[position] >= '0' && input[position] <= '9')
				++position;
			if(position == fraction)
				return false;
		}
		if(position < input.size() && (input[position] == 'e' || input[position] == 'E')) {
			++position;
			if(position < input.size() && (input[position] == '+' || input[position] == '-'))
				++position;
			const size_t exponent = position;
			while(position < input.size() && input[position] >= '0' && input[position] <= '9')
				++position;
			if(position == exponent)
				return false;
		}
		value.number = std::strtod(input.substr(start, position - start).c_str(), nullptr);
		value.type = Value::Type::Number;
		return std::isfinite(value.number);
	}

	bool parseValue(Value &value) {
		skipWhitespace();
		if(position >= input.size())
			return false;
		const char c = input[position];
		if(c == '"') {
			value.type = Value::Type::String;
			return parseString(value.string);
		}
		if(c == '{') {
			++position;
			value.type = Value::Type::Object;
			skipWhitespace();
			if(position < input.size() && input[position] == '}') {
				++position;
				return true;
			}
			while(true) {
				skipWhitespace();
				std::string key;
				if(!parseString(key))
					return false;
				skipWhitespace();
				if(position >= input.size() || input[position++] != ':')
					return false;
				Value member;
				if(!parseValue(member) || !value.object.emplace(key, std::move(member)).second)
					return false;
				skipWhitespace();
				if(position >= input.size())
					return false;
				if(input[position] == '}') {
					++position;
					return true;
				}
				if(input[position++] != ',')
					return false;
			}
		}
		if(c == '[') {
			++position;
			value.type = Value::Type::Array;
			skipWhitespace();
			if(position < input.size() && input[position] == ']') {
				++position;
				return true;
			}
			while(true) {
				Value member;
				if(!parseValue(member))
					return false;
				value.array.push_back(std::move(member));
				skipWhitespace();
				if(position >= input.size())
					return false;
				if(input[position] == ']') {
					++position;
					return true;
				}
				if(input[position++] != ',')
					return false;
			}
		}
		if(c == '-' || (c >= '0' && c <= '9'))
			return parseNumber(value);
		if(input.compare(position, 4, "true") == 0) {
			position += 4;
			value.type = Value::Type::Boolean;
			return true;
		}
		if(input.compare(position, 5, "false") == 0) {
			position += 5;
			value.type = Value::Type::Boolean;
			return true;
		}
		if(input.compare(position, 4, "null") == 0) {
			position += 4;
			value.type = Value::Type::Null;
			return true;
		}
		return false;
	}
};

} // namespace json
} // namespace mi

#endif // MI_JSON_PARSER_H

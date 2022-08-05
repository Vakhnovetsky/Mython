#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input) :input_(input) {
        Load();
    }

    const Token& Lexer::CurrentToken() const {
        return token_;
    }

    Token Lexer::NextToken() {
        Load();
        return token_;
    }

    void Lexer::LoadNewline() {
        if (!new_line_) {
            new_line_ = true;
            offset_ = current_offset_;
            current_offset_ = 0;
            token_ = token_type::Newline();
        }
        else {
            Load();
        }
    }

    void Lexer::Load() {
        char c = ' ';
        if (!new_line_) {
            while (c == ' ') {
                c = input_.get();
            }
        }
        else {
            if (!CheckEmptyLine()) {
                c = input_.get();

                if (c != ' ') {
                    if (offset_ > current_offset_ ) {
                        input_.putback(c);
                        offset_ -= 2;
                        token_ = token_type::Dedent();
                        return;
                    }
                }
                else {
                    while (c == ' ') {
                        ++current_offset_;
                        if (current_offset_ > offset_ && current_offset_ % 2 == 0) {
                            token_ = token_type::Indent();
                            return;
                        }
                        else {
                            c = input_.get();
                        }
                    }

                    if (current_offset_ < offset_) {
                        input_.putback(c);
                        offset_ -= 2;
                        token_ = token_type::Dedent();
                        return;
                    }
                }
            }
            else {
                c = input_.get();
            }
        }

        if (c == EOF && token_ != token_type::Newline() && new_line_ == false) {
            input_.putback(c);
            LoadNewline();
            return;
        }

        switch (c) {
        case EOF: token_ = token_type::Eof();  break;
        case '\n': LoadNewline(); break;
        case '+':
            [[fallthrough]];
        case '-':
            [[fallthrough]];
        case '*':
            [[fallthrough]];
        case '/':
            [[fallthrough]];
        case '=':
            [[fallthrough]];
        case '>':
            [[fallthrough]];
        case '<':
            [[fallthrough]];
        case '.':
            [[fallthrough]];
        case ',':
            [[fallthrough]];
        case '(':
            [[fallthrough]];
        case ')':
            [[fallthrough]];
        case '!':
            [[fallthrough]];
        case ':':
            LoadChar(c); break;
        case '\'':
            [[fallthrough]];
        case '"':
            LoadString(c); break;
        case '#':
            LoadComment(); break;

        default:
            if (std::isalpha(c) || c == '_') {
                input_.putback(c);
                LoadId();
                break;
            }
            else if (std::isdigit(c)) {
                input_.putback(c);
                LoadNumber();
                break;
            }
        }
    }

    std::string Lexer::GetString() {
        std::string result;
        while (std::isalnum(input_.peek()) || input_.peek() == '_') {
            result.push_back(static_cast<char>(input_.get()));
        }
        return result;
    }

    void Lexer::LoadId() {
        new_line_ = false;
        std::string word = GetString();

        if (tokens.find(word) != tokens.end()) {
            token_ = tokens.at(word);
        }
        else {
            token_type::Id s;
            s.value = word;
            token_ = s;
        }
    }

    void Lexer::LoadNumber() {
        new_line_ = false;
        std::string result;
        while (std::isdigit(input_.peek())) {
            result.push_back(static_cast<char>(input_.get()));
        }
        token_type::Number n;
        n.value = atoi(result.c_str());
        token_ = n;
    }

    void Lexer::LoadChar(char c) {
        new_line_ = false;
        if (c == '!' && input_.peek() == '=') {
            token_ = token_type::NotEq();
            input_.get();
        }
        else if (c == '=' && input_.peek() == '=') {
            token_ = token_type::Eq();
            input_.get();
        }
        else if (c == '>' && input_.peek() == '=') {
            token_ = token_type::GreaterOrEq();
            input_.get();
        }
        else if (c == '<' && input_.peek() == '=') {
            token_ = token_type::LessOrEq();
            input_.get();
        }
        else {
            token_type::Char s;
            s.value = c;
            token_ = s;
        }
    }

    void Lexer::LoadString(char first) {
        char c;
        std::string result;

        new_line_ = false;

        while (input_.get(c)) {
            if (c == first) {
                break;
            }
            if (c == '\\' && (input_.peek() == '\"' || input_.peek() == '\'')) {
                result.push_back(input_.get());
            }
            else if (c == '\\' && input_.peek() == 'n') {
                input_.get();
                result.push_back('\n');
            }
            else if (c == '\\' && input_.peek() == 't') {
                input_.get();
                result.push_back('\t');
            }
            else {
                result.push_back(c);
            }
        }

        token_type::String s;
        s.value = result;
        token_ = s;
    }

    void Lexer::LoadComment() {
        while (input_.peek() != '\n' && input_.peek() != EOF) {
            input_.get();
        }
        Load();
    }

    bool Lexer::CheckEmptyLine() {
        char c = ' ';
        string temp;
        while (c == ' ') {
            c = input_.get();
            temp.push_back(c);
        }

        if (c == '\n' || c == '#') {
            input_.putback(c);
            return true;
        }
        else {
            for (auto iter = temp.rbegin(); iter != temp.rend(); ++iter) {
                input_.putback(*iter);
            }
            return false;
        }
    }
}  // namespace parse

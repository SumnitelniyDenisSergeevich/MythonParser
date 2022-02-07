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

    Lexer::Lexer(std::istream& input) : input_(input) {
        ScipToBegin();
        current_token_ = NextToken();
    }

    const Token& Lexer::CurrentToken() const {
        return current_token_;
    }

    Token Lexer::NextToken() {
        if (new_line_) {
            static size_t space_count = 0;
            CalcSpaceCount(space_count);
            size_t dent_count = space_count / 2;

            if (Token* tok = GetIndentOrDedentToken(dent_count); tok) {
                return *tok;
            }
            space_count = 0;
        }

        char peek = input_.get();

        while (peek == ' ') {
            peek = input_.get();
        }
        if (std::isdigit(peek)) {
            return current_token_ =  GetDigitToken(peek);
        }
        else if (std::isalpha(peek) || peek == '_') {
            return current_token_ = GetIdToken(peek);
        }
        else if (peek == '\"' || peek == '\'') {
            return current_token_ = GetStringToken(peek);
        }
        else if (peek == '#') {
            ScipComments();
            return current_token_ = NextToken();
        }
        else if (peek == '\n') {
            return current_token_ = GetNewLineToken();
        }
        else if (input_.eof()) {
            return current_token_ = GetEofToken();
        }
        else {
            return current_token_ = GetCharToken(peek);
        };
    }

    Token Lexer::GetDigitToken(char peek) {
        empty_line_ = false;
        int number = atoi(&peek);
        while (std::isdigit(input_.peek())) {
            peek = input_.get();
            number = number * 10 + atoi(&peek);
        }
        return token_type::Number{ number };;
    }

    Token Lexer::GetIdToken(char peek) {
        empty_line_ = false;
        std::string id;
        id.push_back(peek);
        while (std::isalpha(input_.peek()) || std::isdigit(input_.peek()) || input_.peek() == '_') {
            id.push_back(input_.get());
        }
        if (key_words_.find(id) != key_words_.cend()) {
            return key_words_.at(id);
        }
        else {
            return token_type::Id{ id };
        }
    }

    Token Lexer::GetStringToken(char peek) {
        empty_line_ = false;
        if (peek == '\"') {
            return GetStringTokenChoseQuote(peek, '\"');
        }
        else {
            return GetStringTokenChoseQuote(peek, '\'');
        }
    }

    Token Lexer::GetStringTokenChoseQuote(char peek, char quote) {
        const std::unordered_map<char, char> escape_sequences{ {'n','\n'}, {'t','\t'}, {'\'','\''}, {'\"','\"'} };
        string str;
        bool escaped = false;
        peek = input_.get();
        for (; !input_.eof() && !(!escaped && peek == quote); peek = input_.get()) {
            if (!escaped) {
                if (peek == '\\') {
                    escaped = true;
                }
                else {
                    str.push_back(peek);
                }
            }
            else {
                if (const auto it = escape_sequences.find(peek); it != escape_sequences.end()) {
                    str.push_back(it->second);
                }
                escaped = false;
            }
        }
        return token_type::String{ str };
    }

    Token Lexer::GetCharToken(char peek) {
        empty_line_ = false;
        string str;
        str.push_back(peek);
        str.push_back(input_.peek());
        if (key_words_.find(str) != key_words_.cend()) {
            input_.get();
            return key_words_.at(str);
        }
        else {
            return token_type::Char{ peek };
        }
    }

    Token Lexer::GetNewLineToken() {
        new_line_ = true;
        if (!empty_line_) {
            empty_line_ = true;
            return token_type::Newline{};
        }
        return NextToken();
    }

    Token Lexer::GetEofToken() {
        if (!empty_line_) {
            empty_line_ = true;
            return token_type::Newline{};
        }
        return token_type::Eof{};
    }

    Token* Lexer::GetIndentOrDedentToken(size_t dent_count) {
        if (dent_count > dent_count_) {
            size_t indent_count = dent_count - dent_count_;
            if (indent_count > 0) {
                ++dent_count_;
                current_token_ = token_type::Indent{};
                return &current_token_;
            }
        }
        else if (dent_count < dent_count_) {
            size_t dedent_count = dent_count_ - dent_count;
            if (dedent_count > 0) {
                --dent_count_;
                current_token_ = token_type::Dedent{};
                return &current_token_;
            }
        }
        new_line_ = false;
        return nullptr;
    }

    void Lexer::ScipToBegin() {
        while (input_.peek() == ' ' || input_.peek() == '\n' || input_.peek() == '#') {
            if (input_.peek() == '#') {
                while (input_.peek() != '\n' && !input_.eof()) {
                    input_.get();
                }
            }
            while (input_.peek() == ' ' || input_.peek() == '\n') {
                input_.get();
            }
        }
    }

    void Lexer::CalcSpaceCount(size_t& space_count) {
        bool new_line_flag;
        do {
            while (input_.peek() == ' ') {
                ++space_count;
                input_.get();
            }
            new_line_flag = false;
            if (input_.peek() == '\n') {
                input_.get();
                space_count = 0;
                new_line_flag = true;
            }
        } while (new_line_flag);
    }

    void Lexer::ScipComments() {
        while (input_.peek() != '\n' && !input_.eof()) {
            input_.get();
        }
    }

}  // namespace parse



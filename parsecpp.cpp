#include "parsecpp.h"

Source::Source(const char *s) : s(s), l(s), line(1), col(1) {}
char Source::peek() {
    char ch = *s;
    if (!ch) throw ex("too short");
    return ch;
}
void Source::next() {
    char ch = peek();
    ++s;
    ++col;
    if (ch == '\n') {
        l = s;
        ++line;
        col = 1;
    }
}
std::string Source::getLine() const {
    std::ostringstream oss;
    for (auto l = this->l; *l && *l != '\r' && *l != '\n'; ++l) {
        oss << *l;
    }
    return oss.str();
}
void Source::showPos(std::ostream &os) const {
    os << getLine() << std::endl;
    for (int i = 0; i < col - 1; ++i) os << (l[i] == '\t' ? '\t' : ' ');
    os << "^" << std::endl;
}
std::string Source::ex(const std::string &e) const {
    std::ostringstream oss;
    oss << "[line " << line << ", col " << col << "] " << e;
    if (*s) oss << ": '" << *s << "'";
    return oss.str();
}
std::string Source::ex2(const std::string &e) const {
    std::ostringstream oss;
    oss << "[line " << line << ", col " << col << "] " << e;
    if (*s) oss << ": '" << *s << "'" << std::endl;
    showPos(oss);
    return oss.str();
}
bool Source::operator==(const Source &src) const {
    return s == src.s;
}
bool Source::operator!=(const Source &src) const {
    return !(*this == src);
}
bool Source::eof() const {
    return !*s;
}

Parser<char> satisfy(const std::function<bool (char)> &f) {
    return [=](Source *s) {
        char ch = s->peek();
        if (!f(ch)) throw s->ex("not satisfy");
        s->next();
        return ch;
    };
}

Parser<char> anyChar = satisfy([](char) { return true; });

Parser<std::string> many(const Parser<char> &p) {
    return many_(p);
}
Parser<std::string> many(const Parser<std::string> &p) {
    return many_(p);
}

Parser<std::string> right(const char *s) {
    return right<std::string>(s);
}

Parser<char> left(const std::string &e) {
    return left<char>(e);
}

Parser<char> char1(char ch) {
    return satisfy([=](char c) { return c == ch; })
        || left(std::string("not char '") + ch + "'");
}

Parser<std::string> string(const std::string &str) {
    return [=](Source *s) {
        for (auto it = str.begin(); it != str.end(); ++it) {
            (char1(*it) || left("not string \"" + str + "\""))(s);
        }
        return str;
    };
}

bool isDigit   (char ch) { return '0' <= ch && ch <= '9'; }
bool isUpper   (char ch) { return 'A' <= ch && ch <= 'Z'; }
bool isLower   (char ch) { return 'a' <= ch && ch <= 'z'; }
bool isAlpha   (char ch) { return isUpper(ch) || isLower(ch); }
bool isAlphaNum(char ch) { return isAlpha(ch) || isDigit(ch); }
bool isLetter  (char ch) { return isAlpha(ch) || ch == '_';   }
bool isSpace   (char ch) { return ch == '\t'  || ch == ' ';   }

Parser<char> digit    = satisfy(isDigit   ) || left("not digit"   );
Parser<char> upper    = satisfy(isUpper   ) || left("not upper"   );
Parser<char> lower    = satisfy(isLower   ) || left("not lower"   );
Parser<char> alpha    = satisfy(isAlpha   ) || left("not alpha"   );
Parser<char> alphaNum = satisfy(isAlphaNum) || left("not alphaNum");
Parser<char> letter   = satisfy(isLetter  ) || left("not letter"  );
Parser<char> space    = satisfy(isSpace   ) || left("not space"   );

Parser<std::string> spaces = many(space);

#include <stdio.h>
#include <vector>
#include <utility>
#include "parsecpp.h"

Parser<std::string> comment = [](Source *s) {
    std::string ret;
    ret += tryp(string("/*"))(s);
    for (;;) {
        char ch1 = anyChar(s);
        ret += ch1;
        if (ch1 == '*') {
            char ch2 = anyChar(s);
            ret += ch2;
            if (ch2 == '/') break;
        }
    }
    return ret;
};
auto spc = char1(' ') || char1('\t') || char1('\n');
auto spcs = many(many1(spc) || comment);
template <typename T>
Parser<T> read(const Parser<T> &p) {
    return spcs >> p << spcs;
}

Parser<char> chr(char ch) { return read(char1(ch)); }
template <typename T>
Parser<std::string> operator+(char ch, const Parser<T> &p) {
    return chr(ch) + p;
}
template <typename T>
Parser<std::string> operator+(const Parser<T> &p, char ch) {
    return p + chr(ch);
}
Parser<std::string> many(char ch) { return many(chr(ch)); }

Parser<std::string> str(const std::string &s) { return read(tryp(string(s))); }
Parser<std::string> strOf(const std::list<std::string> &list) {
    return read(stringOf(list));
}
Parser<std::string> opt(const Parser<std::string> &p) {
    return tryp(p) || right("");
}
Parser<std::string> opt(const Parser<char> &p) {
    return tryp(p + right("")) || right("");
}
Parser<std::string> nochar(char ch) {
    return [=](Source *s) -> std::string {
        if (s->peek() != ch) return "";
        throw s->ex("not nochar '" + std::string(1, ch) + "'");
    };
}

auto sym = letter + many(letter || digit);
auto num = many1(digit);
Parser<std::string> lstr = [](Source *s) {
    std::string ret;
    char1('"')(s);
    for (;;) {
        char ch = anyChar(s);
        if (ch == '"') break;
        ret += ch;
        if (ch == '\\') ret += anyChar(s);
    }
    return "\"" + ret + "\"";
};
auto lchar = char1('\'') + opt(char1('\\')) + anyChar + char1('\'');

auto ops = 
       char1('>') + opt(oneOf(">="))
    || char1('<') + opt(oneOf("<="))
    || char1('=') + opt(stringOf({">>", "<<", "^", "=", "+", "-", "*", "/", "&", "|", "%"}))
    || stringOf({"++", "--", "&&", "||", "!="}) || anyChar + right("");
enum TokenType { Num, Sym, Str, Char, Op };
using Token = std::pair<TokenType, std::string>;
std::ostream &operator<<(std::ostream &cout, const Token &t) {
    cout << "(" << t.first << "," << t.second << ")";
}
Parser<Token> token = [](Source *s) {
    spcs(s);
    try { return std::make_pair(Num , num  (s)); } catch (std::string &) {}
    try { return std::make_pair(Sym , sym  (s)); } catch (std::string &) {}
    try { return std::make_pair(Str , lstr (s)); } catch (std::string &) {}
    try { return std::make_pair(Char, lchar(s)); } catch (std::string &) {}
    return std::make_pair(Op, ops(s));
};

Parser<std::string> word(const std::string &w) {
    return [=](Source *s) {
        auto bak = *s;
        auto sy = read(sym)(s);
        if (w != sy) {
            *s = bak;
            throw "not word \"" + w + "\"";
        }
        return w;
    };
}

auto type = word("int") || word("char") || word("struct") + right(" ") + read(sym);

Parser<std::string> xpr(int n);
Parser<std::string> expr = read(xpr(0));

Parser<std::string> xprs[] = {
    /* 0*/ xpr(1) + many(',' + xpr(1)),
    /* 1*/ xpr(2) + opt('=' + opt(strOf({"+", "-", "*", "/", "%", "<<", ">>", "&", "^", "|"})) + expr),
    /* 2*/ xpr(3) + opt('?' + expr + ':' + expr),
    /* 3*/ xpr(4) + many(str("||") + xpr(4)),
    /* 4*/ xpr(5) + many(str("&&") + xpr(5)),
    /* 5*/ xpr(6) + many(tryp(char1('|') + nochar('|')) + read(xpr(6))),
    /* 6*/ xpr(7) + many('^' + xpr(7)),
    /* 7*/ xpr(8) + many(tryp(char1('&') + nochar('&')) + read(xpr(8))),
    /* 8*/ xpr(9) + many(strOf({"==", "!="}) + xpr(9)),
    /* 9*/ xpr(10) + many(strOf({"<=", ">=", "<", ">"}) + read(xpr(10))),
    /*10*/ xpr(11) + many(strOf({"<<", ">>"}) + xpr(11)),
    /*11*/ xpr(12) + many(strOf({"+", "-"}) + read(xpr(12))),
    /*12*/ xpr(13) + many(strOf({"*", "/", "%"}) + xpr(13)),
    /*13*/ many(strOf({"++", "--", "+", "-", "!", "~", "*", "&"})) + xpr(14),
    /*14*/ xpr(15) + many(
               strOf({"++", "--"})
            || '(' + opt(expr + many(char1(',') + expr)) + ')'
            || '[' + expr + ']'
            || '.' + read(sym)
            || str("->") + read(sym)),
    /*15*/ read(char1('(') + expr + char1(')') || num || lstr || lchar || sym),
};
Parser<std::string> xpr(int n) {
    return [=](Source *s){ return xprs[n](s); };
}

extern Parser<std::string> sentence_;
Parser<std::string> sentence = [](Source *s) { return sentence_(s); };

auto varname = many('*') + read(sym) + opt('[' + opt(num) + ']' || chr('(') + ')');
auto var = type + right(" ") + varname + many(',' + varname) + ';';
auto label = sym + ':';

auto return_ = word("return") + right(" ") + expr + ';';
auto for_ = word("for") + '(' + expr + ';' + expr + ';' + expr + ')' + sentence;
auto if_ = word("if") + '(' + expr + ')' + sentence + opt(word("else") + right(" ") + sentence);
auto while_ = word("while") + '(' + expr + ')' + sentence;
auto do_ = word("do") + sentence + word("while") + '(' + expr + ')' + ';';
auto case_ = (word("case") + right(" ") + expr || word("default")) + ':';
auto switch_ = word("switch") + '(' + expr + ')' +
    '{' + case_ + many(case_ || sentence) + '}';
auto goto_ = word("goto") + right(" ") + sym + ';';

Parser<std::string> sentence_ = '{' + many(sentence) + '}' ||
    return_ || for_ || if_ || while_ || do_ || switch_ || goto_ ||
    tryp(label) || expr + ';';

struct Glob {
    std::string name;
    Glob() {}
    Glob(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Glob &f) {
    cout << f.name;
}
Parser<Glob> func = [](Source *s) {
    Glob g = (sym + '(' + opt(log(sym, "arg") + many(',' + log(sym, "arg"))) + ')')(s);
    many(log(var, "argtype"))(s);
    chr('{')(s);
    many(log(var, "var"))(s);
    many(log(sentence, "sentence"))(s);
    chr('}')(s);
    return g;
};
Parser<Glob> struct_ = [](Source *s) {
    Glob g = (word("struct") + right(" ") + read(sym))(s);
    std::cerr << g << std::endl;
    ('{' + many(var) + '}' + opt(many('*') + sym) + ';')(s);
    return g;
};
Parser<Glob> gvar = [](Source *s) {
    Glob g = (type + right(" ") + sym)(s);
    (opt('[' + opt(num) + ']') + opt(expr) + ';')(s);
    return g;
};
auto decls = many(read(tryp(struct_) || tryp(gvar) || func));

void test() {
    parseTest(expr, "1 >> 2");
    parseTest(expr, "a++");
    parseTest(expr, "++a");
    parseTest(expr, "*a");
    parseTest(expr, "a + b");
    parseTest(expr, "a + *b");
    parseTest(expr, "*a + *b");
    parseTest(expr, "a & b");
    parseTest(expr, "a & *b");
    parseTest(expr, "*a & *b");
    parseTest(expr, "a && b");
    parseTest(expr, "a && *b");
    parseTest(expr, "*a && *b");
}

class CSource {
    const char *s;
    std::vector<Token> toks;
public:
    CSource(const char *s) : s(s) {}
private:
    void skip() {
        for (;;) {
            for (; *s && *s <= ' '; ++s);
            if (s[0] != '/' || s[1] != '*') break;
            for (s += 2; *s; ++s) {
                if (s[0] == '*' && s[1] == '/') {
                    s += 2;
                    break;
                }
            }
        }
    }
public:
    const std::vector<Token> &tokens() const {
        return toks;
    }
    bool read() {
        skip();
        if (!*s) return false;
        auto start = s;
        if (isDigit(*s)) {
            for (++s; *s && isDigit(*s); ++s);
            toks.push_back(Token(Num, std::string(start, s - start)));
            return true;
        } else if (isLetter(*s)) {
            for (++s; *s && isLetter(*s) || isDigit(*s); ++s);
            toks.push_back(Token(Sym, std::string(start, s - start)));
            return true;
        } else if (*s == '"' || *s == '\'') {
            auto t = *s == '"' ? Str : Char;
            char ch = *(s++);
            for (;; ++s) {
                if (!*s) return false;
                if (*s == ch) {
                    ++s;
                    break;
                } else if (*s == '\\') {
                    if (!*++s) return false;
                }
            }
            toks.push_back(Token(t, std::string(start, s - start)));
            return true;
        } else {
            switch (*(s++)) {
            case '+':
            case '-':
            case '&':
            case '|':
                if (*s == *start) ++s;
                break;
            case '<':
            case '>':
                if (*s == *start || *s == '=') ++s;
                break;
            case '!':
                if (*s == '=') ++s;
                break;
            case '=':
                switch (*s) {
                case '^':
                case '=':
                case '+':
                case '-':
                case '*':
                case '/':
                case '&':
                case '|':
                case '%':
                    ++s;
                    break;
                case '<':
                case '>':
                    if (s[0] == s[1]) s += 2;
                    break;
                }
            }
            toks.push_back(Token(Op, std::string(start, s - start)));
            return true;
        }
    }
};

class PExpr {
public:
    virtual ~PExpr() {}
    virtual std::string str() const = 0;
};
std::ostream &operator<<(std::ostream &cout, PExpr *x) {
    return cout << x->str();
}

class PNum : public PExpr {
    int n;
public:
    PNum(Source *s) {
        std::istringstream iss(num(s));
        iss >> n;
    }
    virtual std::string str() const {
        std::ostringstream oss;
        oss << n;
        return oss.str();
    }
};

class PAdd : public PExpr {
    PExpr *x, *y;
public:
    PAdd(PExpr *x, Source *s) : x(x) {
        char1('+')(s);
        y = new PNum(s);
    }
    virtual std::string str() const {
        return x->str() + "+" + y->str();
    }
};

Parser<PExpr *> pexpr = [](Source *s) -> PExpr * {
    auto x = new PNum(s);
    try {
        return new PAdd(x, s);
    } catch (const std::string &) {}
    return x;
};

void test2() {
    parseTest(pexpr, "123");
    parseTest(pexpr, "1+2");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        test2();
        //std::cerr << "usage: " << argv[0] << " source.c" << std::endl;
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        std::cerr << "can not open: " << argv[1] << std::endl;
        return 1;
    }
    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(size + 1);
    fread(&buf[0], 1, size, f);
    fclose(f);

    //Source s = &buf[0];
    CSource cs = &buf[0];
    while (cs.read());
    auto tokens = cs.tokens();
    std::cerr << "[";
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        if (it != tokens.begin()) std::cerr << ",";
        std::cerr << *it;
    }
    std::cerr << "]" << std::endl;
    //parseTest(decls, &s);
    //if (!s.eof()) {
    //    std::cerr << s.ex2("not eof");
    //}
}

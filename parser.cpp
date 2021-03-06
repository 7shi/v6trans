#include <stdio.h>
#include <vector>
#include <utility>
#include <numeric>
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

Parser<int> pint = [](Source *s) {
    std::istringstream iss(num(s));
    int n;
    iss >> n;
    return n;
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
    PNum(int n) : n(n) {}
    virtual std::string str() const {
        std::ostringstream oss;
        oss << n;
        return oss.str();
    }
};
Parser<PExpr *> pnum = [](Source *s) { return new PNum(pint(s)); };

class PSym : public PExpr {
    std::string sym;
public:
    PSym(const std::string &sym) : sym(sym) {}
    virtual std::string str() const { return sym; }
};
Parser<PExpr *> psym = [](Source *s) { return new PSym(sym(s)); };

class POp1 : public PExpr {
protected:
    std::string op;
    PExpr *x;
public:
    POp1(const std::string &op, PExpr *x) : op(op), x(x) {}
    virtual std::string str() const {
        return "(" + op + x->str() + ")";
    }
};
class POp2 : public POp1 {
protected:
    PExpr *y;
public:
    POp2(const std::string &op, PExpr *x, PExpr *y) : POp1(op, x), y(y) {}
    virtual std::string str() const {
        return "(" + x->str() + op + y->str() + ")";
    }
};
class POp3 : public POp2 {
    std::string op2;
    PExpr *z;
public:
    POp3(const std::string &op, const std::string &op2,
        PExpr *x, PExpr *y, PExpr *z) : POp2(op, x, y), op2(op2), z(z) {}
    virtual std::string str() const {
        return "(" + x->str() + op + y->str() + op2 + z->str() + ")";
    }
};

Parser<PExpr *> pxpr(int n);
Parser<PExpr *> pexpr = read(pxpr(0));
Parser<PExpr *> evalMany(int n, const Parser<std::string> &op) {
    return [=](Source *s) {
        auto ret = pxpr(n)(s);
        try {
            for (;;) {
                auto o = op(s);
                ret = new POp2(o, ret, pxpr(n)(s));
            }
        } catch (const std::string &) {}
        return ret;
    };
}
Parser<PExpr *> evalRec(int n, const Parser<std::string> &op) {
    return [=](Source *s) {
        auto ret = pxpr(n)(s);
        try {
            auto o = op(s);
            ret = new POp2(o, ret, evalRec(n, op)(s));
        } catch (const std::string &) {}
        return ret;
    };
}
Parser<PExpr *> evalPre(int n, const Parser<std::string> &op) {
    return [=](Source *s) -> PExpr * {
        try {
            auto o = op(s);
            return new POp1(o, evalPre(n, op)(s));
        } catch (const std::string &) {}
        return pxpr(n)(s);
    };
}
Parser<PExpr *> pxprs[] = {
    /* 0*/ evalMany( 1, string(",")),
    /* 1*/ evalRec ( 2, char1('=') + opt(oneOf("+-*/%&^|") * 1 || stringOf({"<<", ">>"}))),
    /* 2*/ [=](Source *s) -> PExpr * {
        auto ret = pxpr(3)(s);
        try {
            auto cond = (char1('?') >> pxpr(3) << char1(':'))(s);
            return new POp3("?", ":", ret, cond, pxpr(3)(s));
        } catch (const std::string &) {}
        return ret;
    },
    /* 3*/ evalMany( 4, string("||")),
    /* 4*/ evalMany( 5, string("&&")),
    /* 5*/ evalMany( 6, tryp(char1('|') + nochar('|'))),
    /* 6*/ evalMany( 7, string("^")),
    /* 7*/ evalMany( 8, tryp(char1('&') + nochar('&'))),
    /* 8*/ evalMany( 9, stringOf({"==", "!="})),
    /* 9*/ evalMany(10, stringOf({"<=", ">=", "<", ">"})),
    /*10*/ evalMany(11, stringOf({"<<", ">>"})),
    /*11*/ evalMany(12, oneOf("+-" ) * 1),
    /*12*/ evalMany(13, oneOf("*/%") * 1),
    /*13*/ evalPre (14, stringOf({"++", "--"}) || oneOf("+-!~*&") * 1),
    /*14*/ pxpr(15),
    /*15*/ read(char1('(') >> pexpr << char1(')') || pnum || psym),
};
Parser<PExpr *> pxpr(int n) {
    return [=](Source *s){ return pxprs[n](s); };
}

void test2() {
    parseTest(pexpr, "123");
    parseTest(pexpr, "1+2");
    parseTest(pexpr, "1 + 2 - 3");
    parseTest(pexpr, "1+2*3");
    parseTest(pexpr, "(1+2)*3");
    parseTest(pexpr, "1,2+3");
    parseTest(pexpr, "a+b*c");
    parseTest(pexpr, "a+b*c>3 && d==1|e || f<<1<g&h");
    parseTest(pexpr, "a=b=1");
    parseTest(pexpr, "a=1,b=2");
    parseTest(pexpr, "a=<<b=+2");
    parseTest(pexpr, "-*++--a");
    parseTest(pexpr, "a==1?b+1:++c");
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

    Source s = &buf[0];
    parseTest(decls, &s);
    if (!s.eof()) {
        std::cerr << s.ex2("not eof");
    }
}

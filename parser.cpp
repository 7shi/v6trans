#include <stdio.h>
#include <vector>
#include "parsecpp.h"

template <typename T>
Parser<T> trace(const Parser<T> &p, const std::string &tag) {
    std::cerr << tag << std::endl;
    return [=](Source *s) {
        static std::string space;
        std::cerr << space << "<" << tag << ">" << std::endl;
        auto bak = space;
        space += "  ";
        T ret = watch(p, tag)(s);
        std::cerr << space << ret << std::endl;
        space = bak;
        std::cerr << space << "</" << tag << ">" << std::endl;
        return ret;
    };
}

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
Parser<std::string> str(const std::string &s) { return read(tryp(string(s))); }
Parser<std::string> strOf(const std::list<std::string> &list) {
    return read(Parser<std::string>([=](Source *s) -> std::string {
        for (auto it = list.begin(); it != list.end(); ++it) {
            try {
                return str(*it)(s);
            } catch (const std::string &) {}
        }
        throw s->ex("not match: " + toString(list));
    }));
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
auto type = strOf({"int", "char"}) + many(chr('*'));

extern Parser<std::string> sentence_, expr0;
Parser<std::string> expr = [](Source *s) { return read(expr0)(s); };
Parser<std::string> sentence = [](Source *s) { return sentence_(s); };

auto expr15 = read(char1('(') + expr + char1(')') || num || lstr || lchar || sym);
auto expr14 = trace(expr15, "expr15") + many(
       strOf({"++", "--"})
    || chr('(') + opt(expr + many(char1(',') + expr)) + chr(')')
    || chr('[') + expr + chr(']')
    || chr('.') + read(sym)
    || str("->") + read(sym));
auto expr13 = many(strOf({"++", "--", "+", "-", "!", "~", "*", "&"})) + trace(expr14, "expr14");
auto expr12 = trace(expr13, "expr13") + many(strOf({"*", "/", "%"}) + expr13);
auto expr11 = trace(expr12, "expr12") + many(strOf({"+", "-"}) + read(expr12));
auto expr10 = trace(expr11, "expr11") + many(strOf({"<<", ">>"}) + expr11);
auto expr9 = trace(expr10, "expr10") + many(strOf({"<=", ">=", "<", ">"}) + read(expr10));
auto expr8 = trace(expr9, "expr9") + many(strOf({"==", "!="}) + expr9);
auto expr7 = trace(expr8, "expr8") + many(tryp(char1('&') + nochar('&')) + read(expr8));
auto expr6 = trace(expr7, "expr7") + many(chr('^') + expr7);
auto expr5 = trace(expr6, "expr6") + many(tryp(char1('|') + nochar('|')) + read(expr6));
auto expr4 = trace(expr5, "expr5") + many(str("&&") + expr5);
auto expr3 = trace(expr4, "expr4") + many(str("||") + expr4);
auto expr2 = trace(expr3, "expr3") + opt(chr('?') + expr + chr(':') + expr);
auto expr1 = trace(expr2, "expr2") + opt(chr('=') + opt(strOf({"+", "-", "*", "/", "%", "<<", ">>", "&", "^", "|"})) + expr);
Parser<std::string> expr0 = trace(expr1, "expr1") + many(chr(',') + expr1);

auto var1 = sym + opt(chr('[') + opt(num) + chr(']') || chr('(') + chr(')'));
auto var = type + right(" ") + var1 + many(chr(',') + many(chr('*')) + var1) + chr(';');
auto return_ = str("return") + right(" ") + expr + chr(';');
auto for_ = str("for") + chr('(') + expr + chr(';') + expr + chr(';') + expr + chr(')') + sentence;
auto if_ = str("if") + chr('(') + expr + chr(')') + sentence + opt(str("else") + right(" ") + sentence);
auto while_ = str("while") + chr('(') + expr + chr(')') + sentence;
auto do_ = str("do") + sentence + str("while") + chr('(') + expr + chr(')') + chr(';');
auto case_ = log((str("case") + right(" ") + expr || str("default")) + chr(':'), "case");
auto switch_ = str("switch") + log(chr('(') + expr + chr(')'), "switch") +
    chr('{') + case_ + many(case_ || sentence) + chr('}');

Parser<std::string> sentence_ = chr('{') + many(sentence) + chr('}') ||
    return_ || for_ || if_ || while_ || do_ || switch_ || expr + chr(';');

struct Glob {
    std::string name;
    Glob() {}
    Glob(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Glob &f) {
    cout << f.name;
}
Parser<Glob> func = [](Source *s) {
    Glob g = (sym + chr('(') + opt(log(sym, "arg") + many(chr(',') + log(sym, "arg"))) + chr(')'))(s);
    many(log(type + right(" ") + sym + chr(';'), "argtype"))(s);
    chr('{')(s);
    many(log(var, "var"))(s);
    many(log(sentence, "sentence"))(s);
    chr('}')(s);
    return g;
};
Parser<Glob> struct_ = [](Source *s) {
    Glob g = (string("struct") + right(" ") + read(sym))(s);
    std::cerr << g << std::endl;
    (chr('{') + many(var) + chr('}') + opt(many(chr('*')) + sym) + chr(';'))(s);
    return g;
};
Parser<Glob> gvar = [](Source *s) {
    Glob g = (type + right(" ") + sym)(s);
    (opt(chr('[') + opt(num) + chr(']')) + opt(expr) + chr(';'))(s);
    return g;
};
auto decls = many(read(log(tryp(struct_), "struct") || tryp(gvar) || func));

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "start" << std::endl;
        parseTest(trace(expr, "expr"), "1 >> 2");
        parseTest(trace(expr, "expr"), "a++");
        parseTest(trace(expr, "expr"), "++a");
        parseTest(trace(expr, "expr"), "*a");
        parseTest(trace(expr, "expr"), "a + b");
        parseTest(trace(expr, "expr"), "a + *b");
        parseTest(trace(expr, "expr"), "*a + *b");
        parseTest(trace(expr, "expr"), "a & b");
        parseTest(trace(expr, "expr"), "a & *b");
        parseTest(trace(expr, "expr"), "*a & *b");
        parseTest(trace(expr, "expr"), "a && b");
        parseTest(trace(expr, "expr"), "a && *b");
        parseTest(trace(expr, "expr"), "*a && *b");
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

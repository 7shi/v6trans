#include <stdio.h>
#include <vector>
#include "parsecpp.h"

auto spc = char1(' ') || char1('\t') || char1('\n');
template <typename T>
Parser<T> read(const Parser<T> &p) {
    return many(spc) >> p << many(spc);
}
Parser<char> chr(char ch) { return read(char1(ch)); }
Parser<std::string> str(const std::list<std::string> &list) {
    return [=](Source *s) -> std::string {
        for (auto it = list.begin(); it != list.end(); ++it) {
            Source bak = *s;
            try {
                return string(*it)(s);
            } catch (const std::string &) {}
            *s = bak;
        }
        throw s->ex("not match: " + toString(list));
    };
}
Parser<std::string> opt(const Parser<std::string> &p) {
    return tryp(p) || right("");
}
Parser<std::string> opt(const Parser<char> &p) {
    return tryp(p + right("")) || right("");
}
Parser<std::string> nochar(char ch) {
    return [=](Source *s) {
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
auto type = str({"int", "char"}) + many(chr('*'));

extern Parser<std::string> sentence_, expr0;
Parser<std::string> expr = [](Source *s) { return read(expr0)(s); };
Parser<std::string> sentence = [](Source *s) { return sentence_(s); };

auto var = type + right(" ") + sym + chr(';');
auto ret = string("return") + right(" ") + expr;
auto for_ = string("for") + chr('(') + expr + chr(';') + expr + chr(';') + expr + chr(')') + sentence;

auto expr15 = read(char1('(') + expr + char1(')') || num || lstr || lchar || sym);
auto expr14 = expr15 + many(
    char1('(') + opt(expr + many(char1(',') + expr)) + char1(')')
    || char1('[') + expr + char1(']'));
auto expr13 = opt(str({"++", "--", "+", "-", "!", "~", "*", "&"})) + expr14;
auto expr12 = expr13 + many(str({"*", "/", "%"}) + expr13);
auto expr11 = expr12 + many(str({"+", "-"}) + expr12);
auto expr10 = expr11 + many(str({"<<", ">>"}) + expr11);
auto expr9 = expr10 + many((str({"<=", ">="})
    || chr('<') + nochar('<') || chr('>') + nochar('>')) + expr10);
auto expr8 = expr9 + many(str({"==", "!="}) + expr9);
auto expr7 = expr8 + many(char1('&') + expr8);
auto expr6 = expr7 + many(char1('^') + expr7);
auto expr5 = expr6 + many(char1('|') + expr6);
auto expr4 = expr5 + many(char1('&') + char1('&') + expr5);
auto expr3 = expr4 + many(char1('|') + char1('|') + expr4);
auto expr2 = expr3 + opt(char1('?') + expr + char1(':') + expr);
auto expr1 = expr2 + opt(char1('=') + opt(str({"+", "-", "*", "/", "%", "<<", ">>", "&", "^", "|"})) + expr);
Parser<std::string> expr0 = expr1 + many(char1(',') + expr1);

Parser<std::string> sentence_ = read(
    chr('{') + many(sentence) + chr('}') ||
    (ret || for_ || expr) + chr(';'));

struct Func {
    std::string name;
    Func(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Func &f) {
    cout << f.name << "()";
}
Parser<Func> func = [](Source *s) {
    Func f = sym(s);
    chr('(')(s);
    opt(log(sym, "arg") + many(chr(',') + log(sym, "arg")))(s);
    chr(')')(s);
    many(log(type + right(" ") + sym + chr(';'), "argtype"))(s);
    chr('{')(s);
    many(log(var, "var"))(s);
    many(log(sentence, "sentence"))(s);
    chr('}')(s);
    return f;
};

auto decls = many(func);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        parseTest(expr, "1 >> 2");
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

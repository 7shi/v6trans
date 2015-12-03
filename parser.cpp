#include <stdio.h>
#include <vector>
#include "parsecpp.h"

auto spc = char1(' ') || char1('\t') || char1('\n');
template <typename T>
Parser<T> read(const Parser<T> &p) {
    return many(spc) >> p << many(spc);
}
Parser<char> chr(char ch) { return read(char1(ch)); }
Parser<std::string> str(const std::string &s) { return read(string(s)); }
Parser<std::string> strl(const std::list<std::string> &list) {
    return [=](Source *s) -> std::string {
        many(spc)(s);
        for (auto it = list.begin(); it != list.end(); ++it) {
            try {
                return tryp(str(*it))(s);
            } catch (const std::string &) {}
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

auto sym = log(read(letter + many(letter || digit)), "sym");
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
auto type = strl({"int", "char"}) + many(chr('*'));

extern Parser<std::string> expr0, sentence_;
Parser<std::string> expr = [](Source *s) { return expr0(s); };
Parser<std::string> sentence = [](Source *s) { return sentence_(s); };

auto ret = str("return") + right(" ") + expr;
auto let = sym + strl({"=+", "=-", "=*", "=/", "=<<", "=>>", "="}) + expr;
auto var = type + right(" ") + sym + chr(';');
auto for_ = str("for") + chr('(') + let + chr(';') + expr + chr(';') + expr + chr(')') + sentence;

Parser<std::string> varOrCall = [](Source *s) {
    auto sy = sym(s);
    try {
        chr('(')(s);
    } catch (const std::string &) {
        return sy + opt(strl({"++", "--"}) || chr('[') + expr + chr(']'))(s);
    }
    return sy + "(" + log((opt(expr + many(chr(',') + expr)) + chr(')')), "call")(s);
};

auto factor = read(tryp(chr('(') + log(expr, "()") + chr(')')) || num || lstr || lchar || varOrCall);
auto expr8 = factor + opt(chr('?') + expr + chr(':') + expr);
auto expr7 = expr8 + many(strl({"&", "|"}) + expr8);
auto expr6 = expr7 + many(str("||") + expr7);
auto expr5 = expr6 + many(str("&&") + expr6);
auto expr4 = expr5 + many(strl({"==", "!="}) + expr5);
auto expr3 = expr4 + many(log(strl({"<=", "<", ">=", ">"}), "expr3") + expr4);
auto expr2 = expr3 + many(strl({"*", "/"}) + expr3);
auto expr1 = expr2 + many(strl({"+", "-"}) + expr2);
Parser<std::string> expr0 = log(expr1 + many(log(strl({"<<", ">>"}), "<< >>") + expr1), "expr");

Parser<std::string> sentence_ = chr('{') + many(sentence) + chr('}') ||
    (tryp(ret) || tryp(let) || for_ || expr) + chr(';');

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

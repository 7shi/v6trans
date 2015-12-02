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
Parser<std::string> opt(const Parser<std::string> &p) {
    return tryp(p) || right("");
}

auto sym = read(letter + many(letter || digit));
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

extern Parser<std::string> expr_;
Parser<std::string> expr = [](Source *s) { return expr_(s); };

Parser<std::string> call =
    sym + chr('(') + opt(expr + many(chr(',') + expr)) + chr(')');
auto ret = str("return") + right(" ") + expr;
auto let = sym + chr('=') + expr;
auto var = str("int") + right(" ") + sym + chr(';');

auto factor = read(
    tryp(chr('(') + expr + chr(')')) || tryp(call) || num || lstr || sym);
auto term = factor + many(chr('*') + factor || chr('/') + factor);
Parser<std::string> expr_ = term + many(chr('+') + term || chr('-') + term);

auto sentence = (tryp(ret) || tryp(let) || expr) + chr(';');
auto body = many(log(var, "var")) + many(log(sentence, "sentence"));

struct Func {
    std::string name;
    Func(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Func &f) {
    cout << f.name << "()";
}
Parser<Func> func = [](Source *s) {
    Func f = sym(s);
    (chr('(') >> chr(')'))(s);
    chr('{')(s);
    body(s);
    chr('}')(s);
    return f;
};

auto decls = many(func);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " source.c" << std::endl;
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
        std::cerr << s.ex("not eof") << std::endl;
    }
}

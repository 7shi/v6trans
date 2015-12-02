#include <stdio.h>
#include <vector>
#include "parsecpp.h"

template <typename T>
Parser<T> watch(const Parser<T> &p, const std::string &msg) {
    return [=](Source *s) {
        T ret;
        try {
            ret = p(s);
        } catch (const std::string &e) {
            std::cerr << msg << ":" << e << std::endl;
            throw;
        }
        return ret;
    };
}

template <typename T>
Parser<T> log(const Parser<T> &p, const std::string &tag) {
    return [=](Source *s) {
        T ret = watch(p, tag)(s);
        std::cerr << "<" << tag << ">" << ret << "</" << tag << ">" << std::endl;
        return ret;
    };
}

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

auto symbol = read(letter + many(letter || digit));
auto num = many1(digit);

auto var = symbol;
Parser<std::string> lstr = [](Source *s) {
    std::string ret;
    spaces(s);
    char1('"')(s);
    for (;;) {
        char ch = anyChar(s);
        if (ch == '"') break;
        ret += ch;
        if (ch == '\\') ret += anyChar(s);
    }
    return "\"" + ret + "\"";
};
auto expr = tryp(var) || tryp(num) || lstr;
Parser<std::string> call =
    symbol + chr('(') + opt(expr + many(chr(',') + expr)) + chr(')');
auto ret = str("return") + right(" ") + expr;
auto sentence = (tryp(call) || ret) + chr(';');
auto body = many(log(sentence, "sentence"));

struct Func {
    std::string name;
    Func(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Func &f) {
    cout << f.name << "()";
}
Parser<Func> func = [](Source *s) {
    Func f = symbol(s);
    (chr('(') >> chr(')'))(s);
    chr('{')(s);
    body(s);
    chr('}')(s);
    return f;
};

auto decls = many(func);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s source.c\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "can not open: %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    auto size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(size + 1);
    fread(&buf[0], 1, size, f);
    fclose(f);
    parseTest(decls, &buf[0]);
}

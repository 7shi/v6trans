#include <stdio.h>
#include <vector>
#include "parsecpp.h"

template <typename T>
Parser<T> read(const Parser<T> &p) {
    return spaces >> p << spaces;
}
Parser<char> chr(char ch) { return read(char1(ch)); }

auto symbol = letter + many(letter || digit);

struct Func {
    std::string name;
    Func(const std::string &name) : name(name) {}
};
std::ostream &operator<<(std::ostream &cout, const Func &f) {
    cout << f.name << "()";
}
Parser<Func> func = [](Source *s) {
    Func f = read(symbol)(s);
    (chr('(') >> chr(')') >> chr('{') >> chr('}'))(s);
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

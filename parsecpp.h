#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <list>

template <typename T>
std::string toString(const std::list<T> &list) {
    std::stringstream ss;
    ss << "[";
    for (auto it = list.begin();
            it != list.end(); ++it) {
        if (it != list.begin()) ss << ",";
        ss << *it;
    }
    ss << "]";
    return ss.str();
}
template <typename T>
std::ostream &operator<<(std::ostream &cout, const std::list<T> &list) {
    return cout << toString(list);
}

class Source {
    const char *s;
    int line, col;
public:
    Source(const char *s);
    char peek();
    void next();
    std::string ex(const std::string &e);
    bool operator==(const Source &src) const;
    bool operator!=(const Source &src) const;
};

template <typename T>
using Parser = std::function<T (Source *)>;

template <typename T>
void parseTest(const Parser<T> &p, const Source &src) {
    Source s = src;
    try {
        std::cout << p(&s) << std::endl;
    } catch (const std::string &e) {
        std::cout << e << std::endl;
    }
}

extern Parser<char> satisfy(const std::function<bool (char)> &f);
extern Parser<char> anyChar;

template <typename T1, typename T2>
Parser<std::string> operator+(const Parser<T1> &x, const Parser<T2> &y) {
    return [=](Source *s) {
        std::string ret;
        ret += x(s);
        ret += y(s);
        return ret;
    };
}

template <typename T>
Parser<std::string> operator*(int n, const Parser<T> &x) {
    return [=](Source *s) {
        std::string ret;
        for (int i = 0; i < n; ++i) {
            ret += x(s);
        }
        return ret;
    };
}
template <typename T>
Parser<std::string> operator*(const Parser<T> &x, int n) {
    return n * x;
}

template <typename T1, typename T2>
Parser<T1> operator<<(const Parser<T1> &p1, const Parser<T2> &p2) {
    return [=](Source *s) {
        T1 ret = p1(s);
        p2(s);
        return ret;
    };
}

template <typename T1, typename T2>
Parser<T2> operator>>(const Parser<T1> &p1, const Parser<T2> &p2) {
    return [=](Source *s) {
        p1(s);
        return p2(s);
    };
}

template <typename T1, typename T2>
Parser<T1> apply(const std::function <T1 (const T2 &)> &f, const Parser<T2> &p) {
    return [=](Source *s) {
        return f(p(s));
    };
}
template <typename T1, typename T2, typename T3>
Parser<std::function<T1 (const T2 &)>> apply(
        const std::function <T1 (const T2 &, const T3 &)> &f, const Parser<T2> &p) {
    return [=](Source *s) {
        T2 x = p(s);
        return [=](const T3 &y) {
            return f(x, y);
        };
    };
}

template <typename T>
Parser<T> operator-(const Parser<T> &p) {
    return apply<T, T>([=](T x) { return -x; }, p);
}

template <typename T>
Parser<std::string> many_(const Parser<T> &p) {
    return [=](Source *s) {
        std::string ret;
        try {
            for (;;) ret += p(s);
        } catch (const std::string &) {}
        return ret;
    };
}
extern Parser<std::string> many(const Parser<char> &p);
extern Parser<std::string> many(const Parser<std::string> &p);
template <typename T>
Parser<std::list<T>> many(const Parser<T> &p) {
    return [=](Source *s) {
        std::list<T> ret;
        try {
            for (;;) ret.push_back(p(s));
        } catch (const std::string &) {}
        return ret;
    };
}

template <typename T>
Parser<std::string> many1(const Parser<T> &p) {
    return p + many(p);
}

template <typename T>
const Parser<T> operator||(const Parser<T> &p1, const Parser<T> &p2) {
    return [=](Source *s) {
        T ret;
        Source bak = *s;
        try {
            ret = p1(s);
        } catch (const std::string &) {
            if (*s != bak) throw;
            ret = p2(s);
        }
        return ret;
    };
}

template <typename T>
Parser<T> tryp(const Parser<T> &p) {
    return [=](Source *s) {
        T ret;
        Source bak = *s;
        try {
            ret = p(s);
        } catch (const std::string &) {
            *s = bak;
            throw;
        }
        return ret;
    };
}

template <typename T>
Parser<T> right(const T &t) {
    return [=](Source *s) {
        return t;
    };
}
extern Parser<std::string> right(const char *);

template <typename T>
Parser<T> left(const std::string &e) {
    return [=](Source *s) -> T {
        throw s->ex(e);
    };
}
extern Parser<char> left(const std::string &e);

extern Parser<char> char1(char ch);
extern Parser<std::string> string(const std::string &str);

extern bool isDigit   (char ch);
extern bool isUpper   (char ch);
extern bool isLower   (char ch);
extern bool isAlpha   (char ch);
extern bool isAlphaNum(char ch);
extern bool isLetter  (char ch);
extern bool isSpace   (char ch);

extern Parser<char> digit;
extern Parser<char> upper;
extern Parser<char> lower;
extern Parser<char> alpha;
extern Parser<char> alphaNum;
extern Parser<char> letter;
extern Parser<char> space;

extern Parser<std::string> spaces;

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

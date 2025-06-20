#include "literal.h"
#include "supio.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

std::string to_hsp_literal_string(const char *src) {
    std::string out;
    out.push_back('"');
    const unsigned char *s = reinterpret_cast<const unsigned char*>(src);
    while (*s) {
        unsigned char c = *s;
        switch (c) {
        case '\t':
            out += "\\t";
            break;
        case '\r':
            out += "\\";
            if (*(s+1) == '\n') {
                out += 'n';
                ++s;
            } else {
                out += 'r';
            }
            break;
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        default:
            out.push_back(c);
            if (issjisleadbyte(c) && *(s+1) != '\0') {
                out.push_back(*++s);
            }
            break;
        }
        ++s;
    }
    out.push_back('"');
    return out;
}

char *to_hsp_string_literal(const char *src) {
    std::string tmp = to_hsp_literal_string(src);
    char *dest = (char*)malloc(tmp.size() + 1);
    if (dest) {
        memcpy(dest, tmp.c_str(), tmp.size() + 1);
    }
    return dest;
}

std::string to_hsp_literal_int(int value) {
    char buf[32];
    sprintf(buf, "%d", value);
    return std::string(buf);
}

std::string to_hsp_literal_double(double value) {
    char buf[64];
    sprintf(buf, "%g", value);
    return std::string(buf);
}

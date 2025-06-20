#ifndef HSPCMP_LITERAL_H
#define HSPCMP_LITERAL_H

#include <string>

// Types available for literal representation
enum class HspLiteralType {
    String,
    Int,
    Double,
};

// Convert string to HSP literal representation (double quoted and escaped)
std::string to_hsp_literal_string(const char *src);

// Convert numeric values to their HSP literal representation
std::string to_hsp_literal_int(int value);
std::string to_hsp_literal_double(double value);

// Legacy interface returning malloc'ed C-string
char *to_hsp_string_literal(const char *src);

#endif

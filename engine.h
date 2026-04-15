#ifndef ENGINE_H
#define ENGINE_H

#include <stddef.h>

#define CALC_PRECISION 10
#define CALC_EPSILON 1e-10
#define CALC_BUF_SIZE 64
#define MAX_STACK 256

typedef enum {
	OP_NONE = 0,
	OP_ADD = '+',
	OP_SUB = '-',
	OP_MUL = '*',
	OP_DIV = '/',
	OP_MOD = '%',
	OP_POW = '^',
	OP_SQRT = 's'
} OperatorType;

/*
 * Evaluates a mathematical expression string (infix) and returns the result.
 * Supports +, -, *, /, %, ^, sqrt(), PI, E, (, ).
 * If an error occurs, returns 0.0 and sets *error_msg (must be freed by
 * caller).
 */
double engine_eval(const char* expression, char** error_msg);

/*
 * Formats a double value as a string, avoiding scientific notation
 * and trimming trailing zeros.
 */
void engine_format_output(char* buf, size_t len, double val);

#endif /* ENGINE_H */

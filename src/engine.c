#include "engine.h"

#include <ctype.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

typedef struct {
	double data[MAX_STACK];
	int top;
} DoubleStack;

typedef struct {
	char data[MAX_STACK];
	int top;
} CharStack;

static inline int dstack_push(DoubleStack* stk, double val, const char** err)
{
	if (stk->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = "Value stack overflow";
		}
		return 0;
	}
	stk->data[++(stk->top)] = val;
	return 1;
}

static inline double dstack_pop(DoubleStack* stk)
{
	return (stk->top >= 0) ? stk->data[(stk->top)--] : 0.0;
}

static inline int cstack_push(CharStack* stk, char val, const char** err)
{
	if (stk->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = "Operator stack overflow";
		}
		return 0;
	}
	stk->data[++(stk->top)] = val;
	return 1;
}

static inline char cstack_pop(CharStack* stk)
{
	return (stk->top >= 0) ? stk->data[(stk->top)--] : '\0';
}

static inline char cstack_peek(CharStack* stk)
{
	return (stk->top >= 0) ? stk->data[stk->top] : '\0';
}

static int get_prec(char op_char)
{
	switch (op_char) {
	case '+':
	case '-':
		return 1;
	case '*':
	case '/':
	case '%':
		return 2;
	case '^':
		return 3;
	case 's':
		return 4;
	default:
		return 0;
	}
}

static double apply_div(double operand_a, double operand_b, const char** err)
{
	if (fabs(operand_b) < CALC_EPSILON) {
		if (err && !*err) {
			*err = "Division by zero";
		}
		return 0.0;
	}
	return operand_a / operand_b;
}

static double apply_mod(double operand_a, double operand_b, const char** err)
{
	if (fabs(operand_b) < CALC_EPSILON) {
		if (err && !*err) {
			*err = "Modulo by zero";
		}
		return 0.0;
	}
	return fmod(operand_a, operand_b);
}

static double apply_pow(double operand_a, double operand_b, const char** err)
{
	if (fabs(operand_a) < CALC_EPSILON && operand_b < 0.0) {
		if (err && !*err) {
			*err = "Zero to a negative power is undefined";
		}
		return 0.0;
	}
	if (operand_a < 0.0 && fabs(floor(operand_b) - operand_b) > CALC_EPSILON) {
		if (err && !*err) {
			*err = "Negative base with fractional exponent";
		}
		return 0.0;
	}
	return pow(operand_a, operand_b);
}

static double apply_sqrt(double operand_b, const char** err)
{
	if (operand_b < 0.0) {
		if (err && !*err) {
			*err = "Square root of negative number";
		}
		return 0.0;
	}
	return sqrt(operand_b);
}

static double apply_op(double operand_a, double operand_b, OperatorType op_type,
 const char** err)
{
	switch (op_type) {
	case OP_ADD:
		return operand_a + operand_b;
	case OP_SUB:
		return operand_a - operand_b;
	case OP_MUL:
		return operand_a * operand_b;
	case OP_DIV:
		return apply_div(operand_a, operand_b, err);
	case OP_MOD:
		return apply_mod(operand_a, operand_b, err);
	case OP_POW:
		return apply_pow(operand_a, operand_b, err);
	case OP_SQRT:
		return apply_sqrt(operand_b, err);
	default:
		return 0.0;
	}
}

static OperatorType char_to_op(char op_char)
{
	switch (op_char) {
	case '+':
		return OP_ADD;
	case '-':
		return OP_SUB;
	case '*':
		return OP_MUL;
	case '/':
		return OP_DIV;
	case '%':
		return OP_MOD;
	case '^':
		return OP_POW;
	case 's':
		return OP_SQRT;
	default:
		return OP_NONE;
	}
}

static int proc_top(DoubleStack* vals, CharStack* ops, const char** err)
{
	char op_char = cstack_pop(ops);
	double val2 = dstack_pop(vals);
	OperatorType op_type = char_to_op(op_char);
	if (op_type == OP_SQRT) {
		if (!dstack_push(vals, apply_op(0.0, val2, OP_SQRT, err), err)) {
			return 0;
		}
	} else if (op_type != OP_NONE) {
		double val1 = dstack_pop(vals);
		if (!dstack_push(vals, apply_op(val1, val2, op_type, err), err)) {
			return 0;
		}
	}
	return (*err == NULL);
}

static int handle_paren(char paren_char, DoubleStack* vals, CharStack* ops,
 const char** err)
{
	if (paren_char == '(') {
		return cstack_push(ops, '(', err);
	}
	while (ops->top >= 0 && cstack_peek(ops) != '(') {
		if (!proc_top(vals, ops, err)) {
			return 0;
		}
	}
	if (ops->top < 0) {
		*err = "Mismatched parentheses";
		return 0;
	}
	cstack_pop(ops);
	if (ops->top >= 0 && cstack_peek(ops) == 's') {
		return proc_top(vals, ops, err);
	}
	return 1;
}

static int handle_op_tok(char curr_op, const char* expression, int idx,
 DoubleStack* vals, CharStack* ops, const char** err)
{
	if (curr_op == '-' &&
	 (idx == 0 || (idx > 0 && expression[idx - 1] == '('))) {
		if (!dstack_push(vals, 0.0, err)) {
			return 0;
		}
	}
	while (ops->top >= 0 &&
	 (curr_op == '^' ?
	   get_prec(cstack_peek(ops)) > get_prec(curr_op) :
	   get_prec(cstack_peek(ops)) >= get_prec(curr_op))) {
		if (!proc_top(vals, ops, err)) {
			return 0;
		}
	}
	return cstack_push(ops, curr_op, err);
}

static int handle_tok(const char* expression, int len, int idx,
 DoubleStack* vals, CharStack* ops, const char** err, int* next)
{
	if (idx < 0 || idx >= len) {
		return 0;
	}
	char curr_char = expression[idx];
	*next = idx;
	if (isdigit((unsigned char) curr_char) || curr_char == '.') {
		char* end_ptr;
		double val = g_ascii_strtod(expression + idx, &end_ptr);
		if (!dstack_push(vals, val, err)) {
			return 0;
		}
		*next = (int) (end_ptr - expression) - 1;
	} else if (curr_char == 'P' && idx + 1 < len &&
	 expression[idx + 1] == 'I') {
		*next = idx + 1;
		return dstack_push(vals, M_PI, err);
	} else if (curr_char == 'E' &&
	 (idx + 1 >= len || !isalpha((unsigned char) expression[idx + 1]))) {
		return dstack_push(vals, M_E, err);
	} else if (strncmp(expression + idx, "sqrt", 4) == 0) {
		*next = idx + 3;
		return cstack_push(ops, 's', err);
	} else if (curr_char == '(' || curr_char == ')') {
		return handle_paren(curr_char, vals, ops, err);
	} else {
		return handle_op_tok(curr_char, expression, idx, vals, ops, err);
	}
	return 1;
}

double engine_eval(const char* expression, char** error_msg)
{
	DoubleStack vals = {{0.0}, -1};
	CharStack ops = {{'\0'}, -1};
	const char* err = NULL;
	char* cln = calloc(1, strlen(expression) + 1);
	int len = 0;
	for (const char* ptr = expression; *ptr; ptr++) {
		if (!isspace((unsigned char) *ptr)) {
			cln[len++] = *ptr;
		}
	}
	cln[len] = '\0';
	for (int idx = 0; idx < len; idx++) {
		int next = idx;
		if (!handle_tok(cln, len, idx, &vals, &ops, &err, &next)) {
			goto error;
		}
		idx = next;
	}
	while (ops.top >= 0) {
		if (cstack_peek(&ops) == '(') {
			err = "Mismatched parentheses";
			goto error;
		}
		if (!proc_top(&vals, &ops, &err)) {
			goto error;
		}
	}
	double res = dstack_pop(&vals);
	free(cln);
	return res;
error:
	if (error_msg) {
		*error_msg = g_strdup(err);
	}
	free(cln);
	return 0.0;
}

void engine_format_output(char* buf, size_t len, double val)
{
	snprintf(buf, len, "%.*f", CALC_PRECISION, val);
	if (strchr(buf, '.')) {
		char* ptr = buf + strlen(buf) - 1;
		while (ptr > buf && *ptr == '0') {
			*ptr-- = '\0';
		}
		if (ptr > buf && *ptr == '.') {
			*ptr = '\0';
		}
	}
}

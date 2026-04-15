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

static inline int
dstack_push(DoubleStack* stack, double val, const char** error)
{
	if (stack->top >= MAX_STACK - 1) {
		if (error && !*error) {
			*error = "Value stack overflow";
		}
		return 0;
	}
	stack->data[++(stack->top)] = val;
	return 1;
}

static inline double dstack_pop(DoubleStack* stack)
{
	return (stack->top >= 0) ? stack->data[(stack->top)--] : 0.0;
}

static inline int cstack_push(CharStack* stack, char val, const char** error)
{
	if (stack->top >= MAX_STACK - 1) {
		if (error && !*error) {
			*error = "Operator stack overflow";
		}
		return 0;
	}
	stack->data[++(stack->top)] = val;
	return 1;
}

static inline char cstack_pop(CharStack* stack)
{
	return (stack->top >= 0) ? stack->data[(stack->top)--] : '\0';
}

static inline char cstack_peek(CharStack* stack)
{
	return (stack->top >= 0) ? stack->data[stack->top] : '\0';
}

static int precedence(char op_char)
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

static double apply_op(double operand_a, double operand_b, OperatorType op_type,
 const char** error) /* NOLINT(bugprone-easily-swappable-parameters) */
{
	switch (op_type) {
	case OP_ADD:
		return operand_a + operand_b;
	case OP_SUB:
		return operand_a - operand_b;
	case OP_MUL:
		return operand_a * operand_b;
	case OP_DIV:
		if (fabs(operand_b) < CALC_EPSILON) {
			if (error && !*error) {
				*error = "Division by zero";
			}
			return 0.0;
		}
		return operand_a / operand_b;
	case OP_MOD:
		if (fabs(operand_b) < CALC_EPSILON) {
			if (error && !*error) {
				*error = "Modulo by zero";
			}
			return 0.0;
		}
		return fmod(operand_a, operand_b);
	case OP_POW:
		if (fabs(operand_a) < CALC_EPSILON && operand_b < 0.0) {
			if (error && !*error) {
				*error = "Zero to a negative power is undefined";
			}
			return 0.0;
		}
		if (operand_a < 0.0 &&
		 fabs(floor(operand_b) - operand_b) > CALC_EPSILON) {
			if (error && !*error) {
				*error = "Negative base with fractional exponent";
			}
			return 0.0;
		}
		return pow(operand_a, operand_b);
	case OP_SQRT:
		if (operand_b < 0.0) {
			if (error && !*error) {
				*error = "Square root of negative number";
			}
			return 0.0;
		}
		return sqrt(operand_b);
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

static int
process_top_op(DoubleStack* values, CharStack* ops, const char** current_err)
{
	char oper_char = cstack_pop(ops);
	double val2 = dstack_pop(values);
	OperatorType op_type = char_to_op(oper_char);

	if (op_type == OP_SQRT) {
		if (!dstack_push(values, apply_op(0.0, val2, OP_SQRT, current_err),
		     current_err)) {
			return 0;
		}
	} else if (op_type != OP_NONE) {
		double val1 = dstack_pop(values);
		if (!dstack_push(values, apply_op(val1, val2, op_type, current_err),
		     current_err)) {
			return 0;
		}
	}
	return (*current_err == NULL);
}

static int handle_parentheses(char curr_char, DoubleStack* values,
 CharStack* ops, const char** current_err)
{
	if (curr_char == '(') {
		return cstack_push(ops, '(', current_err);
	}
	while (ops->top >= 0 && cstack_peek(ops) != '(') {
		if (!process_top_op(values, ops, current_err)) {
			return 0;
		}
	}
	if (ops->top < 0) {
		*current_err = "Mismatched parentheses";
		return 0;
	}
	cstack_pop(ops);
	if (ops->top >= 0 && cstack_peek(ops) == 's') {
		return process_top_op(values, ops, current_err);
	}
	return 1;
}

static int handle_operator_token(char curr_op, const char* clean_expr, int idx,
 DoubleStack* values, CharStack* ops, const char** current_err)
{
	int is_unary =
	 (curr_op == '-' && (idx == 0 || (idx > 0 && clean_expr[idx - 1] == '(')));
	if (is_unary) {
		if (!dstack_push(values, 0.0, current_err)) {
			return 0;
		}
	}
	while (ops->top >= 0 &&
	 (curr_op == '^' ?
	   precedence(cstack_peek(ops)) > precedence(curr_op) :
	   precedence(cstack_peek(ops)) >= precedence(curr_op))) {
		if (!process_top_op(values, ops, current_err)) {
			return 0;
		}
	}
	return cstack_push(ops, curr_op, current_err);
}

static int handle_token(const char* clean_expr, int clean_len, int idx,
 DoubleStack* values, CharStack* ops, const char** current_err, int* next_idx)
{
	char curr_char;
	if (idx < 0 || idx >= clean_len) {
		return 0;
	}
	curr_char = clean_expr[idx];
	*next_idx = idx;

	if (isdigit((unsigned char) curr_char) || curr_char == '.') {
		char* end_ptr;
		double val = g_ascii_strtod(clean_expr + idx, &end_ptr);
		if (!dstack_push(values, val, current_err)) {
			return 0;
		}
		*next_idx = (int) (end_ptr - clean_expr) - 1;
	} else if (curr_char == 'P' && idx + 1 < clean_len &&
	 clean_expr[idx + 1] == 'I') {
		*next_idx = idx + 1;
		return dstack_push(values, M_PI, current_err);
	} else if (curr_char == 'E' &&
	 (idx + 1 >= clean_len || !isalpha((unsigned char) clean_expr[idx + 1]))) {
		return dstack_push(values, M_E, current_err);
	} else if (strncmp(clean_expr + idx, "sqrt", 4) == 0) {
		*next_idx = idx + 3;
		return cstack_push(ops, 's', current_err);
	} else if (curr_char == '(' || curr_char == ')') {
		return handle_parentheses(curr_char, values, ops, current_err);
	} else {
		return handle_operator_token(curr_char, clean_expr, idx, values, ops,
		 current_err);
	}
	return 1;
}

double engine_eval(const char* expression, char** error_msg)
{
	DoubleStack values = {{0.0}, -1};
	CharStack ops = {{'\0'}, -1};
	const char* current_err = NULL;
	char* clean_expr = NULL;
	int clean_len = 0;
	const char* ptr;
	int idx;

	if (!expression) {
		return 0.0;
	}
	clean_expr = calloc(1, strlen(expression) + 1);
	if (!clean_expr) {
		return 0.0;
	}
	for (ptr = expression; *ptr; ptr++) {
		if (!isspace((unsigned char) *ptr)) {
			clean_expr[clean_len++] = *ptr;
		}
	}
	clean_expr[clean_len] = '\0';
	for (idx = 0; idx < clean_len; idx++) {
		int next_idx = idx;
		if (!handle_token(clean_expr, clean_len, idx, &values, &ops,
		     &current_err, &next_idx)) {
			goto error;
		}
		idx = next_idx;
	}
	while (ops.top >= 0) {
		if (cstack_peek(&ops) == '(') {
			current_err = "Mismatched parentheses";
			goto error;
		}
		if (!process_top_op(&values, &ops, &current_err)) {
			goto error;
		}
	}
	{
		double final_val = dstack_pop(&values);
		free(clean_expr);
		return final_val;
	}
error:
	if (error_msg) {
		*error_msg = g_strdup(current_err);
	}
	free(clean_expr);
	return 0.0;
}

void engine_format_output(char* buf, size_t len, double val)
{
	snprintf(buf, len, "%.*f", CALC_PRECISION, val);
	if (strchr(buf, '.')) {
		char* ptr_ptr = buf + strlen(buf) - 1;
		while (ptr_ptr > buf && *ptr_ptr == '0') {
			*ptr_ptr-- = '\0';
		}
		if (ptr_ptr > buf && *ptr_ptr == '.') {
			*ptr_ptr = '\0';
		}
	}
}

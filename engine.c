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

static inline int dstack_push(DoubleStack* s, double v, const char** err)
{
	if (s->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = "Value stack overflow";
		}
		return 0;
	}
	s->data[++(s->top)] = v;
	return 1;
}

static inline double dstack_pop(DoubleStack* s)
{
	return (s->top >= 0) ? s->data[(s->top)--] : 0.0;
}

static inline int cstack_push(CharStack* s, char v, const char** err)
{
	if (s->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = "Operator stack overflow";
		}
		return 0;
	}
	s->data[++(s->top)] = v;
	return 1;
}

static inline char cstack_pop(CharStack* s)
{
	return (s->top >= 0) ? s->data[(s->top)--] : '\0';
}

static inline char cstack_peek(CharStack* s)
{
	return (s->top >= 0) ? s->data[s->top] : '\0';
}

static inline int precedence(char op)
{
	switch (op) {
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

static inline double apply_op(double a, double b, char op, const char** err)
{
	switch (op) {
	case '+':
		return a + b;
	case '-':
		return a - b;
	case '*':
		return a * b;
	case '/':
		if (fabs(b) < CALC_EPSILON) {
			if (err && !*err) {
				*err = "Division by zero";
			}
			return 0.0;
		}
		return a / b;
	case '%':
		if (fabs(b) < CALC_EPSILON) {
			if (err && !*err) {
				*err = "Modulo by zero";
			}
			return 0.0;
		}
		return fmod(a, b);
	case '^':
		if (fabs(a) < CALC_EPSILON && b < 0.0) {
			if (err && !*err) {
				*err = "Zero to a negative power is undefined";
			}
			return 0.0;
		}
		if (a < 0.0 && fabs(floor(b) - b) > CALC_EPSILON) {
			if (err && !*err) {
				*err = "Negative base with fractional exponent";
			}
			return 0.0;
		}
		return pow(a, b);
	case 's':
		if (b < 0.0) {
			if (err && !*err) {
				*err = "Square root of negative number";
			}
			return 0.0;
		}
		return sqrt(b);
	}
	return 0.0;
}

double engine_eval(const char* expression, char** error_msg)
{
	DoubleStack values = {{0}, -1};
	CharStack ops = {{0}, -1};
	const char* err = NULL;
	char* clean_expr = malloc(strlen(expression) + 1);
	int clean_len = 0;
	for (const char* p = expression; *p; p++) {
		if (!isspace((unsigned char) *p)) {
			clean_expr[clean_len++] = *p;
		}
	}
	clean_expr[clean_len] = '\0';

	for (int i = 0; i < clean_len; i++) {
		char c = clean_expr[i];

		if (isdigit((unsigned char) c) || c == '.') {
			char* end;
			double val = g_ascii_strtod(clean_expr + i, &end);
			if (!dstack_push(&values, val, &err)) {
				goto error;
			}
			i = (int) (end - clean_expr) - 1;
		} else if (c == 'P' && i + 1 < clean_len && clean_expr[i + 1] == 'I') {
			if (!dstack_push(&values, M_PI, &err)) {
				goto error;
			}
			i += 1;
		} else if (c == 'E' &&
		 (i + 1 >= clean_len || !isalpha((unsigned char) clean_expr[i + 1]))) {
			if (!dstack_push(&values, M_E, &err)) {
				goto error;
			}
		} else if (strncmp(clean_expr + i, "sqrt", 4) == 0) {
			if (!cstack_push(&ops, 's', &err)) {
				goto error;
			}
			i += 3;
		} else if (c == '(') {
			if (!cstack_push(&ops, '(', &err)) {
				goto error;
			}
		} else if (c == ')') {
			while (ops.top >= 0 && cstack_peek(&ops) != '(') {
				char op = cstack_pop(&ops);
				double v2 = dstack_pop(&values);
				if (op == 's') {
					if (!dstack_push(&values, apply_op(0, v2, op, &err),
					     &err)) {
						goto error;
					}
				} else {
					double v1 = dstack_pop(&values);
					if (!dstack_push(&values, apply_op(v1, v2, op, &err),
					     &err)) {
						goto error;
					}
				}
				if (err) {
					goto error;
				}
			}
			if (ops.top < 0) {
				err = "Mismatched parentheses";
				goto error;
			}
			cstack_pop(&ops);
			if (ops.top >= 0 && cstack_peek(&ops) == 's') {
				char op = cstack_pop(&ops);
				double v = dstack_pop(&values);
				if (!dstack_push(&values, apply_op(0, v, op, &err), &err)) {
					goto error;
				}
				if (err) {
					goto error;
				}
			}
		} else {
			/* Operator case */
			char current_op = c;
			int is_unary = 0;
			if (current_op == '-') {
				if (i == 0 || clean_expr[i - 1] == '(') {
					is_unary = 1;
				}
			}

			if (is_unary) {
				if (!dstack_push(&values, 0.0, &err)) {
					goto error;
				}
			}

			while (ops.top >= 0 &&
			 (current_op == '^' ?
			   precedence(cstack_peek(&ops)) > precedence(current_op) :
			   precedence(cstack_peek(&ops)) >= precedence(current_op))) {
				char op = cstack_pop(&ops);
				double v2 = dstack_pop(&values);
				if (op == 's') {
					if (!dstack_push(&values, apply_op(0, v2, op, &err),
					     &err)) {
						goto error;
					}
				} else {
					double v1 = dstack_pop(&values);
					if (!dstack_push(&values, apply_op(v1, v2, op, &err),
					     &err)) {
						goto error;
					}
				}
				if (err) {
					goto error;
				}
			}
			if (!cstack_push(&ops, current_op, &err)) {
				goto error;
			}
		}
	}

	while (ops.top >= 0) {
		char op = cstack_pop(&ops);
		if (op == '(') {
			err = "Mismatched parentheses";
			goto error;
		}
		double v2 = dstack_pop(&values);
		if (op == 's') {
			if (!dstack_push(&values, apply_op(0, v2, op, &err), &err)) {
				goto error;
			}
		} else {
			double v1 = dstack_pop(&values);
			if (!dstack_push(&values, apply_op(v1, v2, op, &err), &err)) {
				goto error;
			}
		}
		if (err) {
			goto error;
		}
	}

	double final_val = dstack_pop(&values);
	free(clean_expr);
	return final_val;

error:
	if (error_msg) {
		*error_msg = g_strdup(err);
	}
	free(clean_expr);
	return 0.0;
}

void engine_format_output(char* buf, size_t len, double val)
{
	snprintf(buf, len, "%.*f", CALC_PRECISION, val);
	if (strchr(buf, '.')) {
		char* p = buf + strlen(buf) - 1;
		while (p > buf && *p == '0') {
			*p-- = '\0';
		}
		if (p > buf && *p == '.') {
			*p = '\0';
		}
	}
}

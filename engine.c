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

static int dstack_push(DoubleStack* s, double v, char** err)
{
	if (s->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = g_strdup("Value stack overflow");
		}
		return 0;
	}
	s->data[++(s->top)] = v;
	return 1;
}

static double dstack_pop(DoubleStack* s)
{
	return (s->top >= 0) ? s->data[(s->top)--] : 0.0;
}

static int cstack_push(CharStack* s, char v, char** err)
{
	if (s->top >= MAX_STACK - 1) {
		if (err && !*err) {
			*err = g_strdup("Operator stack overflow");
		}
		return 0;
	}
	s->data[++(s->top)] = v;
	return 1;
}

static char cstack_pop(CharStack* s)
{
	return (s->top >= 0) ? s->data[(s->top)--] : '\0';
}

static char cstack_peek(CharStack* s)
{
	return (s->top >= 0) ? s->data[s->top] : '\0';
}

static int precedence(char op)
{
	if (op == '+' || op == '-') {
		return 1;
	}
	if (op == '*' || op == '/' || op == '%') {
		return 2;
	}
	if (op == '^') {
		return 3;
	}
	if (op == 's') {
		return 4; /* sqrt */
	}
	return 0;
}

static double apply_op(double a, double b, char op, char** err)
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
				*err = g_strdup("Division by zero");
			}
			return 0.0;
		}
		return a / b;
	case '%':
		if (fabs(b) < CALC_EPSILON) {
			if (err && !*err) {
				*err = g_strdup("Modulo by zero");
			}
			return 0.0;
		}
		return fmod(a, b);
	case '^':
		if (fabs(a) < CALC_EPSILON && b < 0.0) {
			if (err && !*err) {
				*err = g_strdup("Zero to a negative power is undefined");
			}
			return 0.0;
		}
		/* Use floor(b) != b instead of fmod(b,1.0) != 0.0 to avoid the
		 * float equality comparison that -Wfloat-equal correctly rejects. */
		if (a < 0.0 && fabs(floor(b) - b) > CALC_EPSILON) {
			if (err && !*err) {
				*err = g_strdup("Negative base with fractional exponent");
			}
			return 0.0;
		}
		return pow(a, b);
	case 's':
		/* FIX: Guard against sqrt of a negative number, which would
		 * silently produce NaN and propagate through further operations. */
		if (b < 0.0) {
			if (err && !*err) {
				*err = g_strdup("Square root of negative number");
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
	int i;
	int len = (int) strlen(expression);

	if (error_msg) {
		*error_msg = NULL;
	}

	for (i = 0; i < len; i++) {
		if (isspace(expression[i])) {
			continue;
		}

		if (isdigit(expression[i]) || expression[i] == '.') {
			char* end;
			double val = g_ascii_strtod(expression + i, &end);
			if (!dstack_push(&values, val, error_msg)) {
				return 0.0;
			}
			i = (int) (end - expression) - 1;
		} else if (strncmp(expression + i, "PI", 2) == 0) {
			if (!dstack_push(&values, M_PI, error_msg)) {
				return 0.0;
			}
			i += 1;
		} else if (strncmp(expression + i, "E", 1) == 0
		 /* FIX: The original read expression[i+1] unconditionally,
		  * which is UB (and caught by ASan) when 'E' is the last
		  * character in the string. Bound the check to len first. */
		 && (i + 1 >= len || !isalpha((unsigned char) expression[i + 1]))) {
			if (!dstack_push(&values, M_E, error_msg)) {
				return 0.0;
			}
		} else if (strncmp(expression + i, "sqrt", 4) == 0) {
			if (!cstack_push(&ops, 's', error_msg)) {
				return 0.0;
			}
			i += 3;
		} else if (expression[i] == '(') {
			if (!cstack_push(&ops, '(', error_msg)) {
				return 0.0;
			}
		} else if (expression[i] == ')') {
			while (ops.top >= 0 && cstack_peek(&ops) != '(') {
				char op = cstack_pop(&ops);
				double v2 = dstack_pop(&values);
				if (op == 's') {
					/* FIX: Propagate dstack_push failures in all operator-drain
					 * loops. Previously the return value was silently
					 * discarded, meaning a stack-full condition set *error_msg
					 * but execution continued and returned stale data instead
					 * of 0.0. */
					if (!dstack_push(&values, apply_op(0, v2, op, error_msg),
					     error_msg)) {
						return 0.0;
					}
				} else {
					double v1 = dstack_pop(&values);
					if (!dstack_push(&values, apply_op(v1, v2, op, error_msg),
					     error_msg)) {
						return 0.0;
					}
				}
				if (error_msg && *error_msg) {
					return 0.0;
				}
			}
			if (ops.top < 0) {
				if (error_msg) {
					*error_msg = g_strdup("Mismatched parentheses");
				}
				return 0.0;
			}
			cstack_pop(&ops);
			/* Check if '(' was preceded by sqrt */
			if (ops.top >= 0 && cstack_peek(&ops) == 's') {
				char op = cstack_pop(&ops);
				double v = dstack_pop(&values);
				if (!dstack_push(&values, apply_op(0, v, op, error_msg),
				     error_msg)) {
					return 0.0;
				}
				if (error_msg && *error_msg) {
					return 0.0;
				}
			}
		} else if (strchr("+-*/%^", expression[i])) {
			char current_op = expression[i];
			/* Check if it's a unary minus */
			int is_unary = 0;
			if (current_op == '-') {
				/* Skip spaces backward to find previous non-space char */
				int prev = i - 1;
				while (prev >= 0 && isspace(expression[prev])) {
					prev--;
				}
				if (prev < 0 || strchr("+-*/%^(", expression[prev])) {
					is_unary = 1;
				}
			}

			if (is_unary) {
				if (!dstack_push(&values, 0.0, error_msg)) {
					return 0.0;
				}
			}

			while (ops.top >= 0 &&
			 precedence(cstack_peek(&ops)) >= precedence(current_op)) {
				char op = cstack_pop(&ops);
				double v2 = dstack_pop(&values);
				if (op == 's') {
					/* FIX: Same push-return-value check as above. */
					if (!dstack_push(&values, apply_op(0, v2, op, error_msg),
					     error_msg)) {
						return 0.0;
					}
				} else {
					double v1 = dstack_pop(&values);
					if (!dstack_push(&values, apply_op(v1, v2, op, error_msg),
					     error_msg)) {
						return 0.0;
					}
				}
				if (error_msg && *error_msg) {
					return 0.0;
				}
			}
			if (!cstack_push(&ops, current_op, error_msg)) {
				return 0.0;
			}
		}
	}

	while (ops.top >= 0) {
		char op = cstack_pop(&ops);
		if (op == '(') {
			if (error_msg) {
				*error_msg = g_strdup("Mismatched parentheses");
			}
			return 0.0;
		}
		double v2 = dstack_pop(&values);
		if (op == 's') {
			/* FIX: Same push-return-value check in the final drain loop. */
			if (!dstack_push(&values, apply_op(0, v2, op, error_msg),
			     error_msg)) {
				return 0.0;
			}
		} else {
			double v1 = dstack_pop(&values);
			if (!dstack_push(&values, apply_op(v1, v2, op, error_msg),
			     error_msg)) {
				return 0.0;
			}
		}
		if (error_msg && *error_msg) {
			return 0.0;
		}
	}

	return dstack_pop(&values);
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

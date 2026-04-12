#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

typedef enum {
	ACT_DIGIT,
	ACT_OPERATOR,
	ACT_PAREN_OPEN,
	ACT_PAREN_CLOSE,
	ACT_CLEAR,
	ACT_BACKSPACE,
	ACT_EQUALS,
	ACT_NEGATE,
	ACT_DECIMAL
} ActionType;

void ui_show(GtkApplication* app);

#endif /* UI_H */

#include "ui.h"

#include <stdlib.h>
#include <string.h>

#include "engine.h"

/* FIX: Cap history_buf growth to prevent unbounded memory consumption when a
 * user chains many operators without ever clearing. Expressions realistically
 * never need to approach this length. */
#define HISTORY_BUF_MAX 4096

typedef struct {
	GtkWidget* window;
	GtkWidget* grid;
	GtkWidget* entry_history;
	GtkWidget* entry_input;
	GtkWidget* history_combo;
	GString* history_buf;
	GString* input_buf;
	gulong history_handler_id;
	int clear_on_next;
	int scientific_mode;
} UIData;

static void ui_update_display(UIData* ui)
{
	gtk_entry_set_text(GTK_ENTRY(ui->entry_history), ui->history_buf->str);
	if (ui->input_buf->len == 0) {
		gtk_entry_set_text(GTK_ENTRY(ui->entry_input), "0");
	} else {
		gtk_entry_set_text(GTK_ENTRY(ui->entry_input), ui->input_buf->str);
	}
}

/* FIX: Central guard called before every append to history_buf. If the buffer
 * has already exceeded the cap, reset both buffers and display an error rather
 * than growing indefinitely. Returns 1 if safe to continue, 0 if capped. */
static int history_buf_guard(UIData* ui)
{
	if (ui->history_buf->len >= HISTORY_BUF_MAX) {
		g_string_assign(ui->history_buf, "");
		g_string_assign(ui->input_buf, "Error: expression too long");
		ui->clear_on_next = 1;
		ui_update_display(ui);
		return 0;
	}
	return 1;
}

static void handle_digit(UIData* ui, const char* value)
{
	if (ui->clear_on_next) {
		g_string_assign(ui->input_buf, "");
		ui->clear_on_next = 0;
	}
	if (strcmp(ui->input_buf->str, "0") == 0) {
		g_string_assign(ui->input_buf, value);
	} else {
		g_string_append(ui->input_buf, value);
	}
}

static void handle_decimal(UIData* ui)
{
	if (ui->clear_on_next) {
		g_string_assign(ui->input_buf, "0.");
		ui->clear_on_next = 0;
	} else if (!strchr(ui->input_buf->str, '.')) {
		if (ui->input_buf->len == 0) {
			g_string_assign(ui->input_buf, "0");
		}
		g_string_append(ui->input_buf, ".");
	}
}

static void handle_operator(UIData* ui, const char* op)
{
	if (!history_buf_guard(ui)) {
		return;
	}
	if (ui->input_buf->len > 0) {
		g_string_append_printf(ui->history_buf, " %s %s", ui->input_buf->str,
		 op);
		g_string_assign(ui->input_buf, "");
	} else if (ui->history_buf->len > 0) {
		g_string_truncate(ui->history_buf, ui->history_buf->len - 2);
		g_string_append_printf(ui->history_buf, "%s ", op);
	}
	ui->clear_on_next = 0;
}

static void handle_equals(UIData* ui)
{
	char* err = NULL;
	double res;
	if (ui->input_buf->len > 0) {
		g_string_append_printf(ui->history_buf, " %s", ui->input_buf->str);
	}

	res = engine_eval(ui->history_buf->str, &err);
	if (err) {
		g_string_assign(ui->input_buf, err);
		g_free(err);
	} else {
		char res_str[CALC_BUF_SIZE];
		engine_format_output(res_str, sizeof(res_str), res);
		char* history_item =
		 g_strdup_printf("%s = %s", ui->history_buf->str, res_str);
		gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(ui->history_combo),
		 history_item);
		g_free(history_item);

		if (!gtk_widget_get_sensitive(ui->history_combo)) {
			gtk_widget_set_sensitive(ui->history_combo, TRUE);
		}
		g_string_assign(ui->input_buf, res_str);
	}
	g_string_assign(ui->history_buf, "");
	ui->clear_on_next = 1;
}

static void ui_handle_action(ActionType type, const char* value, UIData* ui)
{
	switch (type) {
	case ACT_DIGIT:
		handle_digit(ui, value);
		break;
	case ACT_DECIMAL:
		handle_decimal(ui);
		break;
	case ACT_OPERATOR:
		handle_operator(ui, value);
		break;
	case ACT_PAREN_OPEN:
		if (!history_buf_guard(ui)) {
			return;
		}
		g_string_append(ui->history_buf, " ( ");
		break;
	case ACT_PAREN_CLOSE:
		if (!history_buf_guard(ui)) {
			return;
		}
		if (ui->input_buf->len > 0) {
			g_string_append_printf(ui->history_buf, " %s ) ",
			 ui->input_buf->str);
			g_string_assign(ui->input_buf, "");
		} else {
			g_string_append(ui->history_buf, " ) ");
		}
		break;
	case ACT_CLEAR:
		g_string_assign(ui->history_buf, "");
		g_string_assign(ui->input_buf, "0");
		ui->clear_on_next = 0;
		break;
	case ACT_BACKSPACE:
		if (ui->input_buf->len > 0) {
			g_string_truncate(ui->input_buf, ui->input_buf->len - 1);
		}
		break;
	case ACT_NEGATE: {
		double v = g_ascii_strtod(ui->input_buf->str, NULL);
		if (v > CALC_EPSILON || v < -CALC_EPSILON) {
			char buf[CALC_BUF_SIZE];
			engine_format_output(buf, sizeof(buf), -v);
			g_string_assign(ui->input_buf, buf);
		}
		break;
	}
	case ACT_EQUALS:
		handle_equals(ui);
		break;
	}
	ui_update_display(ui);
}

static void on_button_clicked(GtkWidget* widget, gpointer data)
{
	UIData* ui = (UIData*) data;
	const char* label = gtk_button_get_label(GTK_BUTTON(widget));
	if (g_ascii_isdigit(label[0])) {
		ui_handle_action(ACT_DIGIT, label, ui);
	} else if (strcmp(label, ".") == 0) {
		ui_handle_action(ACT_DECIMAL, label, ui);
	} else if (strcmp(label, "C") == 0) {
		ui_handle_action(ACT_CLEAR, label, ui);
	} else if (strcmp(label, "<-") == 0) {
		ui_handle_action(ACT_BACKSPACE, label, ui);
	} else if (strcmp(label, "+/-") == 0) {
		ui_handle_action(ACT_NEGATE, label, ui);
	} else if (strcmp(label, "=") == 0) {
		ui_handle_action(ACT_EQUALS, label, ui);
	} else if (strcmp(label, "(") == 0) {
		ui_handle_action(ACT_PAREN_OPEN, label, ui);
	} else if (strcmp(label, ")") == 0) {
		ui_handle_action(ACT_PAREN_CLOSE, label, ui);
	} else if (strcmp(label, "sqrt") == 0) {
		if (!history_buf_guard(ui)) {
			return;
		}
		g_string_append(ui->history_buf, " sqrt ( ");
		ui_update_display(ui);
	} else {
		ui_handle_action(ACT_OPERATOR, label, ui);
	}
}

static void on_history_changed(GtkComboBoxText* combo, gpointer data)
{
	UIData* ui = (UIData*) data;
	char* text = gtk_combo_box_text_get_active_text(combo);
	if (text) {
		char* eq = strchr(text, '=');
		/* FIX: The original code assumed "= <value>" always followed '=', so
		 * "eq + 2" could read into garbage if the stored string was somehow
		 * malformed. Now we explicitly verify both the space separator and that
		 * there is at least one character of result before dereferencing. */
		if (eq && eq[1] == ' ' && eq[2] != '\0') {
			*eq = '\0';
			g_string_assign(ui->history_buf, "");
			g_string_assign(ui->input_buf, eq + 2);
			ui->clear_on_next = 1;
			ui_update_display(ui);
			g_signal_handler_block(combo, ui->history_handler_id);
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			g_signal_handler_unblock(combo, ui->history_handler_id);
		}
		g_free(text);
	}
}

static void on_mode_toggle(GtkToggleButton* btn, gpointer data)
{
	UIData* ui = (UIData*) data;
	ui->scientific_mode = gtk_toggle_button_get_active(btn);
	GList* children = gtk_container_get_children(GTK_CONTAINER(ui->grid));
	for (GList* l = children; l != NULL; l = l->next) {
		if (g_object_get_data(G_OBJECT(l->data), "is_sci")) {
			gtk_widget_set_visible(GTK_WIDGET(l->data), ui->scientific_mode);
		}
	}
	g_list_free(children);
}

static void setup_theme(GtkWidget* window)
{
	GtkCssProvider* provider = gtk_css_provider_new();
	GdkScreen* screen = gdk_screen_get_default();
	GtkSettings* settings = gtk_settings_get_default();
	gboolean prefer_dark = FALSE;
	char* theme_name = NULL;
	g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark,
	 "gtk-theme-name", &theme_name, NULL);
	gtk_css_provider_load_from_resource(provider, "/org/gtk/calc/style.css");
	gtk_style_context_add_provider_for_screen(screen,
	 GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	if ((theme_name && strstr(theme_name, "-dark")) || prefer_dark) {
		gtk_style_context_add_class(gtk_widget_get_style_context(window),
		 "dark");
	}
	g_object_unref(provider);
	g_free(theme_name);
}

static void on_window_destroy(GtkWidget* widget, gpointer data)
{
	UIData* ui = (UIData*) data;
	(void) widget;
	g_string_free(ui->history_buf, TRUE);
	g_string_free(ui->input_buf, TRUE);
	g_free(ui);
}

static gboolean
on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	UIData* ui = (UIData*) data;
	(void) widget;
	switch (event->keyval) {
	case GDK_KEY_0:
	case GDK_KEY_KP_0:
		ui_handle_action(ACT_DIGIT, "0", ui);
		break;
	case GDK_KEY_1:
	case GDK_KEY_KP_1:
		ui_handle_action(ACT_DIGIT, "1", ui);
		break;
	case GDK_KEY_2:
	case GDK_KEY_KP_2:
		ui_handle_action(ACT_DIGIT, "2", ui);
		break;
	case GDK_KEY_3:
	case GDK_KEY_KP_3:
		ui_handle_action(ACT_DIGIT, "3", ui);
		break;
	case GDK_KEY_4:
	case GDK_KEY_KP_4:
		ui_handle_action(ACT_DIGIT, "4", ui);
		break;
	case GDK_KEY_5:
	case GDK_KEY_KP_5:
		ui_handle_action(ACT_DIGIT, "5", ui);
		break;
	case GDK_KEY_6:
	case GDK_KEY_KP_6:
		ui_handle_action(ACT_DIGIT, "6", ui);
		break;
	case GDK_KEY_7:
	case GDK_KEY_KP_7:
		ui_handle_action(ACT_DIGIT, "7", ui);
		break;
	case GDK_KEY_8:
	case GDK_KEY_KP_8:
		ui_handle_action(ACT_DIGIT, "8", ui);
		break;
	case GDK_KEY_9:
	case GDK_KEY_KP_9:
		ui_handle_action(ACT_DIGIT, "9", ui);
		break;
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		ui_handle_action(ACT_OPERATOR, "+", ui);
		break;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		ui_handle_action(ACT_OPERATOR, "-", ui);
		break;
	case GDK_KEY_asterisk:
	case GDK_KEY_KP_Multiply:
		ui_handle_action(ACT_OPERATOR, "*", ui);
		break;
	case GDK_KEY_slash:
	case GDK_KEY_KP_Divide:
		ui_handle_action(ACT_OPERATOR, "/", ui);
		break;
	case GDK_KEY_percent:
		ui_handle_action(ACT_OPERATOR, "%", ui);
		break;
	case GDK_KEY_asciicircum:
		ui_handle_action(ACT_OPERATOR, "^", ui);
		break;
	case GDK_KEY_period:
	case GDK_KEY_comma:
	case GDK_KEY_KP_Decimal:
		ui_handle_action(ACT_DECIMAL, ".", ui);
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_equal:
		ui_handle_action(ACT_EQUALS, "=", ui);
		break;
	case GDK_KEY_Escape:
		ui_handle_action(ACT_CLEAR, "C", ui);
		break;
	case GDK_KEY_BackSpace:
		ui_handle_action(ACT_BACKSPACE, "<-", ui);
		break;
	case GDK_KEY_parenleft:
		ui_handle_action(ACT_PAREN_OPEN, "(", ui);
		break;
	case GDK_KEY_parenright:
		ui_handle_action(ACT_PAREN_CLOSE, ")", ui);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void ui_show(GtkApplication* app)
{
	UIData* ui = g_malloc0(sizeof(UIData));
	ui->history_buf = g_string_new("");
	ui->input_buf = g_string_new("0");
	ui->window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(ui->window), "Calc");
	gtk_window_set_default_size(GTK_WINDOW(ui->window), 350, 400);
	setup_theme(ui->window);
	g_signal_connect(ui->window, "destroy", G_CALLBACK(on_window_destroy), ui);

	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(ui->window), vbox);
	GtkWidget* hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, 5);
	GtkWidget* mode_btn = gtk_toggle_button_new_with_label("Sci");
	g_signal_connect(mode_btn, "toggled", G_CALLBACK(on_mode_toggle), ui);
	gtk_box_pack_start(GTK_BOX(hbox_top), mode_btn, FALSE, FALSE, 5);
	ui->history_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui->history_combo),
	 "History");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ui->history_combo), 0);
	gtk_widget_set_sensitive(ui->history_combo, FALSE);
	ui->history_handler_id = g_signal_connect(ui->history_combo, "changed",
	 G_CALLBACK(on_history_changed), ui);
	gtk_box_pack_start(GTK_BOX(hbox_top), ui->history_combo, TRUE, TRUE, 5);

	ui->entry_history = gtk_entry_new();
	gtk_entry_set_alignment(GTK_ENTRY(ui->entry_history), 1.0);
	gtk_editable_set_editable(GTK_EDITABLE(ui->entry_history), FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(ui->entry_history),
	 "history");
	gtk_box_pack_start(GTK_BOX(vbox), ui->entry_history, FALSE, FALSE, 0);
	ui->entry_input = gtk_entry_new();
	gtk_entry_set_alignment(GTK_ENTRY(ui->entry_input), 1.0);
	gtk_editable_set_editable(GTK_EDITABLE(ui->entry_input), FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(ui->entry_input),
	 "input");
	gtk_box_pack_start(GTK_BOX(vbox), ui->entry_input, FALSE, FALSE, 5);

	ui->grid = gtk_grid_new();
	gtk_grid_set_column_homogeneous(GTK_GRID(ui->grid), TRUE);
	gtk_grid_set_row_homogeneous(GTK_GRID(ui->grid), TRUE);
	gtk_grid_set_column_spacing(GTK_GRID(ui->grid), 5);
	gtk_grid_set_row_spacing(GTK_GRID(ui->grid), 5);
	gtk_box_pack_start(GTK_BOX(vbox), ui->grid, TRUE, TRUE, 0);

	const char* buttons[] = {"C", "<-", "(", ")", "7", "8", "9", "/", "4", "5",
	    "6", "*", "1", "2", "3", "-", ".", "0", "=", "+"};
	for (int i = 0; i < 20; i++) {
		GtkWidget* btn = gtk_button_new_with_label(buttons[i]);
		GtkStyleContext* ctx = gtk_widget_get_style_context(btn);
		if (strcmp(buttons[i], "=") == 0) {
			gtk_style_context_add_class(ctx, "equal");
		} else if (strcmp(buttons[i], "C") == 0 ||
		 strcmp(buttons[i], "<-") == 0) {
			gtk_style_context_add_class(ctx, "clear");
		} else if (strchr("/ *-+", buttons[i][0]) && strlen(buttons[i]) == 1) {
			gtk_style_context_add_class(ctx, "operator");
		}
		g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), ui);
		gtk_grid_attach(GTK_GRID(ui->grid), btn, i % 4, i / 4, 1, 1);
	}

	const char* sci_buttons[] = {"^", "%", "sqrt", "PI", "E"};
	for (int i = 0; i < 5; i++) {
		GtkWidget* btn = gtk_button_new_with_label(sci_buttons[i]);
		gtk_style_context_add_class(gtk_widget_get_style_context(btn),
		 "scientific");
		g_object_set_data(G_OBJECT(btn), "is_sci", GINT_TO_POINTER(1));
		gtk_widget_set_no_show_all(btn, TRUE);
		gtk_widget_set_visible(btn, FALSE);
		g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), ui);
		gtk_grid_attach(GTK_GRID(ui->grid), btn, 4, i, 1, 1);
	}

	g_signal_connect(ui->window, "key-press-event", G_CALLBACK(on_key_press),
	 ui);
	gtk_widget_show_all(ui->window);
	ui_update_display(ui);
}

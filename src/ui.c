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

#define UI_WINDOW_WIDTH 350
#define UI_WINDOW_HEIGHT 400
#define UI_BOX_SPACING 5
#define UI_GRID_SPACING 5
#define UI_BUTTON_COUNT 20
#define UI_SCI_BUTTON_COUNT 5

static void ui_update_display(UIData* ui_data)
{
	gtk_entry_set_text(GTK_ENTRY(ui_data->entry_history),
	 ui_data->history_buf->str);
	if (ui_data->input_buf->len == 0) {
		gtk_entry_set_text(GTK_ENTRY(ui_data->entry_input), "0");
	} else {
		gtk_entry_set_text(GTK_ENTRY(ui_data->entry_input),
		 ui_data->input_buf->str);
	}
}

/* FIX: Central guard called before every append to history_buf. If the buffer
 * has already exceeded the cap, reset both buffers and display an error rather
 * than growing indefinitely. Returns 1 if safe to continue, 0 if capped. */
static int history_buf_guard(UIData* ui_data)
{
	if (ui_data->history_buf->len >= HISTORY_BUF_MAX) {
		g_string_assign(ui_data->history_buf, "");
		g_string_assign(ui_data->input_buf, "Error: expression too long");
		ui_data->clear_on_next = 1;
		ui_update_display(ui_data);
		return 0;
	}
	return 1;
}

static void handle_digit(UIData* ui_data, const char* digit_value)
{
	if (ui_data->clear_on_next) {
		g_string_assign(ui_data->input_buf, "");
		ui_data->clear_on_next = 0;
	}
	if (strcmp(ui_data->input_buf->str, "0") == 0) {
		g_string_assign(ui_data->input_buf, digit_value);
	} else {
		g_string_append(ui_data->input_buf, digit_value);
	}
}

static void handle_decimal(UIData* ui_data)
{
	if (ui_data->clear_on_next) {
		g_string_assign(ui_data->input_buf, "0.");
		ui_data->clear_on_next = 0;
	} else if (!strchr(ui_data->input_buf->str, '.')) {
		if (ui_data->input_buf->len == 0) {
			g_string_assign(ui_data->input_buf, "0");
		}
		g_string_append(ui_data->input_buf, ".");
	}
}

static void handle_operator(UIData* ui_data, const char* operator_str)
{
	if (!history_buf_guard(ui_data)) {
		return;
	}
	if (ui_data->input_buf->len > 0) {
		g_string_append_printf(ui_data->history_buf, " %s %s",
		 ui_data->input_buf->str, operator_str);
		g_string_assign(ui_data->input_buf, "");
	} else if (ui_data->history_buf->len > 0) {
		g_string_truncate(ui_data->history_buf, ui_data->history_buf->len - 2);
		g_string_append_printf(ui_data->history_buf, "%s ", operator_str);
	}
	ui_data->clear_on_next = 0;
}

static void handle_equals(UIData* ui_data)
{
	char* error_msg = NULL;
	double result_val;
	if (ui_data->input_buf->len > 0) {
		g_string_append_printf(ui_data->history_buf, " %s",
		 ui_data->input_buf->str);
	}

	result_val = engine_eval(ui_data->history_buf->str, &error_msg);
	if (error_msg) {
		g_string_assign(ui_data->input_buf, error_msg);
		g_free(error_msg);
	} else {
		char res_str[CALC_BUF_SIZE];
		engine_format_output(res_str, sizeof(res_str), result_val);
		char* history_item =
		 g_strdup_printf("%s = %s", ui_data->history_buf->str, res_str);
		gtk_combo_box_text_prepend_text(
		 GTK_COMBO_BOX_TEXT(ui_data->history_combo), history_item);
		g_free(history_item);

		if (!gtk_widget_get_sensitive(ui_data->history_combo)) {
			gtk_widget_set_sensitive(ui_data->history_combo, TRUE);
		}
		g_string_assign(ui_data->input_buf, res_str);
	}
	g_string_assign(ui_data->history_buf, "");
	ui_data->clear_on_next = 1;
}

static void ui_handle_action(ActionType action_type, const char* action_value,
 UIData* ui_data)
{
	switch (action_type) {
	case ACT_DIGIT:
		handle_digit(ui_data, action_value);
		break;
	case ACT_DECIMAL:
		handle_decimal(ui_data);
		break;
	case ACT_OPERATOR:
		handle_operator(ui_data, action_value);
		break;
	case ACT_PAREN_OPEN:
		if (!history_buf_guard(ui_data)) {
			return;
		}
		g_string_append(ui_data->history_buf, " ( ");
		break;
	case ACT_PAREN_CLOSE:
		if (!history_buf_guard(ui_data)) {
			return;
		}
		if (ui_data->input_buf->len > 0) {
			g_string_append_printf(ui_data->history_buf, " %s ) ",
			 ui_data->input_buf->str);
			g_string_assign(ui_data->input_buf, "");
		} else {
			g_string_append(ui_data->history_buf, " ) ");
		}
		break;
	case ACT_CLEAR:
		g_string_assign(ui_data->history_buf, "");
		g_string_assign(ui_data->input_buf, "0");
		ui_data->clear_on_next = 0;
		break;
	case ACT_BACKSPACE:
		if (ui_data->input_buf->len > 0) {
			g_string_truncate(ui_data->input_buf, ui_data->input_buf->len - 1);
		}
		break;
	case ACT_NEGATE: {
		double value_to_negate = g_ascii_strtod(ui_data->input_buf->str, NULL);
		if (value_to_negate > CALC_EPSILON || value_to_negate < -CALC_EPSILON) {
			char buf[CALC_BUF_SIZE];
			engine_format_output(buf, sizeof(buf), -value_to_negate);
			g_string_assign(ui_data->input_buf, buf);
		}
		break;
	}
	case ACT_EQUALS:
		handle_equals(ui_data);
		break;
	}
	ui_update_display(ui_data);
}

static void on_button_clicked(GtkWidget* widget, gpointer data)
{
	UIData* ui_data = (UIData*) data;
	const char* label = gtk_button_get_label(GTK_BUTTON(widget));
	if (g_ascii_isdigit(label[0])) {
		ui_handle_action(ACT_DIGIT, label, ui_data);
	} else if (strcmp(label, ".") == 0) {
		ui_handle_action(ACT_DECIMAL, label, ui_data);
	} else if (strcmp(label, "C") == 0) {
		ui_handle_action(ACT_CLEAR, label, ui_data);
	} else if (strcmp(label, "<-") == 0) {
		ui_handle_action(ACT_BACKSPACE, label, ui_data);
	} else if (strcmp(label, "+/-") == 0) {
		ui_handle_action(ACT_NEGATE, label, ui_data);
	} else if (strcmp(label, "=") == 0) {
		ui_handle_action(ACT_EQUALS, label, ui_data);
	} else if (strcmp(label, "(") == 0) {
		ui_handle_action(ACT_PAREN_OPEN, label, ui_data);
	} else if (strcmp(label, ")") == 0) {
		ui_handle_action(ACT_PAREN_CLOSE, label, ui_data);
	} else if (strcmp(label, "sqrt") == 0) {
		if (!history_buf_guard(ui_data)) {
			return;
		}
		g_string_append(ui_data->history_buf, " sqrt ( ");
		ui_update_display(ui_data);
	} else {
		ui_handle_action(ACT_OPERATOR, label, ui_data);
	}
}

static void on_history_changed(GtkComboBoxText* combo, gpointer data)
{
	UIData* ui_data = (UIData*) data;
	char* history_text = gtk_combo_box_text_get_active_text(combo);
	if (history_text) {
		char* equals_ptr = strchr(history_text, '=');
		/* FIX: The original code assumed "= <value>" always followed '=', so
		 * "equals_ptr + 2" could read into garbage if the stored string was
		 * somehow malformed. Now we explicitly verify both the space separator
		 * and that there is at least one character of result before
		 * dereferencing. */
		if (equals_ptr && equals_ptr[1] == ' ' && equals_ptr[2] != '\0') {
			*equals_ptr = '\0';
			g_string_assign(ui_data->history_buf, "");
			g_string_assign(ui_data->input_buf, equals_ptr + 2);
			ui_data->clear_on_next = 1;
			ui_update_display(ui_data);
			g_signal_handler_block(combo, ui_data->history_handler_id);
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			g_signal_handler_unblock(combo, ui_data->history_handler_id);
		}
		g_free(history_text);
	}
}

static void on_mode_toggle(GtkToggleButton* btn, gpointer data)
{
	UIData* ui_data = (UIData*) data;
	ui_data->scientific_mode = gtk_toggle_button_get_active(btn);
	GList* children = gtk_container_get_children(GTK_CONTAINER(ui_data->grid));
	GList* child_node;
	for (child_node = children; child_node != NULL;
	 child_node = child_node->next) {
		if (g_object_get_data(G_OBJECT(child_node->data), "is_sci")) {
			gtk_widget_set_visible(GTK_WIDGET(child_node->data),
			 ui_data->scientific_mode);
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
	gtk_css_provider_load_from_resource(provider, "/org/mini/calc/style.css");
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
	UIData* ui_data = (UIData*) data;
	(void) widget;
	g_string_free(ui_data->history_buf, TRUE);
	g_string_free(ui_data->input_buf, TRUE);
	g_free(ui_data);
}

static gboolean
on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	UIData* ui_data = (UIData*) data;
	(void) widget;
	switch (event->keyval) {
	case GDK_KEY_0:
	case GDK_KEY_KP_0:
		ui_handle_action(ACT_DIGIT, "0", ui_data);
		break;
	case GDK_KEY_1:
	case GDK_KEY_KP_1:
		ui_handle_action(ACT_DIGIT, "1", ui_data);
		break;
	case GDK_KEY_2:
	case GDK_KEY_KP_2:
		ui_handle_action(ACT_DIGIT, "2", ui_data);
		break;
	case GDK_KEY_3:
	case GDK_KEY_KP_3:
		ui_handle_action(ACT_DIGIT, "3", ui_data);
		break;
	case GDK_KEY_4:
	case GDK_KEY_KP_4:
		ui_handle_action(ACT_DIGIT, "4", ui_data);
		break;
	case GDK_KEY_5:
	case GDK_KEY_KP_5:
		ui_handle_action(ACT_DIGIT, "5", ui_data);
		break;
	case GDK_KEY_6:
	case GDK_KEY_KP_6:
		ui_handle_action(ACT_DIGIT, "6", ui_data);
		break;
	case GDK_KEY_7:
	case GDK_KEY_KP_7:
		ui_handle_action(ACT_DIGIT, "7", ui_data);
		break;
	case GDK_KEY_8:
	case GDK_KEY_KP_8:
		ui_handle_action(ACT_DIGIT, "8", ui_data);
		break;
	case GDK_KEY_9:
	case GDK_KEY_KP_9:
		ui_handle_action(ACT_DIGIT, "9", ui_data);
		break;
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		ui_handle_action(ACT_OPERATOR, "+", ui_data);
		break;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		ui_handle_action(ACT_OPERATOR, "-", ui_data);
		break;
	case GDK_KEY_asterisk:
	case GDK_KEY_KP_Multiply:
		ui_handle_action(ACT_OPERATOR, "*", ui_data);
		break;
	case GDK_KEY_slash:
	case GDK_KEY_KP_Divide:
		ui_handle_action(ACT_OPERATOR, "/", ui_data);
		break;
	case GDK_KEY_percent:
		ui_handle_action(ACT_OPERATOR, "%", ui_data);
		break;
	case GDK_KEY_asciicircum:
		ui_handle_action(ACT_OPERATOR, "^", ui_data);
		break;
	case GDK_KEY_period:
	case GDK_KEY_comma:
	case GDK_KEY_KP_Decimal:
		ui_handle_action(ACT_DECIMAL, ".", ui_data);
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_equal:
		ui_handle_action(ACT_EQUALS, "=", ui_data);
		break;
	case GDK_KEY_Escape:
		ui_handle_action(ACT_CLEAR, "C", ui_data);
		break;
	case GDK_KEY_BackSpace:
		ui_handle_action(ACT_BACKSPACE, "<-", ui_data);
		break;
	case GDK_KEY_parenleft:
		ui_handle_action(ACT_PAREN_OPEN, "(", ui_data);
		break;
	case GDK_KEY_parenright:
		ui_handle_action(ACT_PAREN_CLOSE, ")", ui_data);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void ui_show(GtkApplication* app)
{
	UIData* ui_data = g_malloc0(sizeof(UIData));
	ui_data->history_buf = g_string_new("");
	ui_data->input_buf = g_string_new("0");
	ui_data->window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(ui_data->window), "Calc");
	gtk_window_set_default_size(GTK_WINDOW(ui_data->window), UI_WINDOW_WIDTH,
	 UI_WINDOW_HEIGHT);
	setup_theme(ui_data->window);
	g_signal_connect(ui_data->window, "destroy", G_CALLBACK(on_window_destroy),
	 ui_data);

	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, UI_BOX_SPACING);
	gtk_container_add(GTK_CONTAINER(ui_data->window), vbox);
	GtkWidget* hbox_top =
	 gtk_box_new(GTK_ORIENTATION_HORIZONTAL, UI_BOX_SPACING);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, UI_BOX_SPACING);
	GtkWidget* mode_btn = gtk_toggle_button_new_with_label("Sci");
	g_signal_connect(mode_btn, "toggled", G_CALLBACK(on_mode_toggle), ui_data);
	gtk_box_pack_start(GTK_BOX(hbox_top), mode_btn, FALSE, FALSE,
	 UI_BOX_SPACING);
	ui_data->history_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ui_data->history_combo),
	 "History");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ui_data->history_combo), 0);
	gtk_widget_set_sensitive(ui_data->history_combo, FALSE);
	ui_data->history_handler_id = g_signal_connect(ui_data->history_combo,
	 "changed", G_CALLBACK(on_history_changed), ui_data);
	gtk_box_pack_start(GTK_BOX(hbox_top), ui_data->history_combo, TRUE, TRUE,
	 UI_BOX_SPACING);

	ui_data->entry_history = gtk_entry_new();
	gtk_entry_set_alignment(GTK_ENTRY(ui_data->entry_history), (gfloat) 1.0);
	gtk_editable_set_editable(GTK_EDITABLE(ui_data->entry_history), FALSE);
	gtk_style_context_add_class(
	 gtk_widget_get_style_context(ui_data->entry_history), "history");
	gtk_box_pack_start(GTK_BOX(vbox), ui_data->entry_history, FALSE, FALSE, 0);
	ui_data->entry_input = gtk_entry_new();
	gtk_entry_set_alignment(GTK_ENTRY(ui_data->entry_input), (gfloat) 1.0);
	gtk_editable_set_editable(GTK_EDITABLE(ui_data->entry_input), FALSE);
	gtk_style_context_add_class(
	 gtk_widget_get_style_context(ui_data->entry_input), "input");
	gtk_box_pack_start(GTK_BOX(vbox), ui_data->entry_input, FALSE, FALSE,
	 UI_BOX_SPACING);

	ui_data->grid = gtk_grid_new();
	gtk_grid_set_column_homogeneous(GTK_GRID(ui_data->grid), TRUE);
	gtk_grid_set_row_homogeneous(GTK_GRID(ui_data->grid), TRUE);
	gtk_grid_set_column_spacing(GTK_GRID(ui_data->grid), UI_GRID_SPACING);
	gtk_grid_set_row_spacing(GTK_GRID(ui_data->grid), UI_GRID_SPACING);
	gtk_box_pack_start(GTK_BOX(vbox), ui_data->grid, TRUE, TRUE, 0);

	const char* buttons[] = {"C", "<-", "(", ")", "7", "8", "9", "/", "4", "5",
	    "6", "*", "1", "2", "3", "-", ".", "0", "=", "+"};
	{
		int button_idx;
		for (button_idx = 0; button_idx < UI_BUTTON_COUNT; button_idx++) {
			GtkWidget* btn = gtk_button_new_with_label(buttons[button_idx]);
			GtkStyleContext* ctx = gtk_widget_get_style_context(btn);
			if (strcmp(buttons[button_idx], "=") == 0) {
				gtk_style_context_add_class(ctx, "equal");
			} else if (strcmp(buttons[button_idx], "C") == 0 ||
			 strcmp(buttons[button_idx], "<-") == 0) {
				gtk_style_context_add_class(ctx, "clear");
			} else if (strchr("/ *-+", buttons[button_idx][0]) &&
			 strlen(buttons[button_idx]) == 1) {
				gtk_style_context_add_class(ctx, "operator");
			}
			g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked),
			 ui_data);
			gtk_grid_attach(GTK_GRID(ui_data->grid), btn, button_idx % 4,
			 button_idx / 4, 1, 1);
		}
	}

	const char* sci_buttons[] = {"^", "%", "sqrt", "PI", "E"};
	{
		int button_idx;
		for (button_idx = 0; button_idx < UI_SCI_BUTTON_COUNT; button_idx++) {
			GtkWidget* btn = gtk_button_new_with_label(sci_buttons[button_idx]);
			gtk_style_context_add_class(gtk_widget_get_style_context(btn),
			 "scientific");
			g_object_set_data(G_OBJECT(btn), "is_sci", GINT_TO_POINTER(1));
			gtk_widget_set_no_show_all(btn, TRUE);
			gtk_widget_set_visible(btn, FALSE);
			g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked),
			 ui_data);
			gtk_grid_attach(GTK_GRID(ui_data->grid), btn, 4, button_idx, 1, 1);
		}
	}

	g_signal_connect(ui_data->window, "key-press-event",
	 G_CALLBACK(on_key_press), ui_data);
	gtk_widget_show_all(ui_data->window);
	ui_update_display(ui_data);
}

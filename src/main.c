#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "ui.h"

static int handle_cli(int argc, char** argv)
{
	GString* expression_str;
	char* error_msg = NULL;
	double result_val;
	int arg_index;

	expression_str = g_string_new("");
	if (expression_str == NULL) {
		fprintf(stderr,
		 "Error: Failed to allocate memory for expression string\n");
		return 1;
	}

	for (arg_index = 1; arg_index < argc; arg_index++) {
		g_string_append(expression_str, argv[arg_index]);
		if (arg_index < argc - 1) {
			g_string_append_c(expression_str, ' ');
		}
	}

	result_val = engine_eval(expression_str->str, &error_msg);

	if (error_msg != NULL) {
		fprintf(stderr, "Error: %s\n", error_msg);
		g_free(error_msg);
		g_string_free(expression_str, TRUE);
		return 1;
	}

	{
		char res_str[CALC_BUF_SIZE];
		engine_format_output(res_str, sizeof(res_str), result_val);
		printf("%s\n", res_str);
	}

	g_string_free(expression_str, TRUE);
	return 0;
}

int main(int argc, char** argv)
{
	if (argc > 1) {
		return handle_cli(argc, argv);
	}

	{
		GtkApplication* app;
		int status;

		printf("Starting GUI mode...\n");
		app = gtk_application_new("org.gtk.calc", G_APPLICATION_DEFAULT_FLAGS);
		g_signal_connect(app, "activate", G_CALLBACK(ui_show), NULL);
		printf("Running GtkApplication...\n");
		status = g_application_run(G_APPLICATION(app), argc, argv);
		g_object_unref(app);

		return status;
	}
}

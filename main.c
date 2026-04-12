#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "ui.h"

static int handle_cli(int argc, char** argv)
{
	GString* expr = g_string_new("");
	char* err = NULL;
	double res;
	int i;

	for (i = 1; i < argc; i++) {
		g_string_append(expr, argv[i]);
		if (i < argc - 1) {
			g_string_append_c(expr, ' ');
		}
	}

	res = engine_eval(expr->str, &err);

	if (err) {
		fprintf(stderr, "Error: %s\n", err);
		g_free(err);
		g_string_free(expr, TRUE);
		return 1;
	}

	{
		char res_str[64];
		engine_format_output(res_str, sizeof(res_str), res);
		printf("%s\n", res_str);
	}

	g_string_free(expr, TRUE);
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

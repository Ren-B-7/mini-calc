#ifndef MINICLI_H
#define MINICLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "set.h"

typedef void (*cli_callback)(int argc, char** argv, void* user_data);

typedef struct {
	const char* name;
	const char* shorthand;
	const char* description;
	cli_callback callback;
	void* user_data;
} CliArgument;

typedef struct {
	const char* name;
	const char* description;
	SimpleSet arguments;
	// We store pointers to CliArgument in our own array if needed,
	// or just use the set for validation and a separate array for callbacks.
	CliArgument* registered_args;
	size_t arg_count;
	size_t arg_capacity;
} CliParser;

typedef struct {
	const char* name;
	const char* description;
} CliInitParams;

static inline int cli_init(CliParser* parser, CliInitParams params)
{
	parser->name = params.name;
	parser->description = params.description;
	parser->arg_count = 0;
	parser->arg_capacity = 10;
	parser->registered_args =
	 (CliArgument*) malloc(sizeof(CliArgument) * parser->arg_capacity);
	return set_init(&parser->arguments);
}
static inline int cli_add_argument(CliParser* parser, CliArgument arg)
{
	if (parser->arg_count >= parser->arg_capacity) {
		parser->arg_capacity *= 2;
		CliArgument* temp = (CliArgument*) realloc(parser->registered_args,
		 sizeof(CliArgument) * parser->arg_capacity);
		if (!temp) {
			return -1;
		}
		parser->registered_args = temp;
	}
	parser->registered_args[parser->arg_count++] = arg;
	set_add_str(&parser->arguments, arg.name);
	if (arg.shorthand) {
		set_add_str(&parser->arguments, arg.shorthand);
	}
	return 0;
}

static inline void cli_parse(CliParser* parser, int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			printf("Usage: %s\n%s\n\nArguments:\n", parser->name, parser->description);
			for (size_t j = 0; j < parser->arg_count; j++) {
				printf("  %-10s %-10s %s\n", parser->registered_args[j].shorthand ? parser->registered_args[j].shorthand : "", parser->registered_args[j].name, parser->registered_args[j].description);
			}
			exit(EXIT_SUCCESS);
		}
		for (size_t j = 0; j < parser->arg_count; j++) {
			if (strcmp(argv[i], parser->registered_args[j].name) == 0 ||
			 (parser->registered_args[j].shorthand &&
			  strcmp(argv[i], parser->registered_args[j].shorthand) == 0)) {
				// Simple callback execution: pass remaining args
				parser->registered_args[j].callback(argc - i - 1, &argv[i + 1],
				 parser->registered_args[j].user_data);
				return;
			}
		}
	}
}

static inline void cli_destroy(CliParser* parser)
{
	set_destroy(&parser->arguments);
	free(parser->registered_args);
}

#endif

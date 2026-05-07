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

#define DEFAULT_ARG_CAPACITY 10

static inline int cli_init(CliParser* parser, CliInitParams params)
{
	parser->name = params.name;
	parser->description = params.description;
	parser->arg_count = 0;
	parser->arg_capacity = DEFAULT_ARG_CAPACITY;
	parser->registered_args =
	 (CliArgument*) malloc(sizeof(CliArgument) * parser->arg_capacity);
	return set_init(&parser->arguments);
}

static void cli_add_argument(CliParser* parser, CliArgument arg)
{
	if (parser->arg_count >= parser->arg_capacity) {
		parser->arg_capacity *= 2;
		CliArgument* temp = (CliArgument*) realloc(parser->registered_args,
		 sizeof(CliArgument) * parser->arg_capacity);
		if (!temp) {
			return;
		}
		parser->registered_args = temp;
	}
	parser->registered_args[parser->arg_count++] = arg;
	set_add_str(&parser->arguments, arg.name);
	if (arg.shorthand) {
		set_add_str(&parser->arguments, arg.shorthand);
	}
}

static inline void cli_print_help(const CliParser* parser)
{
	printf("Usage: %s [options]\n\n", parser->name);
	printf("%s\n\n", parser->description);
	printf("Options:\n");
	for (size_t i = 0; i < parser->arg_count; i++) {
		const CliArgument* arg = &parser->registered_args[i];
		if (arg->shorthand) {
			printf("  %-16s %-4s  %s\n", arg->name, arg->shorthand,
			 arg->description);
		} else {
			printf("  %-16s       %s\n", arg->name, arg->description);
		}
	}
	printf("  %-16s %-4s  %s\n", "--help", "-h", "Display this help");
}

static inline void cli_print_completions(const CliParser* parser, const char* shell)
{
	if (strcmp(shell, "bash") == 0) {
		printf("_%s_completions()\n", parser->name);
		printf("{\n");
		printf("    local cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
		printf("    COMPREPLY=( $(compgen -W \"");
		for (size_t i = 0; i < parser->arg_count; i++) {
			printf("%s ", parser->registered_args[i].name);
			if (parser->registered_args[i].shorthand)
				printf("%s ", parser->registered_args[i].shorthand);
		}
		printf("--help -h\" -- \"$cur\") )\n");
		printf("}\n");
		printf("complete -F _%s_completions %s\n", parser->name, parser->name);
	} else if (strcmp(shell, "zsh") == 0) {
		printf("#compdef %s\n", parser->name);
		printf("_arguments \\\n");
		for (size_t i = 0; i < parser->arg_count; i++) {
			printf("  '%s[%s]' \\\n", parser->registered_args[i].name, parser->registered_args[i].description);
			if (parser->registered_args[i].shorthand)
				printf("  '%s[%s]' \\\n", parser->registered_args[i].shorthand, parser->registered_args[i].description);
		}
		printf("  '--help[Display this help]' \\\n");
		printf("  '-h[Display this help]'\n");
	}
}

static inline void cli_parse(CliParser* parser, int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			cli_print_help(parser);
			return;
		}
		if (strcmp(argv[i], "--completions") == 0 && i + 1 < argc) {
			cli_print_completions(parser, argv[i + 1]);
			return;
		}
		for (size_t j = 0; j < parser->arg_count; j++) {
			if (strcmp(argv[i], parser->registered_args[j].name) == 0 ||
			 (parser->registered_args[j].shorthand &&
			  strcmp(argv[i], parser->registered_args[j].shorthand) == 0)) {
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

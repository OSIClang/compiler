#include "osic.h"
#include "input.h"
#include "lexer.h"
#include "scope.h"
#include "symbol.h"
#include "syntax.h"
#include "parser.h"
#include "compiler.h"
#include "machine.h"
#include "generator.h"
#include "oTable.h"

#include <string.h>
#include <stdbool.h>

int
check_paren_is_closed(char *code, int len)
{
	int i;
	int a;
	int b;
	int c;
	int ss;
	int ds;

	a = 0;
	b = 0;
	c = 0;
	ss = 0;
	ds = 0;
	for (i = 0; i < len; i++) {
		if (ss) {
			switch (code[i]) {
			case '\\':
				i += 1;
				break;

			case '\'':
				ss = 0;
				break;
			}

			continue;
		}
		if (ds) {
			switch (code[i]) {
			case '\\':
				i += 1;
				break;
			case '"':
				ds = 0;
				break;
			}
			continue;
		}
		switch (code[i]) {
		case '\'':
			ss = 1;
			break;
		case '"':
			ds = 1;
			break;
		case '(':
			a += 1;
			break;
		case ')':
			a -= 1;
			break;
		case '[':
			b += 1;
			break;
		case ']':
			b -= 1;
			break;
		case '{':
			c += 1;
			break;
		case '}':
			c -= 1;
			break;
		}
	}

	return (a == 0 && b == 0 && c == 0);
}

int
check_stmt_is_closed(char *code, int len)
{
	int i;
	int brace;
	int semicon;

	if (!len) {
		return 1;
	}

	if (!check_paren_is_closed(code, len)) {
		return 0;
	}

	brace = 0;
	semicon = 0;
	for (i = 0; i < len; i++) {
		if (semicon) {
			switch (code[i]) {
			case ' ':
			case '\n':
			case '\t':
				break;
			default:
				semicon = 0;
			}
		} else {
			switch (code[i]) {
			case '}':
				brace = 1;
				break;
			case ';':
				semicon = 1;
				break;
			default:
				break;
			}
		}
	}

	if (semicon) {
		return 1;
	}

	if (brace) {
		return 1;
	}

	return 0;
}

void
shell_store_frame(struct osic *osic,
                  struct oframe *frame,
                  struct oobject *locals[])
{
	int i;

	for (i = 0; i < frame->nlocals; i++) {
		locals[i] = oframe_get_item(osic, frame, i);
	}
}

void
shell_restore_frame(struct osic *osic,
                    struct oframe *frame,
                    int nlocals,
                    struct oobject *locals[])
{
	int i;

	for (i = 0; i < nlocals; i++) {
		oframe_set_item(osic, frame, i, locals[i]);
	}
}

struct oframe *
shell_extend_frame(struct osic *osic, struct oframe *frame, int nlocals)
{
	int i;
	struct oframe *newframe;

	newframe = frame;
	if (frame->nlocals < nlocals) {
		newframe = oframe_create(osic, NULL, NULL, NULL, nlocals);

		oobject_copy(osic,
		             (struct oobject *)frame,
		             (struct oobject *)newframe,
		             sizeof(struct oframe));

		for (i = frame->nlocals; i < nlocals; i++) {
			oframe_set_item(osic, newframe, i, osic->l_nil);
		}
	}

	return newframe;
}

void
shell(struct osic *osic)
{
	int pc;

	int stmtlen;
	int codelen;
	char stmt[4096];
	char code[40960];
	char buffer[4096];

	struct syntax *node;
	struct oframe *frame;

	int nlocals;
	struct oobject *locals[256];

	puts("osic Version 0.0.2");
	puts("Copyright 2021 by Swen Kalski");
	puts("");
	puts("Type 'help' for more information, 'exit' ");
	puts("");

	pc = 0;
	codelen = 0;
	stmtlen = 0;
	nlocals = 0;
	osic_machine_reset(osic);
	memset(code, 0, sizeof(code));
	while (1) {
		if (check_stmt_is_closed(code, codelen)) {
			fputs(">>> ", stdout);
		} else {
			fputs("... ", stdout);
		}

		if (!fgets(buffer, sizeof(buffer), stdin)) {
			break;
		}
		buffer[sizeof(buffer) - 1] = '\0';

		if (strcmp(buffer, "help\n") == 0) {
			printf("'dis'  print bytecode\n"
					"'dis-nl'  print bytecode without linenumbers\n"
			       	"'list' print source code to screen\n"
			       	"'exit' exit from shell\n");
			continue;
		}
		if (strcmp(buffer, "dis\n") == 0) {
			machine_disassemble(osic, true);
			continue;
		}
		if (strcmp(buffer, "dis-nl\n") == 0) {
			machine_disassemble(osic, false);
			continue;
		}
		if (strcmp(buffer, "list\n") == 0) {
			printf("%.*s", codelen, code);
			continue;
		}
		if (strcmp(buffer, "exit\n") == 0) {
			break;
		}

		/* copy buffer to stmt for error recovery */
		memcpy(stmt + stmtlen, buffer, strlen(buffer));
		stmtlen += strlen(buffer);

		/* copy buffer to code */
		memcpy(code + codelen, buffer, strlen(buffer));
		codelen += strlen(buffer);

		if (!check_stmt_is_closed(code, codelen)) {
			continue;
		}
		input_set_buffer(osic, "main", code, codelen);
		lexer_next_token(osic);

		node = parser_parse(osic);
		if (!node) {
			codelen -= stmtlen;
			stmtlen = 0;
			fprintf(stderr, "osic: syntax error\n");

			continue;
		}

		if (!compiler_compile(osic, node)) {
			codelen -= stmtlen;
			stmtlen = 0;
			osic->l_generator = generator_create(osic);
			fprintf(stderr, "generator: syntax error\n");

			continue;
		}

		stmtlen = 0;
		osic_machine_reset(osic);
		generator_emit(osic);
		osic->l_generator = generator_create(osic);
		osic_machine_set_pc(osic, pc);
		if (osic_machine_get_fp(osic) >= 0) {
			frame = osic_machine_get_frame(osic, 0);
			frame = shell_extend_frame(osic,
			                           frame,
			                           node->nlocals);
			shell_restore_frame(osic,
			                    frame,
			                    nlocals,
			                    locals);
			osic_machine_set_frame(osic, 0, frame);
		}

		osic_machine_execute_loop(osic);
		pc = ((struct machine *)osic->l_machine)->maxpc;
		if (osic_machine_get_fp(osic) >= 0) {
			frame = osic_machine_get_frame(osic, 0);
			shell_store_frame(osic, frame, locals);
			nlocals = frame->nlocals;
		} else {
			break;
		}

		if (feof(stdin)) {
			printf("\n");

			break;
		}
	}
}

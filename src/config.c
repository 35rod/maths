#include "mml/config.h"

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "mml/parser.h"
#include "mml/token.h"
#include "mml/eval.h"

struct MML_config MML_global_config = {
	.PROG_NAME = NULL,
	.precision = 6,
	.runtime_flags = 0,
	.eval_state = nullptr,
	.last_print_was_newline = true,
};

strbuf expression = { NULL, 0, false };

static struct termios old_term;
static bool raw_mode_is_set = false;
void MML_term_set_raw_mode(void)
{
	struct termios new_term;
	tcgetattr(STDIN_FILENO, &old_term);
	new_term = old_term;
	new_term.c_lflag &= ~(ICANON | ECHO);
	new_term.c_cc[VMIN] = 0;
	new_term.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

	raw_mode_is_set = true;
	
	write(STDOUT_FILENO, "\x1b[5 q", 5);
}
void MML_term_restore(void)
{
	if (!raw_mode_is_set)
		return;

	tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
	raw_mode_is_set = false;

	write(STDOUT_FILENO, "\x1b[0 q", 5);
}

void MML_print_usage(void)
{
	fprintf(stderr, "usage: %s [options] <EXPR>\n\n"

                    "options:\n"
                    "  -d, --debug                        Enable debug output\n"
                    "  -P, --print                        Print the result value even if `print` was not called (default OFF)\n"
			  "  -E EXPR, --expr=EXPR               Alternate way to specify the expression to be evaluated\n"
                    "  -p PREC, --precision=PREC          Set the number of decimal digits to be printed when printing numbers (default 6)\n"
			  "  --no-eval                          Only parse the expression; don't evaluate it (default OFF)\n"
                    "  --bools-are-nums                   Write the number 1 or 0 to represent boolean values (default OFF)\n"
			  "  --no-estimate-equality             Consider two reals equal only if they are exactly equal to the bit (default OFF)\n"
			  "  -I, --interactive                  Start an interactive prompt (similar to the Python IDLE)\n"
			  "  -h, --help                         Display this help message\n"
			  "  -V, --version                      Display program information\n"
			  "  -                                  Read expression string from stdin\n"
			, MML_global_config.PROG_NAME);
	MML_cleanup_state(MML_global_config.eval_state);
	exit(1);
}

static void MML_print_info(void)
{
	printf("Compiled on " __DATE__ " at " __TIME__ " with "
#ifdef __GNUC__
	"GCC"
#elif defined(__clang__)
	"Clang"
#elif defined(__INTEL_COMPILER__)
	"an Intel compiler of"
#endif
		" version " __VERSION__ " and C standard version %ld.\n",
		__STDC_VERSION__);
	exit(1);
}

void MML_arg_parse(int32_t argc, char **argv)
{
	MML_global_config.PROG_NAME = argv[0];
	int32_t arg_n = 1;
	for (; arg_n < argc; ++arg_n)
	{
		if (strncmp(argv[arg_n], "--", 2) == 0)
		{
			if (strcmp(argv[arg_n]+2, "debug") == 0)
				SET_FLAG(DEBUG);
			else if (strcmp(argv[arg_n]+2, "print") == 0)
				SET_FLAG(PRINT);
			else if (strcmp(argv[arg_n]+2, "help") == 0)
				MML_print_usage();
			else if (strcmp(argv[arg_n]+2, "version") == 0)
				MML_print_info();
			else if (strncmp(argv[arg_n]+2, "precision=", 10) == 0)
				MML_global_config.precision = strtoul(argv[arg_n]+2+10, NULL, 10);
			else if (strncmp(argv[arg_n]+2, "expr=", 5) == 0)
				expression.s = argv[arg_n]+2+5;
			else if (strcmp(argv[arg_n]+2, "bools-are-nums") == 0)
				SET_FLAG(BOOLS_PRINT_NUM);
			else if (strcmp(argv[arg_n]+2, "no-estimate-equality") == 0)
				SET_FLAG(NO_ESTIMATE_EQUALITY);
			else if (strcmp(argv[arg_n]+2, "no-eval") == 0)
				SET_FLAG(NO_EVAL);
			else if (strcmp(argv[arg_n]+2, "interactive") == 0)
				SET_FLAG(RUN_PROMPT);
			else if (strncmp(argv[arg_n]+2, "set_var:", 8) == 0)
			{
				const char *cur = argv[arg_n]+2+8;
				cur = strchr(argv[arg_n]+2+8, '=');
				if (cur == NULL || *++cur == '\0')
				{
					fprintf(stderr, "argument error: expected expression following command-line variable definition\n");
					MML_cleanup_state(MML_global_config.eval_state);
					exit(1);
				}
				strbuf name = { argv[arg_n]+2+8, cur - (argv[arg_n]+2+8) - 1, false };
				MML_Expr *e = MML_parse(cur);
				MML_eval_set_variable(MML_global_config.eval_state, name, e);
				--e->num_refs;
			} else
			{
				fprintf(stderr, "argument error: unknown option '%s'\n", argv[arg_n]);
				MML_print_usage();
			}
		} else if (argv[arg_n][0] == '-')
		{
			char *cur = argv[arg_n];
			while (*++cur)
			{
				switch (*cur) {
				case 'h':
					MML_print_usage();
					break;
				case 'V':
					MML_print_info();
					break;
				case 'd':
					SET_FLAG(DEBUG);
					break;
				case 'P':
					SET_FLAG(PRINT);
					break;
				case 'I':
					SET_FLAG(RUN_PROMPT);
					break;
				case 'E':
					if (argv[arg_n+1] == NULL)
					{
						fprintf(stderr, "argument error: expression is required following "
								"'-E' argument to specify an expression to evaluate.\n");
						MML_print_usage();
					}
					expression.s = argv[++arg_n];
					break;
				case 'p':
					if (cur[1] != '\0')
					{
						MML_global_config.precision = strtoul(cur+1, &cur, 10);
						--cur;
						break;
					}
					if (argv[arg_n+1] == NULL)
					{
						fprintf(stderr, "argument error: integer is required following "
								"'-p' argument to specify precision.\n");
						MML_print_usage();
					}
					char *conv_end = NULL;
					uint32_t prec = strtoul(argv[++arg_n], &conv_end, 10);
					if (conv_end == argv[arg_n])
					{
						fprintf(stderr, "argument error: integer is required following "
								"'-p' argument to specify precision.\n");
						MML_print_usage();
					}
					MML_global_config.precision = prec;
					break;
				default:
					fprintf(stderr, "argument error: unknown option '-%c'\n", *cur);
					MML_print_usage();
				}
			}
			if (cur == argv[arg_n]+1)
				SET_FLAG(READ_STDIN);
		} else if (expression.s == NULL)
		{
			expression.s = argv[arg_n];
		}
	}

	if (expression.s == NULL && !FLAG_IS_SET(READ_STDIN))
		SET_FLAG(RUN_PROMPT);
}

strbuf MML_read_string_from_stream(FILE *stream)
{
	size_t buf_size = 2048;
	strbuf ret_buf = { NULL, 0, true };
	ret_buf.s = malloc(buf_size);
	if (ret_buf.s == NULL)
	{
		fprintf(stderr, "failed to allocate the initial buffer size\n");
		return (strbuf) { NULL, 0, 0 };
	}

	size_t cur_chunk_read;
	
	while ((cur_chunk_read = fread(ret_buf.s + ret_buf.len, 1, buf_size - ret_buf.len- 1, stream)) > 0)
	{
		ret_buf.len += cur_chunk_read;
		if (ret_buf.len >= buf_size - 1)
		{
			buf_size *= 2;
			char *temp_buf = realloc(ret_buf.s, buf_size);
			if (temp_buf == NULL)
			{
				free(ret_buf.s);
				fprintf(stderr, "failed to reallocate buffer with size %zu\n", buf_size);
				return (strbuf) { NULL, 0, 0 };
			}
			ret_buf.s = temp_buf;
		}
	}

	ret_buf.s[ret_buf.len++] = '\0';

	return ret_buf;
}

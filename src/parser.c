#include "osic.h"
#include "token.h"
#include "input.h"
#include "lexer.h"
#include "syntax.h"
#include "parser.h"

static struct syntax *
parser_expr(struct osic *osic);

static struct syntax *
parser_stmt(struct osic *osic);

static struct syntax *
parser_define_stmt(struct osic *osic);

static struct syntax *
parser_class_stmt(struct osic *osic);

static struct syntax *
parser_accessor(struct osic *osic);

static struct syntax *
parser_accessor_list(struct osic *osic);

static struct syntax *
parser_block_stmt(struct osic *osic);

static struct syntax *
parser_conditional(struct osic *osic);

static void
parser_error(struct osic *osic, char *message)
{
	fprintf(stderr,
	        "%s:%ld:%ld: error: %s",
	        input_filename(osic),
	        input_line(osic) + 1,
	        input_column(osic) + 1,
	        message);
}

static void
parser_expected(struct osic *osic, int token)
{
	fprintf(stderr,
	        "%s:%ld:%ld: error: "
	        "expected '%s' current is '%s'\n",
	        input_filename(osic),
	        input_line(osic) + 1,
	        input_column(osic) + 1,
	        token_name(token),
	        token_name(lexer_get_token(osic)));
}

/* if match fail return from parser */
#define PARSER_MATCH(osic, token) do {          \
	if ((token) == lexer_get_token(osic)) { \
		lexer_next_token(osic);         \
	} else {                                 \
		parser_expected(osic, (token)); \
		return NULL;                     \
	}                                        \
} while (0)

/*
 * parser naming rule:
 *
 *     parser_[name][s][_list]
 *
 * [name] is semantic name
 * [s] match plural ast but return a single node
 * [_list] return a linked list linked by ast->sibling
 *
 */

/*
 * name : NAME
 *      ;
 */
static struct syntax *
parser_name(struct osic *osic)
{
	struct syntax *node;

	node = syntax_make_name_node(osic,
	                          lexer_get_length(osic),
	                          lexer_get_buffer(osic));
	PARSER_MATCH(osic, TOKEN_NAME);

	return node;
}

/*
 * number : NUMBER
 *        ;
 */
static struct syntax *
parser_number(struct osic *osic)
{
	struct syntax *node;

	node = syntax_make_number_node(osic,
	                            lexer_get_length(osic),
	                            lexer_get_buffer(osic));
	PARSER_MATCH(osic, TOKEN_NUMBER);

	return node;
}

/*
 * number : STRING
 *        ;
 */
static struct syntax *
parser_string(struct osic *osic)
{
	struct syntax *node;

	node = syntax_make_string_node(osic,
	                            lexer_get_length(osic),
	                            lexer_get_buffer(osic));
	PARSER_MATCH(osic, TOKEN_STRING);

	return node;
}

/*
 * array : '[' ']'
 *       | '[' expr_list ']'
 *       ;
 *
 * expr_list : expr
 *           | expr ',' expr_list
 *           ;
 */
/* expr_list is n...0 list */
static struct syntax *
parser_array(struct osic *osic)
{
	struct syntax *expr;
	struct syntax *sibling;

	PARSER_MATCH(osic, TOKEN_LBRACK);
	expr = NULL;
	sibling = NULL;
	while (lexer_get_token(osic) != TOKEN_RBRACK) {
		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}

		expr->sibling = sibling;
		sibling = expr;

		if (lexer_get_token(osic) != TOKEN_COMMA) {
			break;
		}
		PARSER_MATCH(osic, TOKEN_COMMA);
	}
	PARSER_MATCH(osic, TOKEN_RBRACK);

	return syntax_make_array_node(osic, expr);
}

/*
 * dictionary : '{' '}'
 *            | '{' element_list '}'
 *            ;
 *
 * element_list : element
 *              | element ',' element_list
 *              ;
 *
 * element : expr ':' expr
 *         ;
 */
static struct syntax *
parser_dictionary(struct osic *osic)
{
	struct syntax *name;
	struct syntax *value;
	struct syntax *element;
	struct syntax *sibling;

	PARSER_MATCH(osic, TOKEN_LBRACE);
	element = NULL;
	sibling = NULL;
	while (lexer_get_token(osic) != TOKEN_RBRACE) {
		name = parser_expr(osic);
		if (!name) {
			return NULL;
		}
		PARSER_MATCH(osic, TOKEN_COLON);

		value = parser_expr(osic);
		if (!value) {
			return NULL;
		}

		element = syntax_make_element_node(osic, name, value);
		if (!element) {
			return NULL;
		}

		element->sibling = sibling;
		sibling = element;

		if (lexer_get_token(osic) != TOKEN_COMMA) {
			break;
		}
		PARSER_MATCH(osic, TOKEN_COMMA);
	}
	PARSER_MATCH(osic, TOKEN_RBRACE);

	return syntax_make_dictionary_node(osic, element);
}

/*
 * primary | NIL
 *         | TRUE
 *         | FALSE
 *         | SENTINEL
 *         | SELF
 *         | SUPER
 *         | name
 *         | number
 *         | string
 *         | define_stmt
 *         | class_stmt
 *         | '(' expr ')'
 *         ;
 */
static struct syntax *
parser_primary(struct osic *osic)
{
	struct syntax *node;

	switch (lexer_get_token(osic)) {
	case TOKEN_NAME:
		node = parser_name(osic);
		break;

	case TOKEN_NUMBER:
		node = parser_number(osic);
		break;

	case TOKEN_STRING:
		node = parser_string(osic);
		break;

	case TOKEN_LBRACK:
		node = parser_array(osic);
		break;

	case TOKEN_LBRACE:
		node = parser_dictionary(osic);
		break;

	case TOKEN_NIL:
		PARSER_MATCH(osic, TOKEN_NIL);
		node = syntax_make_nil_node(osic);
		break;

	case TOKEN_TRUE:
		PARSER_MATCH(osic, TOKEN_TRUE);
		node = syntax_make_true_node(osic);
		break;

	case TOKEN_FALSE:
		PARSER_MATCH(osic, TOKEN_FALSE);
		node = syntax_make_false_node(osic);
		break;

	case TOKEN_SENTINEL:
		PARSER_MATCH(osic, TOKEN_SENTINEL);
		node = syntax_make_sentinel_node(osic);
		break;

	case TOKEN_SELF:
		PARSER_MATCH(osic, TOKEN_SELF);
		node = syntax_make_self_node(osic);
		break;

	case TOKEN_SUPER:
		PARSER_MATCH(osic, TOKEN_SUPER);
		node = syntax_make_super_node(osic);
		break;

	case TOKEN_DEFINE:
		node = parser_define_stmt(osic);
		break;

	case TOKEN_CLASS:
		node = parser_class_stmt(osic);
		break;

	case TOKEN_LPAREN:
		PARSER_MATCH(osic, TOKEN_LPAREN);
		node = parser_expr(osic);
		if (!node) {
			return NULL;
		}
		PARSER_MATCH(osic, TOKEN_RPAREN);
		break;

	default:
		parser_error(osic, "unknown token\n");
		return NULL;
	}

	return node;
}

/*
 * argument : conditional
 *          | name '=' conditional
 *          ;
 */
static struct syntax *
parser_argument(struct osic *osic)
{
	int argument_type;
	struct syntax *name;
	struct syntax *expr;

	argument_type = 0;
	if (lexer_get_token(osic) == TOKEN_MUL) {
		PARSER_MATCH(osic, TOKEN_MUL);

		argument_type = 1;
		if (lexer_get_token(osic) == TOKEN_MUL) {
			PARSER_MATCH(osic, TOKEN_MUL);

			argument_type = 2;
		}
	}

	expr = parser_expr(osic);
	if (!expr) {
		return NULL;
	}

	name = NULL;
	if (lexer_get_token(osic) == TOKEN_ASSIGN) {
		PARSER_MATCH(osic, TOKEN_ASSIGN);

		name = expr;
		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}
	}

	return syntax_make_argument_node(osic, name, expr, argument_type);
}

/*
 * argument_list : argument
 *               | argument ',' argument_list
 *               ;
 */
/*
 * argument_list is is a n...0 order linked list
 */
static struct syntax *
parser_argument_list(struct osic *osic)
{
	int karg;
	struct syntax *sibling;
	struct syntax *argument;

	argument = parser_argument(osic);
	if (!argument) {
		return NULL;
	}

	karg = 0;
	if (argument->u.argument.name) {
		karg = 1;
	}

	/* this parser allow last ',' */
	if (lexer_get_token(osic) == TOKEN_COMMA) {
		PARSER_MATCH(osic, TOKEN_COMMA);

		sibling = argument;
		while (lexer_get_token(osic) != TOKEN_RPAREN) {
			argument = parser_argument(osic);
			if (!argument) {
				return NULL;
			}

			if (karg && !argument->u.argument.name) {
				return NULL;
			}

			if (argument->u.argument.name) {
				karg = 1;
			}

			argument->sibling = sibling;
			sibling = argument;

			if (lexer_get_token(osic) != TOKEN_COMMA) {
				break;
			}
			PARSER_MATCH(osic, TOKEN_COMMA);
		}
	}

	return argument;
}

/*
 * postfix : primary
 *         | postfix '(' ')'
 *         | postfix '(' argument_list ')'
 *         | postfix '[' expr ']'
 *         | postfix '.' name
 *         ;
 */
static struct syntax *
parser_postfix(struct osic *osic)
{
	int slice;
	struct syntax *node;
	struct syntax *stop;
	struct syntax *step;
	struct syntax *start;
	struct syntax *right;
	struct syntax *argument_list;

	node = parser_primary(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_LPAREN:
			PARSER_MATCH(osic, TOKEN_LPAREN);

			argument_list = NULL;
			if (lexer_get_token(osic) != TOKEN_RPAREN) {
				argument_list = parser_argument_list(osic);
				if (!argument_list) {
					return NULL;
				}
			}
			PARSER_MATCH(osic, TOKEN_RPAREN);
			node = syntax_make_call_node(osic,
			                             node,
			                             argument_list);
			break;

		case TOKEN_LBRACK:
			PARSER_MATCH(osic, TOKEN_LBRACK);
			slice = 0;
			if (lexer_get_token(osic) == TOKEN_COLON) {
				PARSER_MATCH(osic, TOKEN_COLON);
				start = NULL;
				slice = 1;
			} else {
				start = parser_expr(osic);
				if (!start) {
					return NULL;
				}
				if (lexer_get_token(osic) == TOKEN_COLON) {
					PARSER_MATCH(osic, TOKEN_COLON);
					slice = 1;
				}
			}

			stop = NULL;
			if (lexer_get_token(osic) == TOKEN_COLON) {
				PARSER_MATCH(osic, TOKEN_COLON);
				stop = NULL;
			} else if (slice) {
				stop = NULL;
				if (lexer_get_token(osic) != TOKEN_RBRACK) {
					stop = parser_expr(osic);
					if (!stop) {
						return NULL;
					}
				}
				if (lexer_get_token(osic) == TOKEN_COLON) {
					PARSER_MATCH(osic, TOKEN_COLON);
				}
			}

			if (slice) {
				step = NULL;
				if (lexer_get_token(osic) != TOKEN_RBRACK) {
					step = parser_expr(osic);
					if (!step) {
						return NULL;
					}
				}
				PARSER_MATCH(osic, TOKEN_RBRACK);

				node = syntax_make_get_slice_node(osic,
				                               node,
				                               start,
				                               stop,
				                               step);
			} else {
				PARSER_MATCH(osic, TOKEN_RBRACK);
				if (!start) {
					return NULL;
				}
				node = syntax_make_get_item_node(osic,
				                              node,
				                              start);
			}
			break;

		case TOKEN_DOT:
			PARSER_MATCH(osic, TOKEN_DOT);
			right = parser_name(osic);
			if (!right) {
				return NULL;
			}
			node = syntax_make_get_attr_node(osic, node, right);
			break;

		default:
			return node;
		}
	}

	return node;
}

/*
 * prefix : postfix
 *        | '!' prefix
 *        | '-' prefix
 *        ;
 */
static struct syntax *
parser_prefix(struct osic *osic)
{
	struct syntax *node;
	struct syntax *left;

	switch (lexer_get_token(osic)) {
	case TOKEN_BITWISE_NOT:
		PARSER_MATCH(osic, TOKEN_BITWISE_NOT);
		left = parser_prefix(osic);
		if (!left) {
			return NULL;
		}
		node = syntax_make_unop_node(osic,
		                             SYNTAX_OPKIND_BITWISE_NOT,
		                             left);
		break;

	case TOKEN_LOGICAL_NOT:
		PARSER_MATCH(osic, TOKEN_LOGICAL_NOT);
		left = parser_prefix(osic);
		if (!left) {
			return NULL;
		}
		node = syntax_make_unop_node(osic,
		                             SYNTAX_OPKIND_LOGICAL_NOT,
		                             left);
		break;

	case TOKEN_SUB:
		PARSER_MATCH(osic, TOKEN_SUB);
		left = parser_prefix(osic);
		if (!left) {
			return NULL;
		}
		node = syntax_make_unop_node(osic,
		                             SYNTAX_OPKIND_NEG,
		                             left);
		break;

	case TOKEN_ADD:
		PARSER_MATCH(osic, TOKEN_ADD);
		left = parser_prefix(osic);
		if (!left) {
			return NULL;
		}
		node = syntax_make_unop_node(osic,
		                             SYNTAX_OPKIND_POS,
		                             left);
		break;

	default:
		node = parser_postfix(osic);
	}

	return node;
}

/*
 * multiplicative : prefix
 *                | prefix '*' multiplicative
 *                | prefix '/' multiplicative
 *                | prefix '%' multiplicative
 *                ;
 */
static struct syntax *
parser_multiplicative(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_prefix(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_MUL:
			PARSER_MATCH(osic, TOKEN_MUL);
			opkind = SYNTAX_OPKIND_MUL;
			break;

		case TOKEN_DIV:
			PARSER_MATCH(osic, TOKEN_DIV);
			opkind = SYNTAX_OPKIND_DIV;
			break;

		case TOKEN_MOD:
			PARSER_MATCH(osic, TOKEN_MOD);
			opkind = SYNTAX_OPKIND_MOD;
			break;

		default:
			return node;
		}

		right = parser_prefix(osic);
		if (!right) {
			return NULL;
		}

		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * additive : multiplicative
 *          | additive '+' multiplicative
 *          | additive '-' multiplicative
 *          ;
 */
static struct syntax *
parser_additive(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_multiplicative(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_ADD:
			PARSER_MATCH(osic, TOKEN_ADD);
			opkind = SYNTAX_OPKIND_ADD;
			break;

		case TOKEN_SUB:
			PARSER_MATCH(osic, TOKEN_SUB);
			opkind = SYNTAX_OPKIND_SUB;
			break;

		default:
			return node;
		}

		right = parser_multiplicative(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * shifting : additive
 *          | additive '<<' shifting
 *          | additive '>>' shifting
 *          ;
 */
static struct syntax *
parser_shifting(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_additive(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_SHL:
			PARSER_MATCH(osic, TOKEN_SHL);
			opkind = SYNTAX_OPKIND_SHL;
			break;

		case TOKEN_SHR:
			PARSER_MATCH(osic, TOKEN_SHR);
			opkind = SYNTAX_OPKIND_SHR;
			break;

		default:
			return node;
		}

		right = parser_additive(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * relational : shifting
 *            | shifting '<' relational
 *            | shifting '<=' relational
 *            | shifting '>' relational
 *            | shifting '>=' relational
 *            ;
 */
static struct syntax *
parser_relational(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_shifting(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_LT:
			PARSER_MATCH(osic, TOKEN_LT);
			opkind = SYNTAX_OPKIND_LT;
			break;

		case TOKEN_LE:
			PARSER_MATCH(osic, TOKEN_LE);
			opkind = SYNTAX_OPKIND_LE;
			break;

		case TOKEN_GT:
			PARSER_MATCH(osic, TOKEN_GT);
			opkind = SYNTAX_OPKIND_GT;
			break;

		case TOKEN_GE:
			PARSER_MATCH(osic, TOKEN_GE);
			opkind = SYNTAX_OPKIND_GE;
			break;

		default:
			return node;
		}

		right = parser_shifting(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * equality : relational
 *          | relational '==' equality
 *          | relational '!=' equality
 *          | relational 'in' equality
 *          ;
 */
static struct syntax *
parser_equality(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_relational(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_EQ:
			PARSER_MATCH(osic, TOKEN_EQ);
			opkind = SYNTAX_OPKIND_EQ;
			break;

		case TOKEN_NE:
			PARSER_MATCH(osic, TOKEN_NE);
			opkind = SYNTAX_OPKIND_NE;
			break;

		case TOKEN_IN:
			PARSER_MATCH(osic, TOKEN_IN);
			opkind = SYNTAX_OPKIND_IN;
			break;

		default:
			return node;
		}

		right = parser_relational(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * bitwise_and : equality
 *             | equality '&&' bitwise_and
 *             ;
 */
static struct syntax *
parser_bitwise_and(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_equality(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_BITWISE_AND:
			PARSER_MATCH(osic, TOKEN_BITWISE_AND);
			opkind = SYNTAX_OPKIND_BITWISE_AND;
			break;

		default:
			return node;
		}

		right = parser_equality(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * bitwise_xor : bitwise_and
 *             | bitwise_and '^' bitwise_xor
 *             ;
 */
static struct syntax *
parser_bitwise_xor(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_bitwise_and(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_BITWISE_XOR:
			PARSER_MATCH(osic, TOKEN_BITWISE_XOR);
			opkind = SYNTAX_OPKIND_BITWISE_XOR;
			break;

		default:
			return node;
		}

		right = parser_bitwise_and(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * bitwise_or : bitwise_xor
 *            | bitwise_xor '|' bitwise_or
 *            ;
 */
static struct syntax *
parser_bitwise_or(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_bitwise_xor(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_BITWISE_OR:
			PARSER_MATCH(osic, TOKEN_BITWISE_OR);
			opkind = SYNTAX_OPKIND_BITWISE_OR;
			break;

		default:
			return node;
		}

		right = parser_bitwise_and(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * logical_and : bitwise_or
 *             | bitwise_or '&&' logical_and
 *             ;
 */
static struct syntax *
parser_logical_and(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_bitwise_or(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_LOGICAL_AND:
			PARSER_MATCH(osic, TOKEN_LOGICAL_AND);
			opkind = SYNTAX_OPKIND_LOGICAL_AND;
			break;

		default:
			return node;
		}

		right = parser_bitwise_or(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * logical_or : logical_and
 *            | logical_or '||' logical_and
 *            ;
 */
static struct syntax *
parser_logical_or(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;

	node = parser_logical_and(osic);
	while (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_LOGICAL_OR:
			PARSER_MATCH(osic, TOKEN_LOGICAL_OR);
			opkind = SYNTAX_OPKIND_LOGICAL_OR;
			break;

		default:
			return node;
		}

		right = parser_logical_and(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_binop_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * condition : logical_or
 *           | logical_or ? logical_or : condition
 *           ;
 */
static struct syntax *
parser_conditional(struct osic *osic)
{
	struct syntax *node;
	struct syntax *true_expr;
	struct syntax *false_expr;

	node = parser_logical_or(osic);
	if (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_CONDITIONAL:
			PARSER_MATCH(osic, TOKEN_CONDITIONAL);
			true_expr = NULL;
			if (lexer_get_token(osic) != TOKEN_COLON) {
				true_expr = parser_logical_or(osic);
				if (!true_expr) {
					break;
				}
			}
			PARSER_MATCH(osic, TOKEN_COLON);

			false_expr = parser_expr(osic);
			if (!false_expr) {
				break;
			}

			node = syntax_make_conditional_node(osic,
			                                 node,
			                                 true_expr,
			                                 false_expr);
			break;

		default:
			return node;
		}
	}

	return node;
}

/*
 * expr : conditional
 *      ;
 */
static struct syntax *
parser_expr(struct osic *osic)
{
	return parser_conditional(osic);
}

/*
 * assign_stmt : conditional
 *             | unpack '=' conditional
 *             | conditional '+=' conditional
 *             | conditional '-=' conditional
 *             | conditional '*=' conditional
 *             | conditional '/=' conditional
 *             | conditional '%=' conditional
 *             | conditional '&=' conditional
 *             | conditional '|=' conditional
 *             | conditional '<<=' conditional
 *             | conditional '>>=' conditional
 *             ;
 */
/*
 * unpack : conditional
 *        | conditional ','
 *        | conditional ',' unpack
 *        ;
 */
static struct syntax *
parser_assign_stmt(struct osic *osic)
{
	int opkind;
	struct syntax *node;
	struct syntax *right;
	struct syntax *sibling;

	node = parser_expr(osic);
	if (node) {
		switch (lexer_get_token(osic)) {
		case TOKEN_COMMA:
			sibling = node;
			while (lexer_get_token(osic) == TOKEN_COMMA) {
				PARSER_MATCH(osic, TOKEN_COMMA);
				sibling = parser_expr(osic);
				if (!sibling) {
					break;
				}
				sibling->sibling = node;
				node = sibling;
			}
			node = syntax_make_unpack_node(osic, sibling);

		/* fallthrough */
		case TOKEN_ASSIGN:
			PARSER_MATCH(osic, TOKEN_ASSIGN);
			opkind = SYNTAX_OPKIND_ASSIGN;
			break;

		case TOKEN_ADD_ASSIGN:
			PARSER_MATCH(osic, TOKEN_ADD_ASSIGN);
			opkind = SYNTAX_OPKIND_ADD_ASSIGN;
			break;

		case TOKEN_SUB_ASSIGN:
			PARSER_MATCH(osic, TOKEN_SUB_ASSIGN);
			opkind = SYNTAX_OPKIND_SUB_ASSIGN;
			break;

		case TOKEN_MUL_ASSIGN:
			PARSER_MATCH(osic, TOKEN_MUL_ASSIGN);
			opkind = SYNTAX_OPKIND_MUL_ASSIGN;
			break;

		case TOKEN_DIV_ASSIGN:
			PARSER_MATCH(osic, TOKEN_DIV_ASSIGN);
			opkind = SYNTAX_OPKIND_DIV_ASSIGN;
			break;

		case TOKEN_MOD_ASSIGN:
			PARSER_MATCH(osic, TOKEN_MOD_ASSIGN);
			opkind = SYNTAX_OPKIND_MOD_ASSIGN;
			break;

		case TOKEN_BITWISE_AND_ASSIGN:
			PARSER_MATCH(osic, TOKEN_BITWISE_AND_ASSIGN);
			opkind = SYNTAX_OPKIND_BITWISE_AND_ASSIGN;
			break;

		case TOKEN_BITWISE_OR_ASSIGN:
			PARSER_MATCH(osic, TOKEN_BITWISE_OR_ASSIGN);
			opkind = SYNTAX_OPKIND_BITWISE_OR_ASSIGN;
			break;

		case TOKEN_SHL_ASSIGN:
			PARSER_MATCH(osic, TOKEN_SHL_ASSIGN);
			opkind = SYNTAX_OPKIND_SHL_ASSIGN;
			break;

		case TOKEN_SHR_ASSIGN:
			PARSER_MATCH(osic, TOKEN_SHR_ASSIGN);
			opkind = SYNTAX_OPKIND_SHR_ASSIGN;
			break;

		default:
			return node;
		}

		right = parser_expr(osic);
		if (!right) {
			return NULL;
		}
		node = syntax_make_assign_node(osic, opkind, node, right);
	}

	return node;
}

/*
 * if_stmt : if '(' conditional ')' block_stmt
 *         | if '(' conditional ')' block_stmt 'else' block_stmt
 *         ;
 */
static struct syntax *
parser_if_stmt(struct osic *osic)
{
	struct syntax *expr;
	struct syntax *then_block_stmt;
	struct syntax *else_block_stmt;

	PARSER_MATCH(osic, TOKEN_IF);

	PARSER_MATCH(osic, TOKEN_LPAREN);
	expr = parser_expr(osic);
	if (!expr) {
		return NULL;
	}
	PARSER_MATCH(osic, TOKEN_RPAREN);

	then_block_stmt = parser_block_stmt(osic);
	if (!then_block_stmt) {
		return NULL;
	}

	else_block_stmt = NULL;
	if (lexer_get_token(osic) == TOKEN_ELSE) {
		PARSER_MATCH(osic, TOKEN_ELSE);

		if (lexer_get_token(osic) == TOKEN_IF) {
			else_block_stmt = parser_if_stmt(osic);
		} else {
			else_block_stmt = parser_block_stmt(osic);
		}

		if (!else_block_stmt) {
			return NULL;
		}
	}

	return syntax_make_if_node(osic,
	                           expr,
	                           then_block_stmt,
	                           else_block_stmt);
}

/*
 * for_stmt : for '(' expr ';' expr ';' expr ')' block_stmt
 *          | for '(' expr ';' expr ';' assign_stmt ')' block_stmt
 *          | for '(' 'var' name expr ';' expr ';' expr ')' block_stmt
 *          | for '(' 'var' name expr ';' expr ';' assign_stmt ')' block_stmt
 *          | for '(' name 'in' expr ')' block_stmt
 *          | for '(' 'var' name 'in' expr ')' block_stmt
 *          ;
 */
static struct syntax *
parser_for_stmt(struct osic *osic)
{
	struct syntax *name;
	struct syntax *expr;
	struct syntax *iter;
	struct syntax *init_expr;
	struct syntax *cond_expr;
	struct syntax *step_expr;
	struct syntax *block_stmt;

	PARSER_MATCH(osic, TOKEN_FOR);

	PARSER_MATCH(osic, TOKEN_LPAREN);
	if (lexer_get_token(osic) == TOKEN_VAR) {
		PARSER_MATCH(osic, TOKEN_VAR);

		name = parser_name(osic);
		expr = NULL;
		if (lexer_get_token(osic) == TOKEN_ASSIGN) {
			PARSER_MATCH(osic, TOKEN_ASSIGN);
			expr = parser_expr(osic);
			if (!expr) {
				return NULL;
			}
		}
		init_expr = syntax_make_var_node(osic, name, expr);
		if (!init_expr) {
			return NULL;
		}
	} else {
		init_expr = parser_assign_stmt(osic);
		if (!init_expr) {
			return NULL;
		}
	}

	/*
	 * for '(' expr ';' expr ';' expr ')'
	 * for '(' 'var' expr ';' expr ';' expr ')'
	 */
	if (lexer_get_token(osic) == TOKEN_SEMICON) {
		PARSER_MATCH(osic, TOKEN_SEMICON);

		cond_expr = parser_expr(osic);
		if (!cond_expr) {
			return NULL;
		}
		PARSER_MATCH(osic, TOKEN_SEMICON);

		step_expr = parser_assign_stmt(osic);
		if (!step_expr) {
			return NULL;
		}
		PARSER_MATCH(osic, TOKEN_RPAREN);

		block_stmt = parser_block_stmt(osic);
		if (!block_stmt) {
			return NULL;
		}

		return syntax_make_for_node(osic,
		                            init_expr,
		                            cond_expr,
		                            step_expr,
		                            block_stmt);
	}

	/*
	 * for '(' 'var' expr in expr ')'
	 */
	if (lexer_get_token(osic) == TOKEN_IN &&
	    init_expr->kind == SYNTAX_KIND_VAR_STMT &&
	    init_expr->u.var_stmt.expr == NULL)
	{
		PARSER_MATCH(osic, TOKEN_IN);

		iter = parser_assign_stmt(osic);
		if (!iter) {
			return NULL;
		}
		PARSER_MATCH(osic, TOKEN_RPAREN);

		block_stmt = parser_block_stmt(osic);
		if (!block_stmt) {
			return NULL;
		}

		return syntax_make_forin_node(osic,
		                              init_expr,
		                              iter,
		                              block_stmt);
	}

	/*
	 * for '(' expr in expr ')'
	 */
	if (lexer_get_token(osic) == TOKEN_RPAREN &&
	    init_expr->kind == SYNTAX_KIND_BINOP &&
	    init_expr->opkind == SYNTAX_OPKIND_IN)
	{
		PARSER_MATCH(osic, TOKEN_RPAREN);

		block_stmt = parser_block_stmt(osic);
		if (!block_stmt) {
			return NULL;
		}

		return syntax_make_forin_node(osic,
		                              init_expr->u.binop.left,
		                              init_expr->u.binop.right,
		                              block_stmt);
	}

	parser_error(osic, "for syntax error\n");
	return NULL;
}

/*
 * while_stmt : 'while' '(' expr ')' block_stmt
 *            ;
 */
static struct syntax *
parser_while_stmt(struct osic *osic)
{
	struct syntax *expr;
	struct syntax *block_stmt;

	PARSER_MATCH(osic, TOKEN_WHILE);

	PARSER_MATCH(osic, TOKEN_LPAREN);
	expr = parser_expr(osic);
	if (!expr) {
		return NULL;
	}
	PARSER_MATCH(osic, TOKEN_RPAREN);

	block_stmt = parser_block_stmt(osic);
	if (!block_stmt) {
		return NULL;
	}

	return syntax_make_while_node(osic, expr, block_stmt);
}

/*
 * catch_stmt : 'catch' '(' name name ')' block_stmt
 *            ;
 */
static struct syntax *
parser_catch_stmt(struct osic *osic)
{
	struct syntax *catch_type;
	struct syntax *catch_name;
	struct syntax *block_stmt;

	PARSER_MATCH(osic, TOKEN_CATCH);
	PARSER_MATCH(osic, TOKEN_LPAREN);

	catch_type = parser_postfix(osic);
	if (!catch_type) {
		return NULL;
	}

	catch_name = parser_name(osic);
	if (!catch_name) {
		return NULL;
	}
	PARSER_MATCH(osic, TOKEN_RPAREN);

	block_stmt = parser_block_stmt(osic);
	if (!block_stmt) {
		return NULL;
	}

	return syntax_make_catch_node(osic,
	                           catch_type,
	                           catch_name,
	                           block_stmt);
}

/*
 * catch_stmt_list : catch_stmt
 *                 | catch_stmt catch_stmt_list
 *                 ;
 */
static struct syntax *
parser_catch_stmt_list(struct osic *osic)
{
	struct syntax *catch_stmt;
	struct syntax *first;
	struct syntax *sibling;

	first = NULL;
	if (lexer_get_token(osic) == TOKEN_CATCH) {
		catch_stmt = parser_catch_stmt(osic);
		if (!catch_stmt) {
			return NULL;
		}

		first = catch_stmt;
		sibling = catch_stmt;
		while (lexer_get_token(osic) == TOKEN_CATCH) {
			catch_stmt = parser_catch_stmt(osic);
			if (!catch_stmt) {
				return NULL;
			}
			sibling->sibling = catch_stmt;
			sibling = catch_stmt;
		}
	}

	return first;
}

/*
 * try_stmt : 'try' block_stmt catch_stmt_list_stmt
 *          | 'try' block_stmt catch_stmt_list 'finally' block_stmt
 *          | 'try' block_stmt 'finally' block_stmt
 *          ;
 */
static struct syntax *
parser_try_stmt(struct osic *osic)
{
	struct syntax *try_block_stmt;
	struct syntax *catch_stmt_list;
	struct syntax *finally_block_stmt;

	PARSER_MATCH(osic, TOKEN_TRY);

	try_block_stmt = parser_block_stmt(osic);
	if (!try_block_stmt) {
		return NULL;
	}

	catch_stmt_list = parser_catch_stmt_list(osic);

	finally_block_stmt = NULL;
	if (lexer_get_token(osic) == TOKEN_FINALLY) {
		PARSER_MATCH(osic, TOKEN_FINALLY);

		finally_block_stmt = parser_block_stmt(osic);
		if (!finally_block_stmt) {
			return NULL;
		}
	}

	return syntax_make_try_node(osic,
	                         try_block_stmt,
	                         catch_stmt_list,
	                         finally_block_stmt);
}

/*
 * break_stmt : 'break' ';'
 *            ;
 */
static struct syntax *
parser_break_stmt(struct osic *osic)
{
	PARSER_MATCH(osic, TOKEN_BREAK);
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_break_node(osic);
}

/*
 * continue_stmt : 'continue' ';'
 *               ;
 */
static struct syntax *
parser_continue_stmt(struct osic *osic)
{
	PARSER_MATCH(osic, TOKEN_CONTINUE);
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_continue_node(osic);
}

/*
 * delete_stmt : delete expr ';'
 *             ;
 */
static struct syntax *
parser_delete_stmt(struct osic *osic)
{
	struct syntax *expr;

	PARSER_MATCH(osic, TOKEN_DELETE);
	expr = NULL;
	if (lexer_get_token(osic) != TOKEN_SEMICON) {
		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}
	}
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_delete_node(osic, expr);
}

/*
 * return_stmt : return ';'
 *             | return expr ';'
 *             ;
 */
static struct syntax *
parser_return_stmt(struct osic *osic)
{
	struct syntax *expr;

	PARSER_MATCH(osic, TOKEN_RETURN);
	expr = NULL;
	if (lexer_get_token(osic) != TOKEN_SEMICON) {
		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}
	}
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_return_node(osic, expr);
}

/*
 * import_stmt : 'import' string 'as' name ';'
 *             ;
 */
static struct syntax *
parser_import_stmt(struct osic *osic)
{
	struct syntax *name;
	struct syntax *path_string;

	PARSER_MATCH(osic, TOKEN_IMPORT);
	path_string = parser_string(osic);
	if (!path_string) {
		return NULL;
	}

	name = NULL;
	if (lexer_get_token(osic) == TOKEN_AS) {
		PARSER_MATCH(osic, TOKEN_AS);
		name = parser_name(osic);
		if (!name) {
			return NULL;
		}
	}
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_import_node(osic, name, path_string);
}

/*
 * throw_stmt : throw expr ';'
 *            ;
 */
static struct syntax *
parser_throw_stmt(struct osic *osic)
{
	struct syntax *expr;

	PARSER_MATCH(osic, TOKEN_THROW);

	expr = parser_expr(osic);
	if (!expr) {
		return NULL;
	}
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return syntax_make_throw_node(osic, expr);
}

/*
 * var_stmt_list : 'var' name_list ';'
 *               | 'var' name '=' expr ';'
 *               ;
 */

/*
 * name_list : name
 *           | name ',' name_list
 *           ;
 */
static struct syntax *
parser_var_stmt_list(struct osic *osic)
{
	struct syntax *name;
	struct syntax *expr;
	struct syntax *node;
	struct syntax *sibling;

	PARSER_MATCH(osic, TOKEN_VAR);
	name = parser_name(osic);
	expr = NULL;
	if (lexer_get_token(osic) == TOKEN_ASSIGN) {
		PARSER_MATCH(osic, TOKEN_ASSIGN);
		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}
	}

	node = syntax_make_var_node(osic, name, expr);

	/*
	 * disable multiple declare with assignment
	 * `var a,b,c=[1,2,3];' result `a = nil; b = nil; c = [1,2,3];`
	 * `a,b,c=[1,2,3];' result  `a = 1; b = 2; c = 3;`
	 */
	if (!expr) {
		sibling = node;
		while (lexer_get_token(osic) == TOKEN_COMMA) {
			PARSER_MATCH(osic, TOKEN_COMMA);
			name = parser_name(osic);
			if (!name) {
				return NULL;
			}
			sibling->sibling = syntax_make_var_node(osic,
			                                        name,
			                                        NULL);
			sibling = sibling->sibling;
		}
	}
	PARSER_MATCH(osic, TOKEN_SEMICON);

	return node;
}

/*
 * parameter : 'var' name
 *           | 'var' name '=' conditional
 *           | accessor_list 'var' name
 *           | accessor_list 'var' name '=' conditional
 *           ;
 */
static struct syntax *
parser_parameter(struct osic *osic)
{
	int parameter_type;
	struct syntax *name;
	struct syntax *expr;
	struct syntax *accessor_list;

	accessor_list = NULL;
	if (lexer_get_token(osic) == TOKEN_ACCESSOR) {
		accessor_list = parser_accessor_list(osic);
		if (!accessor_list) {
			return NULL;
		}
	}
	PARSER_MATCH(osic, TOKEN_VAR);
	parameter_type = 0;
	if (lexer_get_token(osic) == TOKEN_MUL) {
		PARSER_MATCH(osic, TOKEN_MUL);

		parameter_type = 1;
		if (lexer_get_token(osic) == TOKEN_MUL) {
			PARSER_MATCH(osic, TOKEN_MUL);

			parameter_type = 2;
		}
	}
	name = parser_name(osic);
	if (!name) {
		return NULL;
	}

	expr = NULL;
	if (lexer_get_token(osic) == TOKEN_ASSIGN) {
		PARSER_MATCH(osic, TOKEN_ASSIGN);

		expr = parser_expr(osic);
		if (!expr) {
			return NULL;
		}
	}

	return syntax_make_parameter_node(osic, name, expr,
	                               accessor_list, parameter_type);
}

/*
 * parameter_list : parameter
 *                | parameter ',' parameter_list
 *                ;
 */
/*
 * parameter_list is is a 0...n order linked list
 */
static struct syntax *
parser_parameter_list(struct osic *osic)
{
	int karg;
	struct syntax *first;
	struct syntax *sibling;
	struct syntax *parameter;

	parameter = parser_parameter(osic);
	if (!parameter) {
		return NULL;
	}

	karg = 0;
	if (parameter->u.parameter.name) {
		karg = 1;
	}

	first = parameter;
	if (lexer_get_token(osic) == TOKEN_COMMA) {
		PARSER_MATCH(osic, TOKEN_COMMA);

		sibling = parameter;
		while (lexer_get_token(osic) != TOKEN_RPAREN) {
			parameter = parser_parameter(osic);
			if (!parameter) {
				return NULL;
			}

			if (karg && !parameter->u.parameter.name) {
				return NULL;
			}

			if (parameter->u.parameter.name) {
				karg = 1;
			}

			sibling->sibling = parameter;
			sibling = parameter;

			if (lexer_get_token(osic) != TOKEN_COMMA) {
				break;
			}
			PARSER_MATCH(osic, TOKEN_COMMA);
		}
	}

	return first;
}
/*
 * define_stmt : 'define' '(' ')' block_stmt
 *             | 'define' '(' parameter_list ')' block_stmt
 *             | 'define' name '(' ')' block_stmt
 *             | 'define' name '(' parameter_list ')' block_stmt
 *             ;
 */
static struct syntax *
parser_define_stmt(struct osic *osic)
{
	struct syntax *name;
	struct syntax *parameter_list;
	struct syntax *block_stmt;

	PARSER_MATCH(osic, TOKEN_DEFINE);

	name = NULL;
	if (lexer_get_token(osic) == TOKEN_NAME) {
		name = parser_name(osic);
		if (!name) {
			return NULL;
		}
	}

	PARSER_MATCH(osic, TOKEN_LPAREN);
	parameter_list = NULL;
	if (lexer_get_token(osic) != TOKEN_RPAREN) {
		parameter_list = parser_parameter_list(osic);
		if (!parameter_list) {
			return NULL;
		}
	}
	PARSER_MATCH(osic, TOKEN_RPAREN);

	block_stmt = parser_block_stmt(osic);
	if (!block_stmt) {
		return NULL;
	}

	return syntax_make_define_node(osic,
	                               name,
	                               parameter_list,
	                               block_stmt);
}

/*
 * class_super_list : postfix
 *                  | postfix ',' class_super_list
 *                  ;
 */
static struct syntax *
parser_class_super_list(struct osic *osic)
{
	struct syntax *name;
	struct syntax *first;
	struct syntax *sibling;

	name = parser_postfix(osic);
	if (!name) {
		return NULL;
	}

	first = name;
	sibling = name;
	while (lexer_get_token(osic) != TOKEN_RPAREN) {
		if (lexer_get_token(osic) == TOKEN_COMMA) {
			PARSER_MATCH(osic, TOKEN_COMMA);
		}

		name = parser_postfix(osic);
		if (!name) {
			return NULL;
		}

		sibling->sibling = name;
		sibling = name;
	}

	return first;
}

/*
 * class_decls_stmt_list : var_stmt
 *                       | var_stmt class_decls_stmt_list
 *                       | define_stmt
 *                       | define_stmt class_decls_stmt_list
 *                       ;
 */
static struct syntax *
parser_class_decls_stmt_list(struct osic *osic)
{
	struct syntax *stmt;
	struct syntax *first;
	struct syntax *sibling;

	first = NULL;
	sibling = NULL;
	while (lexer_get_token(osic) != TOKEN_RBRACE) {
		if (lexer_get_token(osic) == TOKEN_VAR) {
			stmt = parser_var_stmt_list(osic);
		} else if (lexer_get_token(osic) == TOKEN_DEFINE) {
			stmt = parser_define_stmt(osic);
		} else if (lexer_get_token(osic) == TOKEN_ACCESSOR) {
			stmt = parser_accessor(osic);
		} else {
			parser_error(osic,
			             "class only take var "
			             "and define statement\n");

			return NULL;
		}

		if (!stmt) {
			return NULL;
		}

		if (sibling) {
			sibling->sibling = stmt;
		}
		sibling = stmt;

		if (!first) {
			first = stmt;
		}
	}

	return first;
}

/*
 * class_block_stmt : '{' class_decls_stmt_list '}'
 *                  ;
 */
static struct syntax *
parser_class_block_stmt(struct osic *osic)
{
	struct syntax *stmt_list;

	PARSER_MATCH(osic, TOKEN_LBRACE);
	stmt_list = parser_class_decls_stmt_list(osic);
	PARSER_MATCH(osic, TOKEN_RBRACE);

	return syntax_make_block_node(osic, stmt_list);
}

/*
 * class_stmt : 'class' class_block_stmt
 *            | 'class' name '(' ')' class_block_stmt
 *            | 'class' name '(' class_super_list ')' class_block_stmt
 *            ;
 */
static struct syntax *
parser_class_stmt(struct osic *osic)
{
	struct syntax *name;
	struct syntax *super_list;
	struct syntax *block_stmt;

	PARSER_MATCH(osic, TOKEN_CLASS);

	name = NULL;
	if (lexer_get_token(osic) == TOKEN_NAME) {
		name = parser_name(osic);
		if (!name) {
			return NULL;
		}
	}

	super_list = NULL;
	if (lexer_get_token(osic) == TOKEN_LPAREN) {
		PARSER_MATCH(osic, TOKEN_LPAREN);

		if (lexer_get_token(osic) == TOKEN_NAME) {
			super_list = parser_class_super_list(osic);
			if (!super_list) {
				return NULL;
			}
		}
		PARSER_MATCH(osic, TOKEN_RPAREN);
	}

	block_stmt = parser_class_block_stmt(osic);
	if (!block_stmt) {
		return NULL;
	}

	return syntax_make_class_node(osic, name, super_list, block_stmt);
}

/*
 * accessor_list : '@' name
 *             | '@' name accessor_list
 *             | '@' 'getter' '(' name ')'
 *             | '@' 'getter' '(' name ')' accessor_list
 *             | '@' 'setter' '(' name ')'
 *             | '@' 'setter' '(' name ')' accessor_list
 *             ;
 */
/* return n..1 list */
static struct syntax *
parser_accessor_list(struct osic *osic)
{
	struct syntax *node;
	struct syntax *name;
	struct syntax *sibling;
	struct syntax *accessor;

	node = NULL;
	sibling = NULL;
	while (lexer_get_token(osic) == TOKEN_ACCESSOR) {
		PARSER_MATCH(osic, TOKEN_ACCESSOR);

		if (lexer_get_token(osic) == TOKEN_ACCESSOR_GETTER) {
			PARSER_MATCH(osic, TOKEN_ACCESSOR_GETTER);
			name = syntax_make_name_node(osic, 6, "getter");
		} else if (lexer_get_token(osic) == TOKEN_ACCESSOR_SETTER) {
			PARSER_MATCH(osic, TOKEN_ACCESSOR_SETTER);
			name = syntax_make_name_node(osic, 6, "setter");
		} else {
			name = syntax_make_name_node(osic, 6, "getter");
		}

		if (!name) {
			return NULL;
		}

		accessor = parser_postfix(osic);
		if (!accessor) {
			return NULL;
		}

		node = syntax_make_accessor_node(osic, name, accessor);
		if (!node) {
			break;
		}

		node->sibling = sibling;
		sibling = node;
	}

	return node;
}

/*
 * accessor : accessor_list var_stmt
 *           | accessor_list define_stmt
 *           | accessor_list class_stmt
 *           ;
 */
static struct syntax *
parser_accessor(struct osic *osic)
{
	struct syntax *var_stmt;
	struct syntax *define_stmt;
	struct syntax *class_stmt;
	struct syntax *accessor_list;
	struct syntax *sibling;

	accessor_list = parser_accessor_list(osic);
	if (!accessor_list) {
		return NULL;
	}

	if (lexer_get_token(osic) == TOKEN_VAR) {
		var_stmt = parser_var_stmt_list(osic);
		if (!var_stmt) {
			return NULL;
		}
		for (sibling = var_stmt; sibling; sibling = sibling->sibling) {
			sibling->u.var_stmt.accessor_list = accessor_list;
		}

		return var_stmt;
	}

	if (lexer_get_token(osic) == TOKEN_DEFINE) {
		define_stmt = parser_define_stmt(osic);
		if (!define_stmt) {
			return NULL;
		}
		define_stmt->u.define_stmt.accessor_list = accessor_list;

		return define_stmt;
	}

	if (lexer_get_token(osic) == TOKEN_CLASS) {
		class_stmt = parser_class_stmt(osic);
		if (!class_stmt) {
			return NULL;
		}
		class_stmt->u.class_stmt.accessor_list = accessor_list;

		return class_stmt;
	}

	return NULL;
}

/*
 * block_stmt : '{' block_stmt_list '}'
 *            ;
 */
/*
 * block_stmt_list : stmt
 *                 | stmt block_stmt_list
 *                 | ';'
 *                 | ';' block_stmt_list
 *                 ;
 */
static struct syntax *
parser_block_stmt(struct osic *osic)
{
	struct syntax *stmt;
	struct syntax *sibling;
	struct syntax *stmt_list;

	sibling = NULL;
	stmt_list = NULL;
	PARSER_MATCH(osic, TOKEN_LBRACE);
	while (lexer_get_token(osic) != TOKEN_RBRACE) {
		/* empty stmt */
		if (lexer_get_token(osic) == TOKEN_SEMICON) {
			PARSER_MATCH(osic, TOKEN_SEMICON);

			continue;
		}

		stmt = parser_stmt(osic);
		if (!stmt) {
			return NULL;
		}

		if (!stmt_list) {
			stmt_list = stmt;
		}

		if (sibling) {
			sibling->sibling = stmt;
		}

		for (; stmt; stmt = stmt->sibling) {
			sibling = stmt;
		}
	}
	PARSER_MATCH(osic, TOKEN_RBRACE);

	return syntax_make_block_node(osic, stmt_list);
}

/*
 * stmt : block_stmt
 *      | var_stmt
 *      | define_stmt
 *      | class_stmt
 *      | try_stmt
 *      | throw_stmt
 *      | if_stmt
 *      | while_stmt
 *      | break_stmt
 *      | continue_stmt
 *      | delete_stmt
 *      | return_stmt
 *      | expr ';'
 *      ;
 */
static struct syntax *
parser_stmt(struct osic *osic)
{
	struct syntax *node;

	switch (lexer_get_token(osic)) {
	case TOKEN_LBRACE:
		node = parser_block_stmt(osic);
		break;

	case TOKEN_IF:
		node = parser_if_stmt(osic);
		break;

	case TOKEN_FOR:
		node = parser_for_stmt(osic);
		break;

	case TOKEN_WHILE:
		node = parser_while_stmt(osic);
		break;

	case TOKEN_VAR:
		node = parser_var_stmt_list(osic);
		break;

	case TOKEN_TRY:
		node = parser_try_stmt(osic);
		break;

	case TOKEN_THROW:
		node = parser_throw_stmt(osic);
		break;

	case TOKEN_BREAK:
		node = parser_break_stmt(osic);
		break;

	case TOKEN_CONTINUE:
		node = parser_continue_stmt(osic);
		break;

	case TOKEN_DELETE:
		node = parser_delete_stmt(osic);
		break;

	case TOKEN_RETURN:
		node = parser_return_stmt(osic);
		break;

	case TOKEN_IMPORT:
		node = parser_import_stmt(osic);
		break;

	case TOKEN_ACCESSOR:
		node = parser_accessor(osic);
		break;

	default:
		node = parser_assign_stmt(osic);
		if (!node) {
			break;
		}

		if (node->kind != SYNTAX_KIND_CLASS_STMT &&
		    node->kind != SYNTAX_KIND_DEFINE_STMT)
		{
			PARSER_MATCH(osic, TOKEN_SEMICON);
		}
		break;
	}

	return node;
}

/*
 * module : module_stmt_list '\0'
 *        ;
 */
/*
 * module_stmt_list : stmt
 *                  | stmt module_stmt_list
 *                  | ';'
 *                  | ';' module_stmt_list
 *                  ;
 */
static struct syntax *
parser_module(struct osic *osic)
{
	struct syntax *stmt;
	struct syntax *sibling;
	struct syntax *stmt_list;

	sibling = NULL;
	stmt_list = NULL;
	while (lexer_get_token(osic) != TOKEN_EOF) {
		/* empty stmt */
		if (lexer_get_token(osic) == TOKEN_SEMICON) {
			PARSER_MATCH(osic, TOKEN_SEMICON);

			continue;
		}

		stmt = parser_stmt(osic);
		if (!stmt) {
			return NULL;
		}

		if (!stmt_list) {
			stmt_list = stmt;
		}

		if (sibling) {
			sibling->sibling = stmt;
		}

		for (; stmt; stmt = stmt->sibling) {
			sibling = stmt;
		}
	}
	PARSER_MATCH(osic, TOKEN_EOF);

	return syntax_make_module_node(osic, stmt_list);
}

struct syntax *
parser_parse(struct osic *osic)
{
	return parser_module(osic);
}

#include "markdown.h"

static int
block_type_kind(int type)
{
    switch (type) {
    case MD_BLOCK_DOC: return MD_NODE_DOCUMENT;
    case MD_BLOCK_QUOTE: return MD_NODE_QUOTE;
    case MD_BLOCK_UL: return MD_NODE_UL;
    case MD_BLOCK_OL: return MD_NODE_OL;
    case MD_BLOCK_LI: return MD_NODE_LI;
    case MD_BLOCK_HR: return MD_NODE_HR;
    case MD_BLOCK_H: return MD_NODE_HEADING;
    case MD_BLOCK_CODE: return MD_NODE_CODE_BLOCK;
    case MD_BLOCK_HTML: return MD_NODE_HTML_BLOCK;
    case MD_BLOCK_P: return MD_NODE_PARAGRAPH;
    case MD_BLOCK_TABLE: return MD_NODE_TABLE;
    case MD_BLOCK_THEAD: return MD_NODE_THEAD;
    case MD_BLOCK_TBODY: return MD_NODE_TBODY;
    case MD_BLOCK_TR: return MD_NODE_TR;
    case MD_BLOCK_TH: return MD_NODE_TH;
    case MD_BLOCK_TD: return MD_NODE_TD;
    default: return MD_NODE_PARAGRAPH;
    }
}

static mrb_sym
block_type_sym(mrb_state *mrb, int type)
{
    switch (type) {
    case MD_BLOCK_DOC: return g_syms.document;
    case MD_BLOCK_QUOTE: return g_syms.quote;
    case MD_BLOCK_UL: return g_syms.ul;
    case MD_BLOCK_OL: return g_syms.ol;
    case MD_BLOCK_LI: return g_syms.li;
    case MD_BLOCK_HR: return g_syms.hr;
    case MD_BLOCK_H: return g_syms.heading;
    case MD_BLOCK_CODE: return g_syms.code_block;
    case MD_BLOCK_P: return g_syms.paragraph;
    case MD_BLOCK_HTML: return mrb_intern_lit(mrb, "html_block");
    case MD_BLOCK_TABLE: return mrb_intern_lit(mrb, "table");
    case MD_BLOCK_THEAD: return mrb_intern_lit(mrb, "thead");
    case MD_BLOCK_TBODY: return mrb_intern_lit(mrb, "tbody");
    case MD_BLOCK_TR: return mrb_intern_lit(mrb, "tr");
    case MD_BLOCK_TH: return mrb_intern_lit(mrb, "th");
    case MD_BLOCK_TD: return mrb_intern_lit(mrb, "td");
    default: return mrb_intern_lit(mrb, "block");
    }
}

static int
span_type_kind(int type)
{
    switch (type) {
    case MD_SPAN_EM: return MD_NODE_EM;
    case MD_SPAN_STRONG: return MD_NODE_STRONG;
    case MD_SPAN_A: return MD_NODE_LINK;
    case MD_SPAN_IMG: return MD_NODE_IMAGE;
    case MD_SPAN_CODE: return MD_NODE_CODE;
    case MD_SPAN_DEL: return MD_NODE_DEL;
    case MD_SPAN_LATEXMATH: return MD_NODE_LATEX_MATH;
    case MD_SPAN_LATEXMATH_DISPLAY: return MD_NODE_LATEX_MATH_DISPLAY;
    case MD_SPAN_WIKILINK: return MD_NODE_WIKILINK;
    case MD_SPAN_U: return MD_NODE_UNDERLINE;
    default: return MD_NODE_EM;
    }
}

static mrb_sym
span_type_sym(mrb_state *mrb, int type)
{
    switch (type) {
    case MD_SPAN_EM: return g_syms.em;
    case MD_SPAN_STRONG: return g_syms.strong;
    case MD_SPAN_A: return g_syms.link;
    case MD_SPAN_CODE: return g_syms.code;
    case MD_SPAN_DEL: return g_syms.del;
    case MD_SPAN_U: return g_syms.underline;
    case MD_SPAN_IMG: return mrb_intern_lit(mrb, "image");
    case MD_SPAN_LATEXMATH: return mrb_intern_lit(mrb, "latex_math");
    case MD_SPAN_LATEXMATH_DISPLAY: return mrb_intern_lit(mrb, "latex_math_display");
    case MD_SPAN_WIKILINK: return mrb_intern_lit(mrb, "wikilink");
    default: return mrb_intern_lit(mrb, "span");
    }
}

static int
text_type_kind(int type)
{
    switch (type) {
    case MD_TEXT_NORMAL: return MD_TEXTK_NORMAL;
    case MD_TEXT_NULLCHAR: return MD_TEXTK_NULLCHAR;
    case MD_TEXT_BR: return MD_TEXTK_BR;
    case MD_TEXT_SOFTBR: return MD_TEXTK_SOFTBR;
    case MD_TEXT_ENTITY: return MD_TEXTK_ENTITY;
    case MD_TEXT_CODE: return MD_TEXTK_CODE;
    default: return MD_TEXTK_NORMAL;
    }
}

static mrb_sym
text_type_sym(mrb_state *mrb, int type)
{
    switch (type) {
    case MD_TEXT_NORMAL: return g_syms.text;
    case MD_TEXT_BR: return g_syms.br;
    case MD_TEXT_SOFTBR: return g_syms.softbreak;
    case MD_TEXT_NULLCHAR: return mrb_intern_lit(mrb, "null_char");
    case MD_TEXT_ENTITY: return mrb_intern_lit(mrb, "entity");
    case MD_TEXT_CODE: return mrb_intern_lit(mrb, "code_text");
    default: return g_syms.text;
    }
}

static mrb_bool
node_has_children(int type, int is_block)
{
    if (is_block && type == MD_BLOCK_HR) return FALSE;
    return TRUE;
}

/* Attach a child node to the current AST parent on the parse stack. */
static void
ast_push_child(ast_ctx *ctx, mrb_value child)
{
    mrb_state *mrb = ctx->mrb;
    mrb_value parent = array_last(mrb, ctx->stack);
    mrb_value children = hash_get(mrb, parent, sym_children(mrb));
    mrb_ary_push(mrb, children, child);
}

/* md4c block callbacks map parser events into AST nodes. */
static void
ast_enter_block(const MD_BLOCK *block, void *userdata)
{
    ast_ctx *ctx = (ast_ctx*)userdata;
    mrb_state *mrb = ctx->mrb;
    mrb_bool with_children = node_has_children(block->type, TRUE);
    mrb_value node;

    if (block->type == MD_BLOCK_DOC) return;
    node = node_new(mrb, block_type_kind(block->type),
                    block_type_sym(mrb, block->type), with_children);

    if (block->level > 0) {
        hash_set(mrb, node, sym_level(mrb), mrb_fixnum_value(block->level));
    }
    if (block->align != 0) {
        hash_set(mrb, node, sym_align(mrb), mrb_fixnum_value((mrb_int)block->align));
    }
    if (block->n_cols > 0) {
        hash_set(mrb, node, sym_n_cols(mrb), mrb_fixnum_value(block->n_cols));
    }

    ast_push_child(ctx, node);
    if (with_children) {
        mrb_ary_push(mrb, ctx->stack, node);
    }
}

/* Closing a nested block/span just pops the current parent off the stack. */
static void
ast_leave_block(const MD_BLOCK *block, void *userdata)
{
    ast_ctx *ctx = (ast_ctx*)userdata;
    if (block->type == MD_BLOCK_DOC) return;
    if (node_has_children(block->type, TRUE)) {
        array_pop(ctx->mrb, ctx->stack);
    }
}

static void
ast_enter_span(const MD_SPAN *span, void *userdata)
{
    ast_ctx *ctx = (ast_ctx*)userdata;
    mrb_state *mrb = ctx->mrb;
    mrb_value node = node_new(mrb, span_type_kind(span->type),
                              span_type_sym(mrb, span->type), TRUE);

    if (span->size > 0) {
        hash_set(mrb, node, sym_size(mrb), mrb_fixnum_value(span->size));
    }
    if (span->href) {
        hash_set(mrb, node, sym_href(mrb), mrb_str_new_cstr(mrb, span->href));
    }
    if (span->title) {
        hash_set(mrb, node, sym_title(mrb), mrb_str_new_cstr(mrb, span->title));
    }
    if (span->label) {
        hash_set(mrb, node, sym_label(mrb), mrb_str_new_cstr(mrb, span->label));
    }
    if (span->child_count > 0) {
        hash_set(mrb, node, sym_child_count(mrb), mrb_fixnum_value(span->child_count));
    }
    if (span->flags != 0) {
        hash_set(mrb, node, sym_flags(mrb), mrb_fixnum_value((mrb_int)span->flags));
    }

    ast_push_child(ctx, node);
    mrb_ary_push(mrb, ctx->stack, node);
}

static void
ast_leave_span(const MD_SPAN *span, void *userdata)
{
    ast_ctx *ctx = (ast_ctx*)userdata;
    (void)span;
    array_pop(ctx->mrb, ctx->stack);
}

static void
ast_text_cb(const MD_TEXT *text, void *userdata)
{
    ast_ctx *ctx = (ast_ctx*)userdata;
    mrb_state *mrb = ctx->mrb;
    mrb_value node = node_new(mrb, MD_NODE_TEXT, g_syms.text, FALSE);

    hash_set(mrb, node, sym_text_kind(mrb), mrb_fixnum_value(text_type_kind(text->type)));
    hash_set(mrb, node, sym_text_type(mrb), mrb_symbol_value(text_type_sym(mrb, text->type)));
    hash_set(mrb, node, sym_text(mrb), mrb_str_new(mrb, text->text, text->size));

    ast_push_child(ctx, node);
}

/* Parse raw markdown into the Ruby AST exposed by the gem. */
static mrb_value
parse_to_ast(mrb_state *mrb, mrb_value text, mrb_int flags)
{
    ast_ctx ctx;
    MD_PARSER parser;
    const char *input = mrb_string_value_ptr(mrb, text);
    size_t input_len = RSTRING_LEN(text);

    memset(&ctx, 0, sizeof(ctx));
    ctx.mrb = mrb;
    ctx.root = node_new(mrb, MD_NODE_DOCUMENT, g_syms.document, TRUE);
    ctx.stack = mrb_ary_new(mrb);
    mrb_ary_push(mrb, ctx.stack, ctx.root);

    memset(&parser, 0, sizeof(parser));
    parser.flags = (int)flags;
    parser.dialect = MD_DIALECT_COMMONMARK;
    parser.enter_block = ast_enter_block;
    parser.leave_block = ast_leave_block;
    parser.enter_span = ast_enter_span;
    parser.leave_span = ast_leave_span;
    parser.text = ast_text_cb;
    parser.userdata = &ctx;

    if (md_parse(input, input_len, &parser, &ctx) != 0) {
        mrb_raise(mrb, E_RUNTIME_ERROR, "markdown parse failed");
    }

    return ctx.root;
}

/*
 * The AST is parsed lazily and cached on the instance.
 *
 * Callers only pay the md4c parse cost once per `Markdown` object.
 */
mrb_value markdown_instance_ast(mrb_state *mrb, mrb_value self) {
    mrb_value ast = mrb_iv_get(mrb, self, iv_ast(mrb));
    if (!mrb_nil_p(ast)) return ast;

    ast = parse_to_ast(mrb,
                       mrb_iv_get(mrb, self, iv_content(mrb)),
                       mrb_fixnum(mrb_iv_get(mrb, self, iv_flags(mrb))));
    mrb_iv_set(mrb, self, iv_ast(mrb), ast);
    return ast;
}

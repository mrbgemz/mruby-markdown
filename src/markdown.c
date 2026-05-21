/*
 * markdown.c — mruby bindings for md4c
 *
 * Provides the Markdown class with:
 *   Markdown.new(text)        => Markdown instance
 *   Markdown.new(text, flags) => Markdown instance with flags
 *   markdown.ast              => AST hash
 */
#include "markdown.h"

markdown_syms g_syms;

static void
markdown_define_flag(mrb_state *mrb, struct RClass *klass, const char *name, mrb_int value)
{
    mrb_define_const(mrb, klass, name, mrb_fixnum_value(value));
}

mrb_int markdown_default_flags(void) {
    return MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH |
           MD_FLAG_UNDERLINE | MD_FLAG_COLLAPSEWHITESPACE;
}

void markdown_init_symbols(mrb_state *mrb) {
    g_syms.document = mrb_intern_lit(mrb, "document");
    g_syms.quote = mrb_intern_lit(mrb, "quote");
    g_syms.ul = mrb_intern_lit(mrb, "ul");
    g_syms.ol = mrb_intern_lit(mrb, "ol");
    g_syms.li = mrb_intern_lit(mrb, "li");
    g_syms.hr = mrb_intern_lit(mrb, "hr");
    g_syms.heading = mrb_intern_lit(mrb, "heading");
    g_syms.code_block = mrb_intern_lit(mrb, "code_block");
    g_syms.paragraph = mrb_intern_lit(mrb, "paragraph");
    g_syms.em = mrb_intern_lit(mrb, "em");
    g_syms.strong = mrb_intern_lit(mrb, "strong");
    g_syms.code = mrb_intern_lit(mrb, "code");
    g_syms.del = mrb_intern_lit(mrb, "del");
    g_syms.underline = mrb_intern_lit(mrb, "underline");
    g_syms.link = mrb_intern_lit(mrb, "link");
    g_syms.text = mrb_intern_lit(mrb, "text");
    g_syms.br = mrb_intern_lit(mrb, "br");
    g_syms.softbreak = mrb_intern_lit(mrb, "softbreak");
}

/* The instance holds source content plus a lazily populated AST cache. */
static mrb_value mrb_markdown_initialize(mrb_state *mrb, mrb_value self) {
    mrb_value text;
    mrb_int flags = markdown_default_flags();

    mrb_get_args(mrb, "S|i", &text, &flags);
    mrb_iv_set(mrb, self, iv_content(mrb), text);
    mrb_iv_set(mrb, self, iv_flags(mrb), mrb_fixnum_value(flags));
    mrb_iv_set(mrb, self, iv_ast(mrb), mrb_nil_value());
    return self;
}

static mrb_value mrb_markdown_ast(mrb_state *mrb, mrb_value self) {
    return markdown_instance_ast(mrb, self);
}

void mrb_mruby_markdown_gem_init(mrb_state *mrb) {
    struct RClass *klass = mrb_define_class(mrb, "Markdown", mrb->object_class);

    markdown_init_symbols(mrb);
    markdown_define_flag(mrb, klass, "COLLAPSE_WHITESPACE", MD_FLAG_COLLAPSEWHITESPACE);
    markdown_define_flag(mrb, klass, "PERMISSIVE_ATX_HEADERS", MD_FLAG_PERMISSIVEATXHEADERS);
    markdown_define_flag(mrb, klass, "PERMISSIVE_URL_AUTOLINKS", MD_FLAG_PERMISSIVEURLAUTOLINKS);
    markdown_define_flag(mrb, klass, "PERMISSIVE_EMAIL_AUTOLINKS", MD_FLAG_PERMISSIVEEMAILAUTOLINKS);
    markdown_define_flag(mrb, klass, "NO_INDENTED_CODE_BLOCKS", MD_FLAG_NOINDENTEDCODEBLOCKS);
    markdown_define_flag(mrb, klass, "NO_HTML_BLOCKS", MD_FLAG_NOHTMLBLOCKS);
    markdown_define_flag(mrb, klass, "NO_HTML_SPANS", MD_FLAG_NOHTMLSPANS);
    markdown_define_flag(mrb, klass, "TABLES", MD_FLAG_TABLES);
    markdown_define_flag(mrb, klass, "STRIKETHROUGH", MD_FLAG_STRIKETHROUGH);
    markdown_define_flag(mrb, klass, "PERMISSIVE_WWW_AUTOLINKS", MD_FLAG_PERMISSIVEWWWAUTOLINKS);
    markdown_define_flag(mrb, klass, "TASK_LISTS", MD_FLAG_TASKLISTS);
    markdown_define_flag(mrb, klass, "LATEX_MATH", MD_FLAG_LATEXMATH);
    markdown_define_flag(mrb, klass, "WIKI_LINKS", MD_FLAG_WIKILINKS);
    markdown_define_flag(mrb, klass, "UNDERLINE", MD_FLAG_UNDERLINE);
    markdown_define_flag(mrb, klass, "DEFAULT_FLAGS", markdown_default_flags());
    mrb_define_method(mrb, klass, "initialize",
                      mrb_markdown_initialize, MRB_ARGS_ARG(1, 1));
    mrb_define_method(mrb, klass, "ast",
                      mrb_markdown_ast, MRB_ARGS_NONE());
}

void mrb_mruby_markdown_gem_final(mrb_state *mrb) {
    (void)mrb;
}

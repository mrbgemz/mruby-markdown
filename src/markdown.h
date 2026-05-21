#ifndef MRUBY_MARKDOWN_INTERNAL_H
#define MRUBY_MARKDOWN_INTERNAL_H

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "md4c/md4c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * AST builder state.
 *
 * The parser produces a Ruby-owned tree of hashes and arrays. `root` is the
 * outer `:document` node, and `stack` tracks the current parent while md4c
 * walks nested blocks and spans.
 */
typedef struct {
    mrb_state *mrb;
    mrb_value root;
    mrb_value stack;
} ast_ctx;

/*
 * Frequently used node and text-type symbols.
 *
 * The AST stores node types as Ruby symbols. Caching the common ones avoids
 * repeated interning and gives the emitters a cheap dispatch path.
 */
typedef struct {
    mrb_sym document;
    mrb_sym quote;
    mrb_sym ul;
    mrb_sym ol;
    mrb_sym li;
    mrb_sym hr;
    mrb_sym heading;
    mrb_sym code_block;
    mrb_sym paragraph;
    mrb_sym em;
    mrb_sym strong;
    mrb_sym code;
    mrb_sym del;
    mrb_sym underline;
    mrb_sym link;
    mrb_sym text;
    mrb_sym br;
    mrb_sym softbreak;
} markdown_syms;

enum markdown_node_type {
    MD_NODE_DOCUMENT = 1,
    MD_NODE_QUOTE,
    MD_NODE_UL,
    MD_NODE_OL,
    MD_NODE_LI,
    MD_NODE_HR,
    MD_NODE_HEADING,
    MD_NODE_CODE_BLOCK,
    MD_NODE_HTML_BLOCK,
    MD_NODE_PARAGRAPH,
    MD_NODE_TABLE,
    MD_NODE_THEAD,
    MD_NODE_TBODY,
    MD_NODE_TR,
    MD_NODE_TH,
    MD_NODE_TD,
    MD_NODE_EM,
    MD_NODE_STRONG,
    MD_NODE_LINK,
    MD_NODE_IMAGE,
    MD_NODE_CODE,
    MD_NODE_DEL,
    MD_NODE_LATEX_MATH,
    MD_NODE_LATEX_MATH_DISPLAY,
    MD_NODE_WIKILINK,
    MD_NODE_UNDERLINE,
    MD_NODE_TEXT
};

enum markdown_text_type {
    MD_TEXTK_NORMAL = 0,
    MD_TEXTK_NULLCHAR,
    MD_TEXTK_BR,
    MD_TEXTK_SOFTBR,
    MD_TEXTK_ENTITY,
    MD_TEXTK_CODE
};

extern markdown_syms g_syms;

static inline mrb_sym sym_type(mrb_state *mrb) { return mrb_intern_lit(mrb, "type"); }
static inline mrb_sym sym_kind(mrb_state *mrb) { return mrb_intern_lit(mrb, "kind"); }
static inline mrb_sym sym_children(mrb_state *mrb) { return mrb_intern_lit(mrb, "children"); }
static inline mrb_sym sym_text(mrb_state *mrb) { return mrb_intern_lit(mrb, "text"); }
static inline mrb_sym sym_text_type(mrb_state *mrb) { return mrb_intern_lit(mrb, "text_type"); }
static inline mrb_sym sym_text_kind(mrb_state *mrb) { return mrb_intern_lit(mrb, "text_kind"); }
static inline mrb_sym sym_level(mrb_state *mrb) { return mrb_intern_lit(mrb, "level"); }
static inline mrb_sym sym_align(mrb_state *mrb) { return mrb_intern_lit(mrb, "align"); }
static inline mrb_sym sym_n_cols(mrb_state *mrb) { return mrb_intern_lit(mrb, "n_cols"); }
static inline mrb_sym sym_href(mrb_state *mrb) { return mrb_intern_lit(mrb, "href"); }
static inline mrb_sym sym_title(mrb_state *mrb) { return mrb_intern_lit(mrb, "title"); }
static inline mrb_sym sym_label(mrb_state *mrb) { return mrb_intern_lit(mrb, "label"); }
static inline mrb_sym sym_size(mrb_state *mrb) { return mrb_intern_lit(mrb, "size"); }
static inline mrb_sym sym_child_count(mrb_state *mrb) { return mrb_intern_lit(mrb, "child_count"); }
static inline mrb_sym sym_flags(mrb_state *mrb) { return mrb_intern_lit(mrb, "flags"); }
static inline mrb_sym iv_content(mrb_state *mrb) { return mrb_intern_lit(mrb, "@content"); }
static inline mrb_sym iv_flags(mrb_state *mrb) { return mrb_intern_lit(mrb, "@flags"); }
static inline mrb_sym iv_ast(mrb_state *mrb) { return mrb_intern_lit(mrb, "@ast"); }

/* Small wrappers keep the hash access pattern consistent throughout the code. */
static inline mrb_value hash_get(mrb_state *mrb, mrb_value hash, mrb_sym key) {
    return mrb_hash_get(mrb, hash, mrb_symbol_value(key));
}

static inline void hash_set(mrb_state *mrb, mrb_value hash, mrb_sym key, mrb_value value) {
    mrb_hash_set(mrb, hash, mrb_symbol_value(key), value);
}

static inline mrb_value array_last(mrb_state *mrb, mrb_value ary) {
    mrb_int len = RARRAY_LEN(ary);
    if (len <= 0) return mrb_nil_value();
    return mrb_ary_ref(mrb, ary, len - 1);
}

static inline void array_pop(mrb_state *mrb, mrb_value ary) {
    mrb_int len = RARRAY_LEN(ary);
    if (len > 0) mrb_ary_pop(mrb, ary);
}

/* Every AST node is a Ruby hash with a `:type` symbol and optional `:children`. */
static inline mrb_value node_new(mrb_state *mrb, mrb_int kind, mrb_sym type, mrb_bool with_children) {
    mrb_value node = mrb_hash_new_capa(mrb, with_children ? 4 : 3);
    hash_set(mrb, node, sym_kind(mrb), mrb_fixnum_value(kind));
    hash_set(mrb, node, sym_type(mrb), mrb_symbol_value(type));
    if (with_children) {
        hash_set(mrb, node, sym_children(mrb), mrb_ary_new(mrb));
    }
    return node;
}

/* All emitters dispatch on the stored node type symbol. */
static inline mrb_sym node_type(mrb_state *mrb, mrb_value node) {
    mrb_value value = hash_get(mrb, node, sym_type(mrb));
    if (!mrb_symbol_p(value)) return 0;
    return mrb_symbol(value);
}

static inline mrb_int node_kind(mrb_state *mrb, mrb_value node) {
    return mrb_fixnum(hash_get(mrb, node, sym_kind(mrb)));
}

void markdown_init_symbols(mrb_state *mrb);
mrb_int markdown_default_flags(void);
mrb_value markdown_instance_ast(mrb_state *mrb, mrb_value self);

#endif

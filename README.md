## About

Markdown AST parser for mruby &ndash; built with [md4c](https://github.com/mity/md4c).

The library produces an AST that can then be rendered
for the browser or other environments like the terminal
but that is left up to the user to do from the AST alone.

The focus on the AST helps keep the core library simple,
understandable, and maintainable. If you build your own renderer
that uses the AST, consider opening a pull request with a link
to your repository.

## Quick Start

```ruby
markdown = Markdown.new("# Hello **world**")
ast = markdown.ast

puts ast[:type]                  # => :document
puts ast[:children][0][:type]    # => :heading
puts ast[:children][0][:level]   # => 1
```

## AST Shape

The root node is always:

```ruby
{ type: :document, children: [...] }
```

Common node fields:

- `:type` for the symbolic node type
- `:kind` for the internal numeric node kind
- `:children` for nested nodes
- `:text` for text-node content

Example:

```ruby
{
  kind: 7,
  type: :heading,
  level: 1,
  children: [
    {
      kind: 27,
      type: :text,
      text_kind: 0,
      text_type: :text,
      text: "Hello world"
    }
  ]
}
```

Tables, links, lists, emphasis, code, and other markdown structures are
represented as nested hashes and arrays in the same style.

## Flags

Pass parser flags as the optional second argument to `Markdown.new`:

```ruby
flags = Markdown::TABLES | Markdown::STRIKETHROUGH
markdown = Markdown.new(text, flags)
```

Useful constants:

- `Markdown::DEFAULT_FLAGS`
- `Markdown::TABLES`
- `Markdown::STRIKETHROUGH`
- `Markdown::UNDERLINE`
- `Markdown::COLLAPSE_WHITESPACE`
- `Markdown::NO_HTML_BLOCKS`
- `Markdown::NO_HTML_SPANS`
- `Markdown::TASK_LISTS`
- `Markdown::LATEX_MATH`
- `Markdown::WIKI_LINKS`

## Source Layout

- `src/markdown.c` defines the `Markdown` class and public API
- `src/ast.c` builds the Ruby AST from md4c callbacks
- `src/md4c/` contains the bundled md4c sources

## License

MIT. Bundled md4c is under its own MIT license.

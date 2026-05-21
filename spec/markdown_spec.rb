# frozen_string_literal: true

describe "Markdown" do
  it "can be instantiated" do
    expect(Markdown.new("hi").class).must_equal Markdown
  end

  it "responds to ast" do
    expect(Markdown.new("hi").respond_to?(:ast)).must_equal true
  end

  it "does not expose html or tui renderers" do
    markdown = Markdown.new("hi")
    expect(markdown.respond_to?(:html)).must_equal false
    expect(markdown.respond_to?(:tui)).must_equal false
  end

  it "exposes parser flags as Markdown constants" do
    expect(Markdown::TABLES).must_equal 0x0100
    expect(Markdown::STRIKETHROUGH).must_equal 0x0200
    expect(Markdown::UNDERLINE).must_equal 0x4000
    expect(Markdown::DEFAULT_FLAGS).must_equal(
      Markdown::TABLES |
      Markdown::STRIKETHROUGH |
      Markdown::UNDERLINE |
      Markdown::COLLAPSE_WHITESPACE
    )
  end

  it "handles empty string" do
    ast = Markdown.new("").ast
    expect(ast[:type]).must_equal :document
    expect(ast[:children]).must_equal []
  end

  it "renders paragraph tree" do
    ast = Markdown.new("Hello world").ast
    expect(ast[:type]).must_equal :document
    expect(ast[:children].length).must_equal 1
    expect(ast[:children][0][:type]).must_equal :paragraph
    expect(ast[:children][0][:children][0][:text]).must_equal "Hello world"
  end

  it "renders heading metadata" do
    ast = Markdown.new("# Title").ast
    heading = ast[:children][0]
    expect(heading[:type]).must_equal :heading
    expect(heading[:level]).must_equal 1
    expect(heading[:children][0][:text]).must_equal "Title"
  end

  it "parses tables with default flags" do
    ast = Markdown.new("| A | B |\n|---|---|\n| 1 | 2 |").ast
    table = ast[:children][0]
    expect(table[:type]).must_equal :table
    expect(table[:n_cols]).must_equal 2
    expect(table[:children][0][:type]).must_equal :thead
    expect(table[:children][1][:type]).must_equal :tbody
  end

  it "does not parse tables when the tables flag is disabled" do
    flags = Markdown::DEFAULT_FLAGS & ~Markdown::TABLES
    ast = Markdown.new("| A | B |\n|---|---|\n| 1 | 2 |", flags).ast
    expect(ast[:children][0][:type]).must_equal :paragraph
  end

  it "preserves inline structure" do
    ast = Markdown.new("**bold** and *italic*").ast
    paragraph = ast[:children][0]
    expect(paragraph[:children][0][:type]).must_equal :strong
    expect(paragraph[:children][0][:children][0][:text]).must_equal "bold"
    expect(paragraph[:children][2][:type]).must_equal :em
    expect(paragraph[:children][2][:children][0][:text]).must_equal "italic"
  end

  it "preserves links" do
    ast = Markdown.new("[text](https://example.com)").ast
    link = ast[:children][0][:children][0]
    expect(link[:type]).must_equal :link
    expect(link[:href]).must_equal "https://example.com"
    expect(link[:children][0][:text]).must_equal "text"
  end

  it "caches the parsed ast" do
    markdown = Markdown.new("**hello** *world*")
    ast = markdown.ast
    expect(markdown.ast).must_equal ast
  end
end

Minitest.run(ARGV) || exit(1)

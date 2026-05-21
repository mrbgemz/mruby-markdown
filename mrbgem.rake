# frozen_string_literal: true

MRuby::Gem::Specification.new("mruby-markdown") do |spec|
  spec.license = "MIT"
  spec.authors = "0x1eef"
  spec.version = "0.1.0"
  spec.description = "CommonMark compliant markdown AST parser for mruby, built on md4c"

  spec.cc.defines << "MD4C_USE_UTF8"

  if ENV["ENV"] == "TEST"
    spec.add_dependency "mruby-minitest", github: "0x1eef/mruby-minitest", branch: "main"
    spec.rbfiles.concat Dir[File.expand_path("spec/*.rb", __dir__)].sort
  end

  spec.rbfiles = Dir[File.expand_path("mrblib/*.rb", __dir__)].sort
end

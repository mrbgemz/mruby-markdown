MRuby::Build.new("mruby-markdown") do |conf|
  profile = ENV.fetch("BUILD_PROFILE", "test")

  conf.toolchain
  conf.gembox "default"

  case profile
  when "test", "developer"
    conf.enable_debug
    ENV["ENV"] = "TEST"
  when "production"
    conf.cc.flags << "-DNDEBUG"
  else
    raise ArgumentError, "unknown BUILD_PROFILE=#{profile.inspect}"
  end

  conf.gem File.expand_path(__dir__)
  conf.gem github: "0x1eef/mruby-process", branch: "main"
end

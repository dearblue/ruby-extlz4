require_relative "lib/extlz4/version"

GEMSTUB = Gem::Specification.new do |s|
  s.name = "extlz4"
  s.version = LZ4::VERSION
  s.summary = "ruby bindings for LZ4"
  s.description = <<EOS
ruby bindings for LZ4 <https://code.google.com/p/lz4/>.
EOS
  s.homepage = "http://sourceforge.jp/projects/rutsubo/"
  s.license = "2-clause BSD License"
  s.author = "dearblue"
  s.email = "dearblue@users.sourceforge.jp"

  s.required_ruby_version = ">= 2.0"
  s.add_development_dependency "rake", "~> 10.0"
end

contrib = FileList["contrib/**/*"]
contrib.reject! { |e| e =~ %r(\bcontrib/lz4/(?:Makefile|appveyor\.yml|contrib|doc|examples|lib/Makefile|lib/dll|programs|tests|visual)(?:$|/)) }

EXTRA.concat(contrib)

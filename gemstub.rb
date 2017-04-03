unless File.read("README.md", 4096) =~ /^\s*\*\s*version:{1,2}\s*(.+)/i
  raise "バージョン情報が README.md に見つかりません"
end

ver = $1


GEMSTUB = Gem::Specification.new do |s|
  s.name = "extlz4"
  s.version = ver
  s.summary = "ruby bindings for LZ4"
  s.description = <<EOS
ruby bindings for LZ4 <https://github.com/lz4/lz4>.
EOS
  s.homepage = "https://github.com/dearblue/extlz4"
  s.license = "BSD-2-Clause"
  s.author = "dearblue"
  s.email = "dearblue@users.noreply.github.com"

  s.required_ruby_version = ">= 2.0"
  s.add_development_dependency "rake", "~> 10.0"
end

contrib = FileList["contrib/**/*"]
contrib.reject! { |e| e =~ %r(\bcontrib/lz4/(?:Makefile|appveyor\.yml|contrib|doc|examples|lib/Makefile|lib/dll|programs|tests|visual)(?:$|/)) }

EXTRA.concat(contrib)

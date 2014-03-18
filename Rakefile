#vim: set fileencoding:utf-8

AUTHOR = "dearblue"
EMAIL = "dearblue@users.sourceforge.jp"
WEBSITE = "http://sourceforge.jp/projects/rutsubo/"
LICENSE = "2-clause BSD License"
PACKAGE = "lz4"
VERSION = "0.1.PRIVATE"
SUMMARY = "ruby binding for lz4"
DESCRIPTION = <<EOS
ruby binding for lz4
EOS

RUBY20 = ENV["RUBY20"]
RUBY21 = ENV["RUBY21"]
RUBYSET = [RUBY20, RUBY21].compact
RUBY_VERSIONS = RUBYSET.map { |ruby| [`#{ruby} -e "puts RbConfig::CONFIG['ruby_version']"`.chomp, ruby] }

EXTCONF = FileList["ext/extconf.rb"]
HASEXT = EXTCONF.empty? ? false : true

BINFILES = FileList["bin/*"]
LIBFILES = FileList["lib/**/*.rb"]
GEMFILE = "#{PACKAGE}-#{VERSION}.gem"
GEMSPEC = "#{PACKAGE}.gemspec"

if HASEXT
  EXTFILES = FileList["ext/**/*"]
  SOFILES_SET = RUBY_VERSIONS.map { |(ver, ruby)| ["lib/#{ver}/#{PACKAGE}.so", ruby] }
  SOFILES = SOFILES_SET.map { |(lib, ruby)| lib }
  PLATFORM_NATIVE = Gem::Platform.local.to_s
  GEMFILE_NATIVE = "#{PACKAGE}-#{VERSION}-#{PLATFORM_NATIVE}.gem"
  GEMSPEC_NATIVE = "#{PACKAGE}-#{PLATFORM_NATIVE}.gemspec"
else
  EXTFILES = []
  SOFILES = []
end

SPECSTUB = Gem::Specification.new do |s|
  s.name = PACKAGE
  s.version = VERSION
  s.summary = SUMMARY
  s.description = DESCRIPTION
  s.homepage = WEBSITE
  s.license = LICENSE
  s.author = AUTHOR
  s.email = EMAIL
  s.executables = BINFILES.map { |n| File.basename n }
  s.files = FileList["{LICENSE{,.txt,.rd,.md},README{,.txt,.rd,.md},Rakefile}"] + FileList["spec/*.rb"] + BINFILES + EXTCONF + EXTFILES + LIBFILES
  s.rdoc_options = %w(--charset UTF-8 --main README.md)
  #s.rdoc_options = %w(--charset UTF-8 --locale ja --main README.md)
  s.has_rdoc = false
  s.required_ruby_version = ">= 1.9.3"
  s.add_development_dependency "rspec", "~> 2.14"
  s.add_development_dependency "rake", "~> 10.0"
end


task :default => :gem
task :gem => (HASEXT ? [GEMFILE_NATIVE, GEMFILE] : GEMFILE)
task :gemspec => GEMSPEC
task :gemfile => GEMFILE

task :rdoc do
  sh "yardoc --charset UTF-8 --locale ja --main README.md README.md LICENSE.md #{(EXTFILES + LIBFILES).join(" ")}"
end


file GEMFILE => [GEMSPEC] + BINFILES + EXTFILES + LIBFILES do
  sh "gem build #{GEMSPEC}"
end

file GEMSPEC => __FILE__ do
  s = SPECSTUB.dup
  s.extensions += EXTCONF
  File.write(GEMSPEC, s.to_ruby, mode: "wb")
end

if HASEXT
  task :gemspec_native => GEMSPEC_NATIVE
  task :gemfile_native => GEMFILE_NATIVE

  file GEMFILE_NATIVE => [GEMSPEC_NATIVE] + BINFILES + EXTFILES + LIBFILES + SOFILES do
    sh "gem build #{GEMSPEC_NATIVE}"
  end

  file GEMSPEC_NATIVE => __FILE__ do
    s = SPECSTUB.dup
    s.files += SOFILES
    s.platform = PLATFORM_NATIVE
    File.write(GEMSPEC_NATIVE, s.to_ruby, mode: "wb")
  end

  SOFILES_SET.each do |(soname, ruby)|
    sodir = File.dirname(soname)
    makefile = File.join(sodir, "Makefile")

    directory sodir

    file soname => [makefile] + EXTFILES do
      cd sodir do
        sh "make"
      end
    end

    file makefile => [sodir] + EXTCONF do
      cd sodir do
        sh "#{ruby} ../../ext/extconf.rb"
      end
    end
  end
end

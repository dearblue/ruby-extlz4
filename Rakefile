#vim: set fileencoding:utf-8

require "rake/clean"

AUTHOR = "dearblue"
EMAIL = "dearblue@users.sourceforge.jp"
WEBSITE = "http://sourceforge.jp/projects/rutsubo/"
LICENSE = "2-clause BSD License"
PACKAGE = "extlz4"
VERSION = "0.1"
SUMMARY = "ruby binding for lz4"
DESCRIPTION = <<EOS
Yet another ruby binding for lz4.
EOS

rubyset = [ENV["RUBY20"], ENV["RUBY21"]].compact
rubyset = rubyset.empty? ? nil : rubyset
RUBYSET = rubyset

EXTCONF = FileList["ext/extconf.rb"]
HASEXT = EXTCONF.empty? ? false : true
SPECFILES = FileList["spec/**/*"]
BINFILES = FileList["bin/*"]
LIBFILES = FileList["lib/**/*.rb"]
GEMFILE = "#{PACKAGE}-#{VERSION}.gem"
GEMSPEC = "#{PACKAGE}.gemspec"
EXTFILES = HASEXT ? FileList["ext/**/*"] : []

CLOBBER << GEMFILE << GEMSPEC

if RUBYSET
  RUBY_VERSIONS = RUBYSET.map { |ruby| [`#{ruby} --disable gem -rrbconfig -e "puts RbConfig::CONFIG['ruby_version']"`.chomp, ruby] }
  SOFILES_SET = RUBY_VERSIONS.map { |(ver, ruby)| ["lib/#{ver}/#{PACKAGE}.so", ruby] }
  SOFILES = SOFILES_SET.map { |(lib, ruby)| lib }
  PLATFORM_NATIVE = Gem::Platform.local.to_s
  GEMFILE_NATIVE = "#{PACKAGE}-#{VERSION}-#{PLATFORM_NATIVE}.gem"
  GEMSPEC_NATIVE = "#{PACKAGE}-#{PLATFORM_NATIVE}.gemspec"
else
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
  s.files = FileList["LICENSE{,.txt,.rd,.md},README{,.txt,.rd,.md}"] +
            FileList["spec/*.rb"] +
            BINFILES + EXTCONF + EXTFILES + LIBFILES + SPECFILES
  if HASEXT
    rdocextfiles = FileList["ext/**/*.{h,hh,c,cc,cpp,cxx}"]
    rdocextfiles.reject! { |n| n.include?("/externals/") }
    rdocextfiles += FileList["ext/**/{LICENSE,README}{,.txt,.rd,.md}"]
  else
    rdocextfiles = []
  end
  s.extra_rdoc_files = FileList["{LICENSE,README}{,.txt,.rd,.md}"] +
                       LIBFILES + rdocextfiles
  s.rdoc_options = %w(--charset UTF-8 --main README.md)
  #s.rdoc_options = %w(--charset UTF-8 --locale ja --main README.md)
  s.has_rdoc = false
  s.required_ruby_version = ">= 2.0"
  s.add_development_dependency "rspec", "~> 2.14"
  s.add_development_dependency "rake", "~> 10.0"
end


task :default => :gem

desc "Make gemfile"
task :gem => (RUBYSET ? [GEMFILE_NATIVE, GEMFILE] : GEMFILE)

desc "Make general gemspec file"
task :gemspec => GEMSPEC

desc "Make general gemfile"
task :gemfile => GEMFILE

desc "Make rdoc"
task :rdoc do
  sh "rdoc --charset UTF-8 --locale ja --main README.md #{SPECSTUB.extra_rdoc_files.join(" ")}"
end


file GEMFILE => [GEMSPEC] + BINFILES + EXTFILES + LIBFILES + SPECFILES do
  sh "gem build #{GEMSPEC}"
end

file GEMSPEC => __FILE__ do
  s = SPECSTUB.dup
  s.extensions += EXTCONF
  File.write(GEMSPEC, s.to_ruby, mode: "wb")
end

if RUBYSET
  CLOBBER << GEMSPEC_NATIVE << GEMFILE_NATIVE

  desc "Make native gemspec file"
  task :gemspec_native => GEMSPEC_NATIVE

  desc "Make native gemfile"
  task :gemfile_native => GEMFILE_NATIVE

  file GEMFILE_NATIVE => [GEMSPEC_NATIVE] + BINFILES + EXTFILES + LIBFILES + SPECFILES + SOFILES do
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

    CLEAN << sodir

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

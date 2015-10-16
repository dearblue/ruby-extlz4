
require "rake/clean"

DOC = FileList["{README,LICENSE,CHANGELOG,Changelog,HISTORY}{,.ja}{,.txt,.rd,.rdoc,.md,.markdown}"] +
      FileList["{contrib,ext}/**/{README,LICENSE,CHANGELOG,Changelog,HISTORY}{,.ja}{,.txt,.rd,.rdoc,.md,.markdown}"] +
      FileList["ext/**/*.{c,C,cc,cxx,cpp,h,H,hh}"]
#EXT = FileList["ext/**/*.{h,hh,c,cc,cpp,cxx}"] +
#      FileList["ext/externals/**/*"]
EXT = FileList["ext/**/*"]
BIN = FileList["bin/*"]
LIB = FileList["lib/**/*.rb"]
SPEC = FileList["spec/**/*"]
TEST = FileList["test/**/*"]
EXAMPLE = FileList["examples/**/*"]
GEMSTUB_SRC = "gemstub.rb"
RAKEFILE = [File.basename(__FILE__), GEMSTUB_SRC]
EXTRA = []

load GEMSTUB_SRC

EXTCONF = FileList["ext/extconf.rb"]
EXTCONF.reject! { |n| !File.file?(n) }
GEMSTUB.extensions += EXTCONF
GEMSTUB.executables += FileList["bin/*"].map { |n| File.basename n }
GEMSTUB.executables.sort!

GEMFILE = "#{GEMSTUB.name}-#{GEMSTUB.version}.gem"
GEMSPEC = "#{GEMSTUB.name}.gemspec"

GEMSTUB.files += DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + RAKEFILE + EXTRA
GEMSTUB.files.sort!
if GEMSTUB.rdoc_options.nil? || GEMSTUB.rdoc_options.empty?
  readme = %W(.md .markdown .rd .rdoc .txt #{""}).map { |ext| "README#{ext}" }.find { |m| DOC.find { |n| n == m } }
  GEMSTUB.rdoc_options = %w(--charset UTF-8) + (readme ? %W(-m #{readme}) : [])
end
GEMSTUB.extra_rdoc_files += DOC + LIB + EXT.reject { |n| n.include?("/externals/") || !%w(.h .hh .c .cc .cpp .cxx).include?(File.extname(n)) }
GEMSTUB.extra_rdoc_files.sort!

CLEAN << GEMSPEC
CLOBBER << GEMFILE

task :default => :all


unless EXTCONF.empty?
  RUBYSET ||= (ENV["RUBYSET"] || "").split(",")

  if RUBYSET.nil? || RUBYSET.empty?
    $stderr.puts <<-EOS
#{__FILE__}:
|
| If you want binary gem package, launch rake with ``RUBYSET`` enviroment
| variable for set ruby interpreters by comma separated.
|
|   e.g.) $ rake RUBYSET=ruby
|     or) $ rake RUBYSET=ruby20,ruby21,ruby22
|
    EOS
  else
    platforms = RUBYSET.map { |ruby| `#{ruby} --disable gems -rrbconfig -e "puts RbConfig::CONFIG['arch']"`.chomp }
    platforms1 = platforms.uniq
    unless platforms1.size == 1 && !platforms1[0].empty?
      raise "different platforms (#{Hash[*RUBYSET.zip(platforms).flatten].inspect})"
    end
    PLATFORM = platforms1[0]

    RUBY_VERSIONS = RUBYSET.map do |ruby|
      ver = `#{ruby} --disable gem -rrbconfig -e "puts RbConfig::CONFIG['ruby_version']"`.slice(/\d+\.\d+/)
      raise "failed ruby checking - ``#{ruby}''" unless $?.success?
      [ver, ruby]
    end
    SOFILES_SET = RUBY_VERSIONS.map { |(ver, ruby)| ["lib/#{ver}/#{GEMSTUB.name}.so", ruby] }
    SOFILES = SOFILES_SET.map { |(lib, ruby)| lib }

    GEMSTUB_NATIVE = GEMSTUB.dup
    GEMSTUB_NATIVE.files += SOFILES
    GEMSTUB_NATIVE.platform = Gem::Platform.new(PLATFORM).to_s
    GEMSTUB_NATIVE.extensions.clear
    GEMFILE_NATIVE = "#{GEMSTUB_NATIVE.name}-#{GEMSTUB_NATIVE.version}-#{GEMSTUB_NATIVE.platform}.gem"
    GEMSPEC_NATIVE = "#{GEMSTUB_NATIVE.name}-#{GEMSTUB_NATIVE.platform}.gemspec"

    task :all => ["native-gem", GEMFILE]

    desc "build binary gem package"
    task "native-gem" => GEMFILE_NATIVE

    desc "generate binary gemspec"
    task "native-gemspec" => GEMSPEC_NATIVE

    file GEMFILE_NATIVE => DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + SOFILES + RAKEFILE + [GEMSPEC_NATIVE] do
      sh "gem build #{GEMSPEC_NATIVE}"
    end

    file GEMSPEC_NATIVE => RAKEFILE do
      File.write(GEMSPEC_NATIVE, GEMSTUB_NATIVE.to_ruby, mode: "wb")
    end

    desc "build c-extension libraries"
    task "sofiles" => SOFILES

    SOFILES_SET.each do |(soname, ruby)|
      sodir = File.dirname(soname)
      makefile = File.join(sodir, "Makefile")

      CLEAN << GEMSPEC_NATIVE << sodir
      CLOBBER << GEMFILE_NATIVE

      directory sodir

      desc "generate Makefile for binary extension library"
      file makefile => [sodir] + EXTCONF do
        cd sodir do
          sh "#{ruby} ../../#{EXTCONF[0]} \"--ruby=#{ruby}\""
        end
      end

      desc "build binary extension library"
      file soname => [makefile] + EXT do
        cd sodir do
          sh "make"
        end
      end
    end
  end
end


task :all => GEMFILE

desc "generate local rdoc"
task :rdoc => DOC + LIB do
  sh *(%w(rdoc) + GEMSTUB.rdoc_options + DOC + LIB)
end

desc "launch rspec"
task rspec: :all do
  sh "rspec"
end

desc "build gem package"
task gem: GEMFILE

desc "generate gemspec"
task gemspec: GEMSPEC

file GEMFILE => DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + RAKEFILE + [GEMSPEC] do
  sh "gem build #{GEMSPEC}"
end

file GEMSPEC => RAKEFILE do
  File.write(GEMSPEC, GEMSTUB.to_ruby, mode: "wb")
end

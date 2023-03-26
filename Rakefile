
require "pathname"
require "rake/clean"

docnames = "{README,LICENSE,CHANGELOG,Changelog,HISTORY}"
doctypes = "{,.txt,.rd,.rdoc,.md,.markdown}"
cexttypes = "{c,C,cc,cxx,cpp,h,H,hh}"

DOC = FileList["#{docnames}{,.ja}#{doctypes}"] +
      FileList["{contrib,ext}/**/#{docnames}{,.ja}#{doctypes}"] +
      FileList["ext/**/*.#{cexttypes}"]
EXT = FileList["ext/**/*"]
BIN = FileList["bin/*"]
LIB = FileList["lib/**/*.rb"]
SPEC = FileList["spec/**/*"]
TEST = FileList["test/**/*"]
EXAMPLE = FileList["examples/**/*"]
GEMSTUB_SRC = "gemstub.rb"
RAKEFILE = [File.basename(__FILE__), GEMSTUB_SRC]
EXTRA = []
EXTCONF = FileList["ext/**/extconf.rb"]
EXTCONF.reject! { |n| !File.file?(n) }
EXTMAP = {}

load GEMSTUB_SRC

EXTMAP.dup.each_pair do |dir, name|
  EXTMAP[Pathname.new(dir).cleanpath.to_s] = Pathname.new(name).cleanpath.to_s
end

GEMSTUB.extensions += EXTCONF
GEMSTUB.executables += FileList["bin/*"].map { |n| File.basename n }
GEMSTUB.executables.sort!

PACKAGENAME = "#{GEMSTUB.name}-#{GEMSTUB.version}"
GEMFILE = "#{PACKAGENAME}.gem"
GEMSPEC = "#{PACKAGENAME}.gemspec"
BUNDLER_GEMFILE = "Gemfile"

GEMSTUB.files += DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + RAKEFILE + EXTRA
GEMSTUB.files.sort!
if GEMSTUB.rdoc_options.nil? || GEMSTUB.rdoc_options.empty?
  readme = %W(.md .markdown .rd .rdoc .txt #{""}).map { |ext| "README#{ext}" }.find { |m| DOC.find { |n| n == m } }
  GEMSTUB.rdoc_options = %w(--charset UTF-8) + (readme ? %W(-m #{readme}) : [])
end
GEMSTUB.extra_rdoc_files += DOC + LIB + EXT.reject { |n| n.include?("/externals/") || !%w(.h .hh .c .cc .cpp .cxx).include?(File.extname(n)) }
GEMSTUB.extra_rdoc_files.sort!

GEMSTUB_TRYOUT = GEMSTUB.dup
GEMSTUB_TRYOUT.version = "#{GEMSTUB.version}#{Time.now.strftime(".TRYOUT.%Y%m%d.%H%M%S")}"
PACKAGENAME_TRYOUT = "#{GEMSTUB.name}-#{GEMSTUB_TRYOUT.version}"
GEMFILE_TRYOUT = "#{PACKAGENAME_TRYOUT}.gem"
GEMSPEC_TRYOUT = "#{PACKAGENAME_TRYOUT}.gemspec"

CLEAN << GEMSPEC << GEMSPEC_TRYOUT
CLOBBER << GEMFILE

task :default => :tryout do
  $stderr.puts <<-EOS
#{__FILE__}:#{__LINE__}:
\ttype ``rake release'' to build release package.
  EOS
end

desc "build tryout package"
task :tryout

desc "build release package"
task :release => :all

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
|     or) $ rake RUBYSET=ruby21,ruby22,ruby23
|
    EOS
  else
    platforms = RUBYSET.map { |ruby| `#{ruby} --disable-gems -e "puts RUBY_PLATFORM"`.chomp }
    platforms1 = platforms.uniq
    unless platforms1.size == 1 && !platforms1[0].empty?
      abort <<-EOS
#{__FILE__}:#{__LINE__}: different platforms:
#{RUBYSET.zip(platforms).map { |ruby, platform| "%24s => %s" % [ruby, platform] }.join("\n")}
ABORTED.
      EOS
    end
    PLATFORM = platforms1[0]

    RUBY_VERSIONS = RUBYSET.map do |ruby|
      ver = `#{ruby} --disable-gems -e "puts RUBY_VERSION"`.slice(/\d+\.\d+/)
      raise "failed ruby checking - ``#{ruby}''" unless $?.success?
      [ver, ruby]
    end

    SOFILES_SET = RUBY_VERSIONS.map { |(ver, ruby)|
      EXTCONF.map { |extconf|
        extdir = Pathname.new(extconf).cleanpath.dirname.to_s
        case
        when soname = EXTMAP[extdir.sub(/^ext\//i, "")]
          soname = soname.sub(/\.so$/i, "")
        when extdir == "ext" || extdir == "."
          soname = GEMSTUB.name
        else
          soname = File.basename(extdir)
        end

        [ruby, File.join("lib", "#{soname.sub(/(?<=\/)|^(?!.*\/)/, "#{ver}/")}.so"), extconf]
      }
    }.flatten(1)
    SOFILES = SOFILES_SET.map { |(ruby, sopath, extconf)| sopath }

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

    SOFILES_SET.each do |(ruby, soname, extconf)|
      sodir = File.dirname(soname)
      makefile = File.join(sodir, "Makefile")

      CLEAN << GEMSPEC_NATIVE << sodir
      CLOBBER << GEMFILE_NATIVE

      directory sodir

      desc "generate Makefile for binary extension library"
      file makefile => [sodir, extconf] do
        rel_extconf = Pathname.new(extconf).relative_path_from(Pathname.new(sodir)).to_s
        cd sodir do
          sh *%W"#{ruby} #{rel_extconf} --ruby=#{ruby} #{ENV["EXTCONF"]}"
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
task :tryout => GEMFILE_TRYOUT

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

desc "print package name"
task "package-name" do
  puts PACKAGENAME
end

desc "generate Gemfile for bundler"
task "gemfile" => BUNDLER_GEMFILE

file GEMFILE => DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + RAKEFILE + [GEMSPEC] do
  sh "gem build #{GEMSPEC}"
end

file GEMFILE_TRYOUT => DOC + EXT + EXTCONF + BIN + LIB + SPEC + TEST + EXAMPLE + RAKEFILE + [GEMSPEC_TRYOUT] do
#file GEMFILE_TRYOUT do
  sh "gem build #{GEMSPEC_TRYOUT}"
end

file GEMSPEC => RAKEFILE do
  File.write(GEMSPEC, GEMSTUB.to_ruby, mode: "wb")
end

file GEMSPEC_TRYOUT => RAKEFILE do
  File.write(GEMSPEC_TRYOUT, GEMSTUB_TRYOUT.to_ruby, mode: "wb")
end

file BUNDLER_GEMFILE => RAKEFILE do
  File.write(BUNDLER_GEMFILE, <<~GemfileSource, mode: "wb")
    source "https://rubygems.org/"

    group "default" do
    #{GEMSTUB.runtime_dependencies.map { |e|
      %(  gem #{e.name.inspect}, #{e.requirements_list.map { |ee| ee.inspect }.join(", ")})
    }.join("\n")}
    end

    group "development", "test" do
    #{GEMSTUB.development_dependencies.map { |e|
      %(  gem #{e.name.inspect}, #{e.requirements_list.map { |ee| ee.inspect }.join(", ")})
    }.join("\n")}
    end
  GemfileSource
end

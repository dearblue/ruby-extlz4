#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

# TODO: システムにインストールされた lz4 がある場合、バージョンを確認してより新しければそちらを利用する

$srcs = Dir.glob(File.join(File.dirname(__FILE__).gsub(/[\[\{\?\*]/, "[\\0]"), "{.,../contrib/lz4/lib}/*.c")).map { |n| File.basename n }
$VPATH << "$(srcdir)/../contrib/lz4/lib"
$CPPFLAGS << " -I$(srcdir)/../contrib/lz4/lib"

if RbConfig::CONFIG["arch"] =~ /mingw/
  $LDFLAGS << " -static-libgcc"
end

create_makefile("extlz4")

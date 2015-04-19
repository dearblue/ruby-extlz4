#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

# TODO: システムにインストールされた lz4 がある場合、バージョンを確認してより新しければそちらを利用する

$srcs = Dir.glob(File.join(File.dirname(__FILE__).gsub(/[\[\{\?\*]/, "[\\0]"), "{.,../contrib/*}/*.c")).map { |n| File.basename n }
$VPATH << "$(srcdir)/../contrib/lz4"
find_header "lz4.h", "$(srcdir)/../contrib/lz4" or abort 1
find_header "xxhash.h", "$(srcdir)/../contrib/lz4" or abort 1

if RbConfig::CONFIG["arch"] =~ /mingw/
  $LDFLAGS << " -static-libgcc"
end

create_makefile("extlz4")

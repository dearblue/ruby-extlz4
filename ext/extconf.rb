#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

# TODO: システムにインストールされた lz4 がある場合、バージョンを確認してより新しければそちらを利用する

$srcs = Dir.glob(File.join(File.dirname(__FILE__).gsub(/[\[\{\?\*]/, "[\\0]"), "{.,externals/lz4}/*.c")).map { |n| File.basename n }
$VPATH.insert(1, "$(srcdir)/externals/lz4")

create_makefile("extlz4")

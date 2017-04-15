#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

# TODO: システムにインストールされた lz4 がある場合、バージョンを確認してより新しければそちらを利用する

append_cppflags "-I$(srcdir)/../contrib/lz4/lib"

if RbConfig::CONFIG["arch"] =~ /mingw/
  $LDFLAGS << " -static-libgcc"
end

create_makefile File.join(RUBY_VERSION[/\d+\.\d+/], "extlz4")

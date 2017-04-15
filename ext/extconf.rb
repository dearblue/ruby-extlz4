#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

# TODO: システムにインストールされた lz4 がある場合、バージョンを確認してより新しければそちらを利用する

append_cppflags "-I$(srcdir)/../contrib/lz4/lib"

if RbConfig::CONFIG["arch"] =~ /mingw/
  append_ldflags "-static-libgcc"
else
  if try_compile(%q(__attribute__ ((visibility("default"))) void testfunc(void) { }))
    if append_cflags "-fvisibility=hidden"
      localsymbol = true
    end
  end
end

if localsymbol
  $defs << %q('-DRBEXT_API=__attribute__ ((visibility("default")))') <<
           %q(-DRBEXT_VISIBILITY=1)
else
  $defs << %q(-DRBEXT_API)
end

create_makefile File.join(RUBY_VERSION[/\d+\.\d+/], "extlz4")

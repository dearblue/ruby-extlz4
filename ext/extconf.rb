#!ruby
#vim: set fileencoding:utf-8

require "mkmf"

module MyExtensions
  refine Object do
    # ruby-2.3 から追加されたメソッドの確認と追加

    unless Object.method_defined?(:append_cppflags)
      def append_cppflags(flags)
        return false unless try_cppflags(flags)
        $CPPFLAGS << " #{flags}"
        true
      end
    end

    unless Object.method_defined?(:append_cflags)
      def append_cflags(flags)
        return false unless try_cflags(flags)
        $CFLAGS << " #{flags}"
        true
      end
    end

    unless Object.method_defined?(:append_ldflags)
      def append_ldflags(flags)
        return false unless try_ldflags(flags)
        $LDFLAGS << " #{flags}"
        true
      end
    end
  end
end

using MyExtensions


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

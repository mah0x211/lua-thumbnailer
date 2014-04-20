package = "thumbnailer"
version = "scm-1"
source = {
    url = "git://github.com/mah0x211/lua-thumbnailer.git"
}
description = {
    summary = "thumbnail generator",
    detailed = [[]],
    homepage = "https://github.com/mah0x211/lua-thumbnailer", 
    license = "MIT/X11",
    maintainer = "Masatoshi Teruya"
}
dependencies = {
    "lua >= 5.1"
}
external_dependencies = {
    IMLIB2 = {
        header = "Imlib2.h"
    }
}
build = {
    type = "builtin",
    modules = {
        thumbnailer = {
            sources = { "thumbnailer.c" },
            libraries = { "Imlib2" },
            incdirs = { 
                "$(IMLIB2_INCDIR)"
            },
            libdirs = { 
                "$(IMLIB2_LIBDIR)"
            }
        }
    }
}


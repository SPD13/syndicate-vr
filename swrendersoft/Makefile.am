ACLOCAL_AMFLAGS = --install -I m4

lib_LIBRARIES = libswworld.a libswrender.a

libswrender_a_headers_src = \
  include/app_box.h \
  include/app_gentab.h \
  include/app_line.h \
  include/app_sprite.h \
  include/app_text_ba.h \
  include/app_text_cw.h \
  include/app_text_sf.h \
  include/drawshape.h \
  include/drawtext.h \
  include/enginbckt.h \
  include/engincam.h \
  include/engincolour.h \
  include/engindrwlstm.h \
  include/engindrwlstx.h \
  include/enginfexpl.h \
  include/enginfloor.h \
  include/enginlights.h \
  include/enginpeff.h \
  include/enginpriobjs.h \
  include/enginpritxtr.h \
  include/enginprops.h \
  include/enginshadws.h \
  include/enginshrapn.h \
  include/enginsngobjs.h \
  include/enginsngtxtr.h \
  include/engintrns.h \
  include/engintxtrmap.h \
  include/enginzoom.h \
  include/frame_sprani.h \
  include/holodrwlstx.h \
  include/huddrwlstm.h \
  include/huddrwlstx.h \
  include/privrdlog.h \
  include/render_gpoly.h \
  include/sprfontut.h

libswrender_a_headers_bld = \
  include/swrendconf.h

libswrender_a_SOURCES = \
  $(libswrender_a_headers_src) \
  src/ap_gspr_map.c \
  src/ap_spr_smap.c \
  src/app_box.c \
  src/app_box_lt.c \
  src/app_box_tx.c \
  src/app_gengh.c \
  src/app_hvline_lt.c \
  src/app_spr_init.c \
  src/app_text_ba.c \
  src/app_text_cw.c \
  src/app_text_sf.c \
  src/drawshape.c \
  src/drawtext.c \
  src/enginbckt.c \
  src/engindrwlstm.c \
  src/engindrwlstm_3d.c \
  src/engindrwlstx.c \
  src/engindrwlstx_fac.c \
  src/engindrwlstx_spr.c \
  src/enginfexpldrw.c \
  src/enginpeffdrw.c \
  src/enginshadws.c \
  src/holodrwlstx.c \
  src/huddrwlstm.c \
  src/huddrwlstx.c \
  src/render_gpoly.c \
  src/sprfontut.c

libswrender_a_CPPFLAGS = \
  -I"$(top_srcdir)/include" -I"$(builddir)/include"

# The world data of the engine, which the game core uses without drawing:
# objects, faces, textures, lights, the camera and zoom, explosion faces,
# shrapnel, animation frames, the scene effect, the buffers the game
# allocates for the renderer. libswrender (the drawing) needs it.
libswworld_a_SOURCES = \
  $(libswrender_a_headers_src) \
  src/engincam.c \
  src/engincolour.c \
  src/engindrwbuf.c \
  src/enginfexpl.c \
  src/enginfloor.c \
  src/enginlights.c \
  src/enginpeff.c \
  src/enginpriobjs.c \
  src/enginpritxtr.c \
  src/enginprops.c \
  src/enginshrapn.c \
  src/enginsngobjs.c \
  src/enginsngtxtr.c \
  src/engintrns.c \
  src/engintxtrmap.c \
  src/enginzoom.c \
  src/frame_sprani.c
libswworld_a_CPPFLAGS = $(libswrender_a_CPPFLAGS)

libswrenderheadersdir = $(includedir)/swrender
libswrenderheaders_HEADERS = \
  $(libswrender_a_headers_src:%=$(srcdir)/%) \
  $(libswrender_a_headers_bld:%=$(builddir)/%)

# Libraries included with the project
libswrender_a_PROJECT_LIBS = ../lib/libbullfrog.a

$(libswrender_a_OBJECTS) $(libswworld_a_OBJECTS) $(lib_LIBRARIES): $(libswrender_a_PROJECT_LIBS)

../lib/libbullfrog.a: ../bflibrary/libbullfrog.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

bin_PROGRAMS = syndwarsfx
bindir = $(prefix)/$(PACKAGE)

MKW = $(PYTHON) $(top_srcdir)/util/mkwrappers

# The game is a library, libswarscore (interface: swcore.h), which front
# ends link; syndwarsfx is the SDL desktop front end, its main() only runs
# the library turn by turn.
noinst_LIBRARIES = libswarscore.a

syndwarsfx_SOURCES = \
	swcore.h \
	swmain.c

libswarscore_a_SOURCES = \
	bflib_vidraw.c \
	bflib_vidraw.h \
	../bfariadne/include/trfringe.h \
	../bfariadne/src/trfringe.c \
	../bfariadne/include/triangls.h \
	../bfariadne/src/triangls.c \
	../bfariadne/include/tringops.h \
	../bfariadne/src/tringops.c \
	../bfariadne/include/trpath.h \
	../bfariadne/include/trpoints.h \
	../bfariadne/src/trpoints.c \
	../bfariadne/src/trfind8.c \
	../bfariadne/include/trfind8.h \
	../bfariadne/src/trstate.c \
	../bfariadne/include/trstate.h \
	../bfariadne/src/delaunay.c \
	../bfariadne/include/delaunay.h \
	../bfsmacklib/include/bfsmack.h \
	../bfsmacklib/src/bfsmack.c \
	../bfsmacklib/include/smack.h \
	../bfsmacklib/include/smack2ail.h \
	../bfsmacklib/src/smailfile.c \
	../bfsmacklib/include/smkdec.h \
	../bfsmacklib/src/smkdec.c \
	config.h \
	bmbang.c \
	bmbang.h \
	bmcrater.c \
	bmcrater.h \
	bat.c \
	bat.h \
	bigmap.c \
	bigmap.h \
	building.c \
	building.h \
	campaign.c \
	campaign.h \
	command.c \
	command.h \
	crtrand.c \
	crtrand.h \
	cybmod.c \
	cybmod.h \
	devcreds.c \
	devcreds.h \
	display.c \
	display.h \
	difftest.c \
	difftest.h \
	difftest_bot.c \
	difftest_fuzz.c \
	difftest_hook_misc.c \
	difftest_hook_people.c \
	difftest_hook_transport.c \
	dos.c \
	dos.h \
	drawtext_wrp.c \
	drawtext_wrp.h \
	embedanim.c \
	embedanim.h \
	engindrwlstm_wrp.c \
	engindrwlstm_wrp.h \
	engindrwlstx_tng.c \
	engindrwlstx_tng.h \
	feappbar.c \
	feappbar.h \
	febrief.c \
	febrief.h \
	fedebrief.c \
	fedebrief.h \
	fecntrls.c \
	fecntrls.h \
	fecryo.c \
	fecryo.h \
	feequip.c \
	feequip.h \
	feoptions.c \
	feoptions.h \
	fepanet.c \
	fepanet.h \
	feshared.c \
	feshared.h \
	felogin.c \
	felogin.h \
	femail.c \
	femail.h \
	femain.c \
	femain.h \
	fenet.c \
	fenet.h \
	fepause.c \
	fepause.h \
	feresearch.c \
	feresearch.h \
	festorage.c \
	festorage.h \
	feworld.c \
	feworld.h \
	fmvplay.c \
	fmvplay.h \
	guiboxes.c \
	guiboxes.h \
	guigraph.c \
	guigraph.h \
	guitext.c \
	guitext.h \
	game.c \
	game_world.c \
	game_world.h \
	game_state.c \
	swfe.h \
	game_bstype.h \
	game_data.c \
	game_data.h \
	game_options.c \
	game_options.h \
	game_save.c \
	game_save.h \
	game_speed.c \
	game_speed.h \
	game_sprts.c \
	game_sprts.h \
	game.h \
	hud_draw.c \
	hud_draw.h \
	hud_panecfg.c \
	hud_panecfg.h \
	hud_panel.c \
	hud_panel.h \
	hud_target.c \
	hud_target.h \
	keyboard.c \
	keyboard.h \
	lvdraw3d.c \
	lvdraw3d.h \
	lvobjctv.c \
	lvobjctv.h \
	lvfiles.c \
	lvfiles.h \
	lvwalk.c \
	lvwalk.h \
	matrix.c \
	matrix.h \
	misstat.c \
	misstat.h \
	mouse.c \
	mouse.h \
	mydraw.c \
	mydraw.h \
	network.c \
	network.h \
	pathtrig.c \
	pathtrig_debug.c \
	pathtrig.h \
	packet.c \
	packet.h \
	packetfe.c \
	packetfe.h \
	people.c \
	people.h \
	pepgroup.c \
	pepgroup.h \
	player.c \
	player.h \
	plyr_net.c \
	plyr_net.h \
	plyr_packet.c \
	plyr_packet.h \
	plyr_usrinp.c \
	plyr_usrinp.h \
	plyr_usrstate.c \
	purpldrw.c \
	purpldrw.h \
	purpldrwlst.c \
	purpldrwlst.h \
	research.c \
	research.h \
	rules.c \
	rules.h \
	scanner.c \
	scanner.h \
	scandraw.c \
	scandraw.h \
	sound.c \
	sound.h \
	specblit.c \
	specblit.h \
	swapp.c \
	swcore.c \
	swcore.h \
	swcore_game.h \
	swcore_world.c \
	swlog.h \
	thing.c \
	thing.h \
	thing_expld.c \
	thing_expld.h \
	thing_search.c \
	thing_search.h \
	thing_fire.c \
	thing_fire.h \
	thing_debug.c \
	thing_onface.h \
	thing_onface.c \
	thing_view.c \
	thing_view.h \
	thing_ovmous.h \
	thing_ovmous.c \
	tngcolisn.c \
	tngcolisn.h \
	tngobjdrw.c \
	tngobjdrw.h \
	osunix.c \
	osunix.h \
	util.c \
	util.h \
	vehicle.c \
	vehicle.h \
	vehtraffic.c \
	vehtraffic.h \
	wadfile.c \
	wadfile.h \
	weapon.c \
	weapon.h \
	oswindws.c \
	oswindws.h \
	wrcities.c \
	wrcities.h

libswarscore_a_CPPFLAGS = \
  -I"$(top_srcdir)/bfsmacklib/include" \
  -I"$(top_srcdir)/swrendersoft/include" \
  -I"$(top_srcdir)/bfariadne/include"

syndwarsfx_CPPFLAGS = $(libswarscore_a_CPPFLAGS)

# Watcom-to-C wrappers (ac_*) are called only from asm, which builds
# without difftest and DOS network do not have
if ASM_WRAPPERS
syndwarsfx_WRAPPERS = wrappers_dos.o wrappers_game.o wrappers_libc.o wrappers_util.o
else
syndwarsfx_WRAPPERS =
endif
if ENABLE_DIFFTEST
syndwarsfx_WRAPPERS += wrappers_smack.o
endif

syndwarsfx_RCFLAGS = \
  -I"$(top_srcdir)/src" -I"$(builddir)" \
  $(RCFLAGS)

# The DOS network services (IPX, serial, RADICA) are asm using DOS
# interrupts; only built with --enable-dos-network. Without them the
# network service functions report failure (single player). The data of
# that asm (*_data.c) is its byte layout; other builds define the variables
# C uses with their types, in the C files.
if DOS_NETWORK
libswarscore_a_SOURCES += \
	netipx1_data.c \
	netser1_data.c \
	netdos_data.c \
	radica1_data.c \
	radica2_data.c \
	radica3_data.c \
	netipx1.sx \
	netser1.sx \
	netdos.sx \
	radica1.sx \
	radica2.sx \
	radica3.sx
endif

# The asm of remade functions is kept only as the difftest reference, with
# its data as the byte layout it needs (*_data.c); other builds define the
# variables C uses with their types, in the C files.
if ENABLE_DIFFTEST
libswarscore_a_SOURCES += \
	../bfsmacklib/src/bfsmacklib_data.c \
	bat_data.c \
	swars_data.c \
	../bfsmacklib/src/bfsmacklib_s.sx \
	swars.sx \
	../bfsmacklib/src/smack2ail.c \
	bat_s.sx \
	bflib_vidraw_s.sx \
	sound_smp_s.sx \
	sound_heap_s.sx \
	thing_expld_s.sx \
	thing_fire_s.sx
endif

if HAS_WINDRES
syndwarsfx_RESRCS = syndwarsfx_stdres.res
else
syndwarsfx_RESRCS =
endif

# Pretending to contain c++ source so that Automake select c++ linker
nodist_EXTRA_syndwarsfx_SOURCES = dummy.cxx

# Libraries included with the project
syndwarsfx_PROJECT_LIBS = ../lib/libbullfrog.a ../lib/libbfsound.a ../lib/libswrender.a

$(libswarscore_a_OBJECTS) $(syndwarsfx_OBJECTS) $(bin_PROGRAMS): $(syndwarsfx_PROJECT_LIBS)

# The whole library goes in, as when its objects were linked directly: the
# asm reference and the wrappers reach functions nothing else references
syndwarsfx_LDFLAGS = -Wl,--whole-archive libswarscore.a -Wl,--no-whole-archive
syndwarsfx_LDADD = \
  $(syndwarsfx_WRAPPERS) $(syndwarsfx_RESRCS)
syndwarsfx_DEPENDENCIES = libswarscore.a \
  $(syndwarsfx_WRAPPERS) $(syndwarsfx_RESRCS)

# A minimal front end for tests: links the library as any front end does,
# and prints the state hash of every turn (swcore_headless.c). Builds with
# asm have the wrappers outside the library, so only builds without asm.
if !ASM_WRAPPERS
noinst_PROGRAMS = swcore_headless
swcore_headless_SOURCES = swcore.h swcore_headless.c
nodist_EXTRA_swcore_headless_SOURCES = dummy.cxx
swcore_headless_LDADD = libswarscore.a
$(swcore_headless_OBJECTS): $(syndwarsfx_PROJECT_LIBS)
endif

../lib/libbullfrog.a: ../bflibrary/libbullfrog.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

../lib/libbfsound.a: ../bfsoundlib/libbfsound.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

../lib/libswrender.a: ../swrendersoft/libswrender.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

$(syndwarsfx_WRAPPERS:.o=.sx): %.sx: $(top_srcdir)/conf/%.conf
	$(AM_V_GEN)$(MKW) $(MKWFLAGS) -o $@ $<

.sx.o:
	$(AM_V_CC)$(CPP) $(CPPFLAGS) $< $(ASFILTER) \
		| $(CCAS) -x assembler -c $(CCASFLAGS) -o $@ -

if HAS_WINDRES
%.res: $(top_srcdir)/res/%.rc
	$(WINDRES) $(syndwarsfx_RCFLAGS) -i $< -J rc -o $@ -O coff
endif

# Include dynamic libraries in the package
if TARGET_WINDOWS
install-exec-hook:
	mkdir -p "$(DESTDIR)${prefix}/$(PACKAGE)"
	## Find main executable dependencies
	$(eval lib_SHARED_INSTALL := $(shell objdump -p syndwarsfx$(EXEEXT) | \
		sed -n 's/\s*\(DLL Name:\|NEEDED\)\s*\(.*\)$$/\2/p' | \
		xargs -I {} find $(shell dirname $(shell which ${CXX})) -name '{}'))
	## Find sub-dependencies for the previous dependencies
	$(eval lib_SHARED_INSTALL += $(shell echo -n '$(lib_SHARED_INSTALL)' | \
        xargs -d ' ' -I {} objdump -p '{}' | \
		sed -n 's/\s*\(DLL Name:\|NEEDED\)\s*\(.*\)$$/\2/p' | \
		xargs -I {} find $(shell dirname $(shell which ${CXX})) -name '{}'))
	## Remove duplicates
	$(eval lib_SHARED_INSTALL := $(sort $(lib_SHARED_INSTALL)))
	cp $(lib_SHARED_INSTALL) $(DESTDIR)${prefix}/$(PACKAGE)
endif

CLEANFILES = wrappers_*.sx
DISTCLEANFILES = *~

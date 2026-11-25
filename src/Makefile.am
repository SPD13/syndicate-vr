bin_PROGRAMS = syndwarsfx
bindir = $(prefix)/$(PACKAGE)

MKW = $(PYTHON) $(top_srcdir)/util/mkwrappers

# The game is a library, libswarscore (interface: swcore.h), which front
# ends link; syndwarsfx is the SDL desktop front end, its main() only runs
# the library turn by turn. libswarscore holds the game core, which calls
# a front end only through swfe.h; libswarsfe holds the desktop front end
# (display, input, renderer's game side, menus, sound, the application).
# The two reference each other, so programs link them as a group.
# libswarstest holds the test hooks of the game turn (the scripted player
# and the hooks of statetrace.sh's scenarios), on the core alone; the
# hooks of front-end code are in libswarsfe (difftest_hook_fe.c).
noinst_LIBRARIES = libswarscore.a libswarsfe.a libswarstest.a

syndwarsfx_SOURCES = \
	swcore.h \
	swmain.c

libswarscore_a_SOURCES = \
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
	../bfsmacklib/include/smack.h \
	../bfsmacklib/include/smack2ail.h \
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
	difftest.c \
	difftest.h \
	dos.c \
	dos.h \
	game_camera.c \
	game_camera.h \
	game_coreinit.c \
	game_coreinit.h \
	femail.c \
	femail.h \
	game_misend.c \
	game_misend.h \
	game_turn.c \
	game_turn.h \
	game_world.c \
	game_world.h \
	game_state.c \
	swfe.h \
	game_bstype.h \
	game_data.c \
	game_data.h \
	game_speed.c \
	game_speed.h \
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
	network.c \
	network.h \
	pathtrig.c \
	pathtrig.h \
	packet.c \
	packet.h \
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
	plyr_usrstate.c \
	research.c \
	research.h \
	rules.c \
	rules.h \
	scanner.c \
	scanner.h \
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
	thing_onface.h \
	thing_onface.c \
	thing_view.c \
	thing_view.h \
	tngcolisn.c \
	tngcolisn.h \
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

# The desktop front end: the SDL display, input, the software
# renderer's game side, menus, sound and the application shell
# (swapp.c); test hooks too. The core calls it only through swfe.h.
libswarsfe_a_SOURCES = \
	bflib_vidraw.c \
	bflib_vidraw.h \
	../bfsmacklib/include/bfsmack.h \
	../bfsmacklib/src/bfsmack.c \
	../bfsmacklib/src/smailfile.c \
	../bfsmacklib/include/smkdec.h \
	../bfsmacklib/src/smkdec.c \
	devcreds.c \
	devcreds.h \
	display.c \
	display.h \
	difftest_fuzz.c \
	difftest_hook_fe.c \
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
	game_options.c \
	game_options.h \
	game_save.c \
	game_save.h \
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
	mouse.c \
	mouse.h \
	mydraw.c \
	mydraw.h \
	pathtrig_debug.c \
	packetfe.c \
	packetfe.h \
	plyr_usrinp.c \
	plyr_usrinp.h \
	purpldrw.c \
	purpldrw.h \
	purpldrwlst.c \
	purpldrwlst.h \
	scandraw.c \
	scandraw.h \
	sound.c \
	sound.h \
	specblit.c \
	specblit.h \
	swapp.c \
	thing_debug.c \
	thing_ovmous.h \
	thing_ovmous.c \
	tngobjdrw.c \
	tngobjdrw.h \
	vrshare.h \
	vrspectate.c \
	vrspectate.h

libswarscore_a_CPPFLAGS = \
  -I"$(top_srcdir)/bfsmacklib/include" \
  -I"$(top_srcdir)/swrendersoft/include" \
  -I"$(top_srcdir)/bfariadne/include"

libswarsfe_a_CPPFLAGS = $(libswarscore_a_CPPFLAGS)

libswarstest_a_SOURCES = \
	difftest_bot.c \
	difftest_hook_misc.c \
	difftest_hook_people.c \
	difftest_hook_transport.c
libswarstest_a_CPPFLAGS = $(libswarscore_a_CPPFLAGS)
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
syndwarsfx_PROJECT_LIBS = ../lib/libbullfrog.a ../lib/libbfsound.a ../lib/libswrender.a \
  ../lib/libswworld.a

$(libswarscore_a_OBJECTS) $(libswarsfe_a_OBJECTS) $(libswarstest_a_OBJECTS) $(syndwarsfx_OBJECTS) $(bin_PROGRAMS): $(syndwarsfx_PROJECT_LIBS)

# The whole libraries go in, as when their objects were linked directly:
# the asm reference and the wrappers reach functions nothing else references
syndwarsfx_LDFLAGS = -Wl,--whole-archive libswarscore.a libswarsfe.a libswarstest.a -Wl,--no-whole-archive
# (the drawing; the world data, libswworld, comes with LIBS for every program)
syndwarsfx_LDADD = \
  $(syndwarsfx_WRAPPERS) $(syndwarsfx_RESRCS) ../lib/libswrender.a
syndwarsfx_DEPENDENCIES = libswarscore.a libswarsfe.a libswarstest.a \
  $(syndwarsfx_WRAPPERS) $(syndwarsfx_RESRCS)

# Test front ends; builds with asm have the wrappers outside the libraries,
# so only builds without asm.
# swcore_headless links the libraries as any front end does, and prints
# the state hash of every turn (swcore_headless.c).
# swcore_coreonly is the same program on the game core alone, with the
# null front end (swfe_null.c): no display, input, sound or menus; it
# starts a mission with swcore_core_init() and plays it with
# swcore_core_step().
if !ASM_WRAPPERS
noinst_PROGRAMS = swcore_headless swcore_coreonly
swcore_headless_SOURCES = swcore.h swcore_headless.c
nodist_EXTRA_swcore_headless_SOURCES = dummy.cxx
# (whole, as syndwarsfx: the two libraries reference each other)
swcore_headless_LDFLAGS = $(syndwarsfx_LDFLAGS)
swcore_headless_LDADD = ../lib/libswrender.a
swcore_headless_DEPENDENCIES = libswarscore.a libswarsfe.a libswarstest.a
$(swcore_headless_OBJECTS): $(syndwarsfx_PROJECT_LIBS)
swcore_coreonly_SOURCES = swcore.h swcore_headless.c swfe.h swfe_null.c
swcore_coreonly_CPPFLAGS = $(libswarscore_a_CPPFLAGS) -DSWCORE_CORE_ONLY=1
nodist_EXTRA_swcore_coreonly_SOURCES = dummy.cxx
# (with the test hooks of the core, which swcore_headless.c sets)
swcore_coreonly_LDADD = libswarstest.a libswarscore.a
swcore_coreonly_DEPENDENCIES = libswarscore.a libswarstest.a
$(swcore_coreonly_OBJECTS): $(syndwarsfx_PROJECT_LIBS)
endif

../lib/libbullfrog.a: ../bflibrary/libbullfrog.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

../lib/libbfsound.a: ../bfsoundlib/libbfsound.a
	$(MAKE) -C $(<D) DESTDIR="../" prefix="" install

# (installing swrendersoft installs both of its libraries)
../lib/libswworld.a: ../lib/libswrender.a

../lib/libswrender.a: ../swrendersoft/libswrender.a ../swrendersoft/libswworld.a
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

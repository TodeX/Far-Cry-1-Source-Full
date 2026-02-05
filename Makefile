CC := gcc
CXX := g++
CFLAGS := -g -O2 -fPIC -DLINUX -D_AMD64_ -DLINUX64 -D_LZSS_ -D_XZLIB_ -I. -ICryCommon -ICrySystem/zlib -Wno-multichar -Wno-write-strings -fpermissive
CXXFLAGS := $(CFLAGS)
LDFLAGS := -shared
LDLIBS := -ldl -lpthread

OUTDIR := BinLinux64

# Ensure output directory exists
$(shell mkdir -p $(OUTDIR))

# Modules
MODULES := crysystem crygame crynetwork cryphysics cryscriptsystem crysoundsystem cryinput crymovie cryaisystem cryentitysystem cryfont cry3dengine cryanimation xrenderogl xrendernull

# Source lists
CRYSYSTEM_SRCS := $(wildcard CrySystem/*.cpp) $(wildcard CrySystem/zlib/*.c)
# Exclude Windows specific files if necessary, but we'll try compiling all .cpp first
# CRYSYSTEM_SRCS := $(filter-out CrySystem/SystemWin32.cpp, $(CRYSYSTEM_SRCS))

CRYGAME_SRCS := $(wildcard CryGame/*.cpp)
CRYNETWORK_SRCS := $(wildcard CryNetwork/*.cpp)
CRYPHYSICS_SRCS := $(wildcard CryPhysics/*.cpp)
CRYSCRIPTSYSTEM_SRCS := $(wildcard CryScriptSystem/*.cpp)
CRYSOUNDSYSTEM_SRCS := $(wildcard CrySoundSystem/*.cpp)
CRYINPUT_SRCS := $(wildcard CryInput/*.cpp)
CRYINPUT_SRCS := $(filter-out CryInput/XGamepad.cpp CryInput/XDebugKeyboard.cpp, $(CRYINPUT_SRCS))

CRYMOVIE_SRCS := $(wildcard CryMovie/*.cpp)
CRYAISYSTEM_SRCS := $(wildcard CryAISystem/*.cpp)
CRYENTITYSYSTEM_SRCS := $(wildcard CryEntitySystem/*.cpp)
CRYFONT_SRCS := $(wildcard CryFont/*.cpp)
CRY3DENGINE_SRCS := $(wildcard Cry3DEngine/*.cpp)
CRYANIMATION_SRCS := $(wildcard CryAnimation/*.cpp)

# RenderDll is special
XRENDEROGL_SRCS := $(wildcard RenderDll/XRenderOGL/*.cpp) $(wildcard RenderDll/Common/*.cpp)
XRENDERNULL_SRCS := $(wildcard RenderDll/XRenderNULL/*.cpp) $(wildcard RenderDll/Common/*.cpp)

# Object files
CRYSYSTEM_OBJS := $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(CRYSYSTEM_SRCS)))
CRYGAME_OBJS := $(patsubst %.cpp, %.o, $(CRYGAME_SRCS))
CRYNETWORK_OBJS := $(patsubst %.cpp, %.o, $(CRYNETWORK_SRCS))
CRYPHYSICS_OBJS := $(patsubst %.cpp, %.o, $(CRYPHYSICS_SRCS))
CRYSCRIPTSYSTEM_OBJS := $(patsubst %.cpp, %.o, $(CRYSCRIPTSYSTEM_SRCS))
CRYSOUNDSYSTEM_OBJS := $(patsubst %.cpp, %.o, $(CRYSOUNDSYSTEM_SRCS))
CRYINPUT_OBJS := $(patsubst %.cpp, %.o, $(CRYINPUT_SRCS))
CRYMOVIE_OBJS := $(patsubst %.cpp, %.o, $(CRYMOVIE_SRCS))
CRYAISYSTEM_OBJS := $(patsubst %.cpp, %.o, $(CRYAISYSTEM_SRCS))
CRYENTITYSYSTEM_OBJS := $(patsubst %.cpp, %.o, $(CRYENTITYSYSTEM_SRCS))
CRYFONT_OBJS := $(patsubst %.cpp, %.o, $(CRYFONT_SRCS))
CRY3DENGINE_OBJS := $(patsubst %.cpp, %.o, $(CRY3DENGINE_SRCS))
CRYANIMATION_OBJS := $(patsubst %.cpp, %.o, $(CRYANIMATION_SRCS))
XRENDEROGL_OBJS := $(patsubst %.cpp, %.o, $(XRENDEROGL_SRCS))
XRENDERNULL_OBJS := $(patsubst %.cpp, %.o, $(XRENDERNULL_SRCS))

# Targets
.PHONY: all clean $(MODULES) farcry

all: $(MODULES) farcry

crysystem: $(OUTDIR)/crysystem.so
$(OUTDIR)/crysystem.so: $(CRYSYSTEM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

crygame: $(OUTDIR)/crygame.so
$(OUTDIR)/crygame.so: $(CRYGAME_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

crynetwork: $(OUTDIR)/crynetwork.so
$(OUTDIR)/crynetwork.so: $(CRYNETWORK_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryphysics: $(OUTDIR)/cryphysics.so
$(OUTDIR)/cryphysics.so: $(CRYPHYSICS_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryscriptsystem: $(OUTDIR)/cryscriptsystem.so
$(OUTDIR)/cryscriptsystem.so: $(CRYSCRIPTSYSTEM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

crysoundsystem: $(OUTDIR)/crysoundsystem.so
$(OUTDIR)/crysoundsystem.so: $(CRYSOUNDSYSTEM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryinput: $(OUTDIR)/cryinput.so
$(OUTDIR)/cryinput.so: $(CRYINPUT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

crymovie: $(OUTDIR)/crymovie.so
$(OUTDIR)/crymovie.so: $(CRYMOVIE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryaisystem: $(OUTDIR)/cryaisystem.so
$(OUTDIR)/cryaisystem.so: $(CRYAISYSTEM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryentitysystem: $(OUTDIR)/cryentitysystem.so
$(OUTDIR)/cryentitysystem.so: $(CRYENTITYSYSTEM_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryfont: $(OUTDIR)/cryfont.so
$(OUTDIR)/cryfont.so: $(CRYFONT_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cry3dengine: $(OUTDIR)/cry3dengine.so
$(OUTDIR)/cry3dengine.so: $(CRY3DENGINE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cryanimation: $(OUTDIR)/cryanimation.so
$(OUTDIR)/cryanimation.so: $(CRYANIMATION_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

xrenderogl: $(OUTDIR)/xrenderogl.so
$(OUTDIR)/xrenderogl.so: $(XRENDEROGL_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lGL -lGLU

xrendernull: $(OUTDIR)/xrendernull.so
$(OUTDIR)/xrendernull.so: $(XRENDERNULL_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# FarCry Executable
FARCRY_SRCS := FARCRY/Main.cpp
FARCRY_OBJS := $(patsubst %.cpp, %.o, $(FARCRY_SRCS))

farcry: $(OUTDIR)/farcry
$(OUTDIR)/farcry: $(FARCRY_OBJS)
	$(CXX) -o $@ $^ $(LDLIBS)

# Generic rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CRYSYSTEM_OBJS) $(CRYGAME_OBJS) $(CRYNETWORK_OBJS) $(CRYPHYSICS_OBJS) \
	      $(CRYSCRIPTSYSTEM_OBJS) $(CRYSOUNDSYSTEM_OBJS) $(CRYINPUT_OBJS) $(CRYMOVIE_OBJS) \
	      $(CRYAISYSTEM_OBJS) $(CRYENTITYSYSTEM_OBJS) $(CRYFONT_OBJS) $(CRY3DENGINE_OBJS) \
	      $(CRYANIMATION_OBJS) $(XRENDEROGL_OBJS) $(XRENDERNULL_OBJS) $(FARCRY_OBJS)
	rm -f $(OUTDIR)/*.so $(OUTDIR)/farcry

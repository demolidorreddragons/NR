ROOT := ../../../..
OBJDIR := $(ROOT)/build/ps4
OUTDIR := $(ROOT)/dist/ps4
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
GH_SDK := $(GOLDHEN_SDK)
PLUGIN := ps4_neural_engine
# GitHub HTTPS client is compiled into the PRX for opt-in Device Flow uploads.\n# Build validation follows every uploader source change.\n# Includes explicit local disconnect shortcut support.\n# CI verifies the final PS4 PRX and installer package.
TARGET_ELF := $(OUTDIR)/$(PLUGIN).elf
TARGET_PRX := $(OUTDIR)/$(PLUGIN).prx
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CC := clang
CXX := clang++
LD := ld.lld
CDIR := linux
else
CC := clang
CXX := clang++
LD := ld.lld
CDIR := macos
endif
COMMON_FLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -fno-rtti -D__FINAL__=1 -D__USE_PRINTF__ -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem /usr/include/x86_64-linux-gnu -I$(GH_SDK)/include -I$(ROOT)/include -I.
CFLAGS := $(COMMON_FLAGS) -std=c11 -c
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 -isystem $(TOOLCHAIN)/include/c++/v1 -c
LIBS := -lSceLibcInternal -lSceGnmDriver -lScePad -lGoldHEN_Hook -lkernel -lSceSystemService -lSceUserService -lSceHttp -lSceSsl -lSceNet -lSceSysmodule -lc++
OBJS := $(OBJDIR)/main.o $(OBJDIR)/plugin_common.o $(OBJDIR)/telemetry.o $(OBJDIR)/telemetry_bridge.o $(OBJDIR)/github_client.o $(OBJDIR)/upload_confirmation.o

.PHONY: all clean
all: $(TARGET_PRX)

$(OBJDIR) $(OUTDIR):
	mkdir -p $@

$(OBJDIR)/main.o: main.c plugin_common.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/plugin_common.o: plugin_common.c plugin_common.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/telemetry.o: $(ROOT)/src/telemetry.cpp $(ROOT)/include/pne/telemetry.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/include -o $@ $<
$(OBJDIR)/telemetry_bridge.o: $(ROOT)/src/telemetry_bridge.cpp $(ROOT)/include/pne/telemetry.hpp telemetry_bridge.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/include -o $@ $<
$(OBJDIR)/github_client.o: github_client.cpp upload_confirmation.h $(ROOT)/include/pne/github_oauth.hpp plugin_common.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<
$(OBJDIR)/upload_confirmation.o: upload_confirmation.cpp upload_confirmation.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(TARGET_PRX): $(OBJS) | $(OUTDIR)
	$(LD) -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x -e _init --eh-frame-hdr -L$(TOOLCHAIN)/lib -L$(GH_SDK) $(GH_SDK)/build/crtprx.o $(OBJS) -o $(TARGET_ELF) $(LIBS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(TARGET_ELF) -out=$(TARGET_ELF).oelf --lib=$@ --paid 0x3800000000000011
	test -s $@

clean:
	rm -rf $(OBJDIR) $(OUTDIR)

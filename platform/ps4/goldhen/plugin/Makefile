ROOT := ../../../..
OBJDIR := $(ROOT)/build/ps4
OUTDIR := $(ROOT)/dist/ps4
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
GH_SDK := $(if $(strip $(GOLDHEN_SDK)),$(GOLDHEN_SDK),$(ROOT)/GoldHEN_Plugins_SDK)
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
COMMON_FLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -fno-exceptions -fno-rtti -D__FINAL__=1 -D__USE_PRINTF__ -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem /usr/include/x86_64-linux-gnu -I$(GH_SDK)/include -I$(ROOT)/include -I$(ROOT)/OpenGNM/include -I.
CFLAGS := $(COMMON_FLAGS) -std=c11 -c
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 -isystem $(TOOLCHAIN)/include/c++/v1 -c
LIBS := -lSceLibcInternal -lSceGnmDriver -lScePad -lGoldHEN_Hook -lkernel -lSceSystemService -lSceUserService -lSceHttp -lSceSsl -lSceNet -lSceSysmodule -lc++
OBJS := $(OBJDIR)/main.o $(OBJDIR)/plugin_common.o $(OBJDIR)/hde64_bounded.o $(OBJDIR)/hde64_bounded_plan.o $(OBJDIR)/telemetry.o $(OBJDIR)/telemetry_bridge.o $(OBJDIR)/github_client.o $(OBJDIR)/gnm_resource_binding.o $(OBJDIR)/resource_table_contract.o $(OBJDIR)/shader_binary_contract.o $(OBJDIR)/vsharp_codec.o $(OBJDIR)/stage_log.o $(OBJDIR)/compute_smoke.o

.PHONY: all clean
all: $(TARGET_PRX)

$(OBJDIR) $(OUTDIR):
	mkdir -p $@

$(OBJDIR)/main.o: main.c plugin_common.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/plugin_common.o: plugin_common.c plugin_common.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/hde64_bounded.o: hde64_bounded.c hde64_bounded.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/hde64_bounded_plan.o: hde64_bounded_plan.c hde64_bounded_plan.h hde64_bounded.h | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $<
$(OBJDIR)/telemetry.o: $(ROOT)/src/telemetry.cpp $(ROOT)/include/pne/telemetry.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/include -o $@ $<
$(OBJDIR)/telemetry_bridge.o: $(ROOT)/src/telemetry_bridge.cpp $(ROOT)/include/pne/telemetry.hpp telemetry_bridge.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/include -o $@ $<
$(OBJDIR)/github_client.o: github_client.cpp $(ROOT)/include/pne/github_oauth.hpp plugin_common.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -o $@ $<
$(OBJDIR)/gnm_resource_binding.o: $(ROOT)/platform/ps4/gcn8/gnm_resource_binding.cpp $(ROOT)/platform/ps4/gcn8/gnm_resource_binding.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/OpenGNM/include -o $@ $<
$(OBJDIR)/resource_table_contract.o: $(ROOT)/platform/ps4/gcn8/host/resource_table_contract.cpp $(ROOT)/platform/ps4/gcn8/host/resource_table_contract.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/platform/ps4/gcn8/host -o $@ $<
$(OBJDIR)/shader_binary_contract.o: $(ROOT)/platform/ps4/gcn8/host/shader_binary_contract.cpp $(ROOT)/platform/ps4/gcn8/host/shader_binary_contract.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/platform/ps4/gcn8/host -o $@ $<
$(OBJDIR)/vsharp_codec.o: $(ROOT)/platform/ps4/gcn8/host/vsharp_codec.cpp $(ROOT)/platform/ps4/gcn8/host/vsharp_codec.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/platform/ps4/gcn8/host -o $@ $<
$(OBJDIR)/stage_log.o: $(ROOT)/platform/ps4/gcn8/stage_log.c $(ROOT)/platform/ps4/gcn8/stage_log.h | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(ROOT)/platform/ps4/gcn8 -o $@ $<
$(OBJDIR)/compute_smoke.o: $(ROOT)/platform/ps4/gcn8/compute_smoke.cpp $(ROOT)/platform/ps4/gcn8/compute_smoke.h | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(ROOT)/OpenGNM/include -I$(ROOT)/platform/ps4/gcn8 -o $@ $<

$(TARGET_PRX): $(OBJS) | $(OUTDIR)
	$(LD) -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x -e _init --eh-frame-hdr -L$(TOOLCHAIN)/lib -L$(GH_SDK) $(GH_SDK)/build/crtprx.o $(OBJS) -o $(TARGET_ELF) $(LIBS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(TARGET_ELF) -out=$(TARGET_ELF).oelf --lib=$@ --paid 0x3800000000000011
	test -s $@

clean:
	rm -rf $(OBJDIR) $(OUTDIR)

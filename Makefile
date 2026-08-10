# statusblocks
# See LICENSE file for copyright and license details.

VERSION ?= 1.0.0
CC      ?= cc

CFLAGS ?= -Os

# Kept separate from CFLAGS so that `make CFLAGS=-O2` (or a distro's
# hardening flags) cannot silently switch the warnings back off.
WARNINGS := -std=c11 -Wall -Wextra -Wpedantic -Wpointer-arith -Wshadow \
	-Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
	-Wformat=2 -Wconversion -Wsign-conversion -Wno-deprecated-declarations

CPPFLAGS += -MMD -MP -DVERSION=\"${VERSION}\" -I$(INCLUDE) -I.

# Installation paths. Blocks live in their own directory so they do not
# clutter $(BINDIR); point dwmblocks' config at $(BLOCKDIR).
PREFIX   ?= $(HOME)/.local
BINDIR   ?= $(PREFIX)/bin
BLOCKDIR ?= $(BINDIR)/statusblocks

# Directory structure
SRC_DIR   := src
BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj
INCLUDE   := $(SRC_DIR)/include

# Per-library flags. Each block pulls in only what it actually uses, so a
# missing libnm does not stop you building the clock.
#
# Dependency include paths are rewritten from -I to -isystem: the strict
# warning set applies to this project's code, not to third-party headers.
# (libnm's nm-dbus-interface.h, for one, is not -Wpedantic clean.)
sysinc = $(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(1)))

NOTIFY_CFLAGS := $(call sysinc,libnotify)
NOTIFY_LIBS   := $(shell pkg-config --libs libnotify)
NM_CFLAGS     := $(call sysinc,libnm glib-2.0)
NM_LIBS       := $(shell pkg-config --libs libnm glib-2.0)
PULSE_CFLAGS  := $(call sysinc,libpulse)
PULSE_LIBS    := $(shell pkg-config --libs libpulse)
DBUS_CFLAGS   := $(call sysinc,dbus-1)
DBUS_LIBS     := $(shell pkg-config --libs dbus-1)

# utils.c always needs libnotify; colors.c and toggle.c always need Xlib.
COMMON_CFLAGS := $(NOTIFY_CFLAGS)
COMMON_LIBS   := $(NOTIFY_LIBS) -lX11

# Shared objects linked into every block
UTILS_SRC := $(SRC_DIR)/utils.c $(SRC_DIR)/colors.c $(SRC_DIR)/toggle.c
UTILS_OBJ := $(OBJ_DIR)/utils.o $(OBJ_DIR)/colors.o $(OBJ_DIR)/toggle.o

# Block configuration - comment out blocks you don't need
BLOCKS := time \
          keyboard \
          battery \
          date \
          system \
          bluetooth \
          internet \
          memory \
          power \
          volume

BLOCK_SRCS := $(addprefix $(SRC_DIR)/, $(addsuffix .c, $(BLOCKS)))
BLOCK_OBJS := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(BLOCKS)))
BINARIES   := $(addprefix $(BIN_DIR)/, $(BLOCKS))
DEPS       := $(BLOCK_OBJS:.o=.d) $(UTILS_OBJ:.o=.d)

# Per-block extras, applied to both the compile and the link
$(OBJ_DIR)/keyboard.o  $(BIN_DIR)/keyboard:  BLOCK_LIBS := -lxkbfile
$(OBJ_DIR)/bluetooth.o $(BIN_DIR)/bluetooth: BLOCK_CFLAGS := $(DBUS_CFLAGS)
$(OBJ_DIR)/bluetooth.o $(BIN_DIR)/bluetooth: BLOCK_LIBS := $(DBUS_LIBS)
$(OBJ_DIR)/internet.o  $(BIN_DIR)/internet:  BLOCK_CFLAGS := $(NM_CFLAGS)
$(OBJ_DIR)/internet.o  $(BIN_DIR)/internet:  BLOCK_LIBS := $(NM_LIBS)
$(OBJ_DIR)/volume.o    $(BIN_DIR)/volume:    BLOCK_CFLAGS := $(PULSE_CFLAGS)
$(OBJ_DIR)/volume.o    $(BIN_DIR)/volume:    BLOCK_LIBS := $(PULSE_LIBS) -lm

# Pretty output
COLOR  ?= 1
PRINTF ?= printf

ifeq ($(COLOR),0)
COLOR_RESET  :=
COLOR_GREEN  :=
COLOR_YELLOW :=
COLOR_BLUE   :=
COLOR_CYAN   :=
else
COLOR_RESET  := \033[0m
COLOR_GREEN  := \033[1;32m
COLOR_YELLOW := \033[1;33m
COLOR_BLUE   := \033[1;34m
COLOR_CYAN   := \033[1;36m
endif

all: $(BINARIES)

# Shorthand: `make time` builds just that block.
$(BLOCKS): %: $(BIN_DIR)/%

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(UTILS_OBJ) | $(BIN_DIR)
	@$(PRINTF) "$(COLOR_GREEN)Linking:$(COLOR_RESET) %s\n" "$@"
	@$(CC) $(LDFLAGS) -o $@ $< $(UTILS_OBJ) $(COMMON_LIBS) $(BLOCK_LIBS) $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c config.h | $(OBJ_DIR)
	@$(PRINTF) "$(COLOR_BLUE)Compiling:$(COLOR_RESET) %s\n" "$@"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(COMMON_CFLAGS) $(BLOCK_CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	@mkdir -p $@

config.h: | config.def.h
	@$(PRINTF) "$(COLOR_CYAN)Generating:$(COLOR_RESET) %s\n" "$@"
	@cp config.def.h $@

clean:
	@$(PRINTF) "$(COLOR_YELLOW)Cleaning:$(COLOR_RESET) %s\n" "$(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)

install: all
	@$(PRINTF) "$(COLOR_CYAN)Installing blocks to:$(COLOR_RESET) %s\n" "$(DESTDIR)$(BLOCKDIR)"
	@install -d $(DESTDIR)$(BLOCKDIR)
	@for block in $(BLOCKS); do \
		install -m 755 $(BIN_DIR)/$$block $(DESTDIR)$(BLOCKDIR)/$$block; \
	done

uninstall:
	@$(PRINTF) "$(COLOR_CYAN)Uninstalling blocks from:$(COLOR_RESET) %s\n" "$(DESTDIR)$(BLOCKDIR)"
	@for block in $(BLOCKS); do \
		rm -f $(DESTDIR)$(BLOCKDIR)/$$block; \
	done
	@rmdir $(DESTDIR)$(BLOCKDIR) 2>/dev/null || true

-include $(DEPS)

.PHONY: all clean install uninstall $(BLOCKS)
.SECONDARY: $(BLOCK_OBJS) $(UTILS_OBJ)

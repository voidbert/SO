# CONFIGURATION VARIABLES

CC        := gcc
CFLAGS    := -Werror -Wall -Wextra -pedantic -Wshadow -Wcast-qual -Wfloat-conversion \
	-Wwrite-strings -Wstrict-prototypes -Winit-self -Wfloat-equal
STANDARDS := -std=c99 -D_POSIX_C_SOURCE=200809L
LIBS      :=

DEBUG_CFLAGS   := -O0 -ggdb3
RELEASE_CFLAGS := -O2
PROFILE_CFLAGS := -O2 -ggdb3

# Note: none of these directories can be the root of the project
# Also, these may need to be updated in .gitignore
BUILDDIR       := bin
SERVER_EXENAME := orchestrator
CLIENT_EXENAME := client
DEPDIR         := deps
DOCSDIR        := docs
OBJDIR         := obj

# Default installation directory (if PREFIX is not set)
PREFIX ?= $(HOME)/.local

# END OF CONFIGURATION

COMMON_SOURCES = $(shell find src -maxdepth 1 -name '*.c' -type f )
SERVER_SOURCES = $(COMMON_SOURCES) $(shell find src/server -name '*.c' -type f)
CLIENT_SOURCES = $(COMMON_SOURCES) $(shell find src/client -name '*.c' -type f)

COMMON_HEADERS = $(shell find include -maxdepth 1 -name '*.h' -type f)
SERVER_HEADERS = $(COMMON_HEADERS) $(shell find include/server -name '*.c' -type f)
CLIENT_HEADERS = $(COMMON_HEADERS) $(shell find include/client -name '*.c' -type f)

SERVER_OBJECTS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SERVER_SOURCES))
CLIENT_OBJECTS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(CLIENT_SOURCES))

THEMES  = $(wildcard theme/*)
DEPENDS = $(patsubst src/%.c, $(DEPDIR)/%.d, $(COMMON_SOURCES) $(SERVER_SOURCES) $(CLIENT_SOURCES))

ifeq ($(DEBUG), 1)
	CFLAGS += $(DEBUG_CFLAGS)
	BUILD_TYPE = DEBUG
else ifeq ($(PROFILE), 1)
	CFLAGS += $(PROFILE_CFLAGS)
	BUILD_TYPE = PROFILE
else
	CFLAGS += $(RELEASE_CFLAGS)
	BUILD_TYPE = RELEASE
endif
CFLAGS += $(STANDARDS)

# Only generate dependencies for tasks that require them
# THIS WILL NOT WORK IF YOU TRY TO MAKE AN INDIVIDUAL FILE
ifeq (, $(MAKECMDGOALS))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter all, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter install, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter server, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter orchestrator, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter client, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else
	INCLUDE_DEPENDS = N
endif

default: $(BUILDDIR)/$(SERVER_EXENAME) $(BUILDDIR)/$(CLIENT_EXENAME)
all: $(BUILDDIR)/$(SERVER_EXENAME) $(BUILDDIR)/$(CLIENT_EXENAME) $(DOCSDIR)
server: $(BUILDDIR)/$(SERVER_EXENAME)
orchestrator: $(BUILDDIR)/$(SERVER_EXENAME)
client: $(BUILDDIR)/$(CLIENT_EXENAME)

ifeq (Y, $(INCLUDE_DEPENDS))
include $(DEPENDS)
endif

# Welcome to my unorthodox meta-programming Makefile! To get auto-dependency generation working,
# this code is unusual for a Makefile, but hey, it works!
#
# To compile a source file, a Makefile rule is generated with $(CC) -MM, to account for header
# dependencies. The commands that actually compile the source are added to that rule file before
# its included. A script is also run in the end of the generated rule's execution, to regenerate
# that same rule with any dependency change that may have occurred.
$(DEPDIR)/%.d: src/%.c Makefile
	$(eval OBJ := $(patsubst src/%.c, $(OBJDIR)/%.o, $<))

	$(eval RULE_CMD_MKDIR = @mkdir -p $(shell dirname $(OBJ)))
	$(eval RULE_CMD_CC = $(CC) -MMD -MT "$(OBJ)" -MF $@2 -c -o $(OBJ) $< $(CFLAGS) -Iinclude)
	$(eval RULE_CMD_SCRIPT = @./scripts/makefilehelper.sh $@)

	@# Create the dependency file for the first time, adding all the commands.
	@mkdir -p $(shell dirname $@)
	$(CC) -MM $< -MT $(OBJ) -MF $@ -Iinclude

	@printf "\t%s\n" "$(RULE_CMD_MKDIR)" >> $@
	@printf "\t%s\n" "$(RULE_CMD_CC)" >> $@
	@printf "\t%s\n" "$(RULE_CMD_SCRIPT)" >> $@

$(BUILDDIR)/$(SERVER_EXENAME) $(BUILDDIR)/$(SERVER_EXENAME)_type: $(SERVER_OBJECTS)
	@mkdir -p $(BUILDDIR)
	@echo $(BUILD_TYPE) > $(BUILDDIR)/$(SERVER_EXENAME)_type
	$(CC) -o $@ $^ $(LIBS)

$(BUILDDIR)/$(CLIENT_EXENAME) $(BUILDDIR)/$(CLIENT_EXENAME)_type: $(CLIENT_OBJECTS)
	@mkdir -p $(BUILDDIR)
	@echo $(BUILD_TYPE) > $(BUILDDIR)/$(CLIENT_EXENAME)_type
	$(CC) -o $@ $^ $(LIBS)

define Doxyfile
	INPUT                  = include src README.md DEVELOPERS.md
	RECURSIVE              = YES
	EXTRACT_ALL            = YES
	FILE_PATTERNS          = *.h *.c

	PROJECT_NAME           = SO
	USE_MDFILE_AS_MAINPAGE = README.md

	ENABLE_PREPROCESSING   = YES
	MACRO_EXPANSION        = YES
	EXPAND_ONLY_PREDEF     = YES
	PREDEFINED             = __attribute__(x)=

	OUTPUT_DIRECTORY       = $(DOCSDIR)
	GENERATE_HTML          = YES
	GENERATE_LATEX         = NO

	HTML_EXTRA_FILES       = theme
	HTML_EXTRA_STYLESHEET  = theme/UMinho.css
	PROJECT_LOGO           = theme/UMinhoLogo.png
endef
export Doxyfile

$(DOCSDIR): $(SERVER_SOURCES) $(CLIENT_SOURCES) $(SERVER_HEADERS) $(CLIENT_HEADERS) $(THEMES)
	echo "$$Doxyfile" | doxygen - 1> /dev/null
	@touch $(DOCSDIR) # Update "last updated" time to now

.PHONY: clean
clean:
	rm -r $(BUILDDIR) $(DEPDIR) $(DOCSDIR) $(OBJDIR) 2> /dev/null ; true

install: $(BUILDDIR)/$(SERVER_EXENAME) $(BUILDDIR)/$(CLIENT_EXENAME)
	install -Dm 755 $(BUILDDIR)/$(SERVER_EXENAME) $(PREFIX)/bin
	install -Dm 755 $(BUILDDIR)/$(CLIENT_EXENAME) $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm $(PREFIX)/bin/$(SERVER_EXENAME)
	rm $(PREFIX)/bin/$(CLIENT_EXENAME)

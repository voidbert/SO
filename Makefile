# CONFIGURATION VARIABLES

CC              := gcc
CFLAGS          := -Wall -Wextra -Werror -pedantic
STANDARDS       := -std=c99 -D_POSIX_C_SOURCE=200809L
LIBS            := -lm -lpthread

DEBUG_CFLAGS    := -O0 -ggdb3
RELEASE_CFLAGS  := -O2
PROFILE_CFLAGS  := -O2 -ggdb3

# Note: none of these directories can be the root of the project
# Also, these may need to be updated in .gitignore
BUILDDIR        := build
EXENAME         := SO
DEPDIR          := deps
DOCSDIR         := docs
OBJDIR          := obj

# Default installation directory (if PREFIX is not set)
PREFIX          ?= $(HOME)/.local

# END OF CONFIGURATION

SOURCES = $(shell find "src" -name '*.c' -type f)
HEADERS = $(shell find "include" -name '*.h' -type f)
THEMES  = $(wildcard theme/*)
OBJECTS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SOURCES))
DEPENDS = $(patsubst src/%.c, $(DEPDIR)/%.d, $(SOURCES))

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
# THIS WILL NOT WORK IF YOU TRY TO MAKE A INDIVIDUAL FILES
ifeq (, $(MAKECMDGOALS))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter default, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter all, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else ifneq (, $(filter install, $(MAKECMDGOALS)))
	INCLUDE_DEPENDS = Y
else
	INCLUDE_DEPENDS = N
endif

default: $(BUILDDIR)/$(EXENAME)
all: $(BUILDDIR)/$(EXENAME) $(DOCSDIR)

ifeq (Y, $(INCLUDE_DEPENDS))
include $(DEPENDS)
endif

# Welcome to my unorthodox meta-programming Makefile! To get auto-dependency generation working,
# this code is unusual for a Makefile, but hey, it works!
#
# To compile a source file, a makefile rule is generated with $(CC) -MM, to account for header
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

$(BUILDDIR)/$(EXENAME) $(BUILDDIR)/$(EXENAME)_type: $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	@echo $(BUILD_TYPE) > $(BUILDDIR)/$(EXENAME)_type
	$(CC) -o $@ $^ $(LIBS)

define Doxyfile
	INPUT                  = include src README.md DEVELOPERS.md
	RECURSIVE              = YES
	EXTRACT_ALL            = YES
	FILE_PATTERNS          = *.h *.c

	PROJECT_NAME           = SO
	USE_MDFILE_AS_MAINPAGE = README.md

	OUTPUT_DIRECTORY       = $(DOCSDIR)
	GENERATE_HTML          = YES
	GENERATE_LATEX         = NO

	HTML_EXTRA_FILES       = theme
	HTML_EXTRA_STYLESHEET  = theme/UMinho.css
	PROJECT_LOGO           = theme/UMinhoLogo.png
endef
export Doxyfile

$(DOCSDIR): $(SOURCES) $(HEADERS) $(THEMES)
	echo "$$Doxyfile" | doxygen -
	@touch $(DOCSDIR) # Update "last updated" time to now

.PHONY: clean
clean:
	rm -r $(BUILDDIR) $(DEPDIR) $(DOCSDIR) $(OBJDIR) 2> /dev/null ; true

install: $(BUILDDIR)/$(EXENAME)
	install -Dm 755 $(BUILDDIR)/$(EXENAME) $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm $(PREFIX)/bin/$(EXENAME)

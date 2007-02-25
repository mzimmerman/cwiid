#Copyright (C) 2007 L. Donnie Smith

HEADER	   = $(LIB_NAME).h
STATIC_LIB = lib$(LIB_NAME).a
LINK_NAME  = lib$(LIB_NAME).so
SO_NAME    = $(LINK_NAME).$(MAJOR_VER)
SHARED_LIB = $(SO_NAME).$(MINOR_VER)

OBJECTS = $(SOURCES:.c=.o)
DEPS    = $(SOURCES:.c=.d)

CFLAGS += -fpic

all: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB)

$(STATIC_LIB): $(OBJECTS)
	ar rcs $(STATIC_LIB) $(OBJECTS)

$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SO_NAME) $(LDFLAGS) $(LDLIBS) \
	      -o $(SHARED_LIB) $(OBJECTS)

install: install_header install_static install_shared

install_header:
	cp $(LIB_NAME).h $(INC_INST_DIR)

install_static: static
	cp $(STATIC_LIB) $(LIB_INST_DIR)

install_shared: shared
	cp $(SHARED_LIB) $(LIB_INST_DIR)
	ln -sf $(SO_NAME) $(LIB_INST_DIR)/$(LINK_NAME)
	ldconfig

clean:
	rm -f $(STATIC_LIB) $(SHARED_LIB) $(OBJECTS) $(DEPS)

uninstall:
	rm -f $(INC_INST_DIR)/$(LIB_NAME).h $(LIB_INST_DIR)/$(STATIC_LIB) \
	    $(LIB_INST_DIR)/$(LINK_NAME)*

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
include $(COMMON)/include/dep.mak
-include $(DEPS)
endif
endif

.PHONY: all static shared install install_header install_static install_shared clean uninstall
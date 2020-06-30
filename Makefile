TARGET := xrofs-cextract
SRC := xrofs-cextract xrofs
BUILDDIR := .build

ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
    CFLAGS := 
	EXTFLAGS := -l:libargp.dll.a
else
    CFLAGS := -m32 -march=i386 -static
endif
# win gcc -O2 -DXROFS_USE_ALWAYSINLINE=1 -std=gnu11 xrofs-cextract.c xrofs.c -largp -o ./test
# gcc -static -O2 -DXROFS_USE_ALWAYSINLINE=1 -std=gnu11 xrofs-cextract.c xrofs.c -l:libargp.dll.a -o ./test

CFLAGS += -std=gnu11 -DXROFS_USE_ALWAYSINLINE=1 -O2 
CC := gcc

.PHONY: clean all

all: | tidy $(BUILDDIR)/$(TARGET) dist clean

$(BUILDDIR):
	mkdir -p $@

$(BUILDDIR)/$(TARGET): $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SRC)))
	$(CC) $(CFLAGS) $^ $(EXTFLAGS) -o $@

$(BUILDDIR)/%.o: %.c $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SRC)))

tidy:
	-rm -rf $(BUILDDIR)

dist:
ifeq ($(OS),Windows_NT)
	cp -v /bin/{cygwin1.dll,cygargp-0.dll} $(BUILDDIR)/
endif





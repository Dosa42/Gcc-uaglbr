native: uaglbr-runtime.o uaglbr-runtime32.o uaglbr-runtimex32.o

gcc.o: $(srcdir)/uaglbr-specs.h $(srcdir)/uaglbr-driver.inc

uaglbr-runtime.o: $(srcdir)/uaglbr-runtime.c
	$(CC) -m64 -O2 -fPIC -ffreestanding -fno-stack-protector -c $< -o $@

uaglbr-runtime32.o: $(srcdir)/uaglbr-runtime.c
	$(CC) -m32 -O2 -fPIC -ffreestanding -fno-stack-protector -c $< -o $@

uaglbr-runtimex32.o: $(srcdir)/uaglbr-runtime.c
	$(CC) -mx32 -O2 -fPIC -ffreestanding -fno-stack-protector -c $< -o $@

install install-strip: install-uaglbr-runtime

.PHONY: install-uaglbr-runtime
install-uaglbr-runtime: uaglbr-runtime.o uaglbr-runtime32.o uaglbr-runtimex32.o
	$(mkinstalldirs) $(DESTDIR)$(libsubdir)
	$(INSTALL_DATA) uaglbr-runtime.o $(DESTDIR)$(libsubdir)/uaglbr-runtime.o
	$(INSTALL_DATA) uaglbr-runtime32.o $(DESTDIR)$(libsubdir)/uaglbr-runtime32.o
	$(INSTALL_DATA) uaglbr-runtimex32.o $(DESTDIR)$(libsubdir)/uaglbr-runtimex32.o

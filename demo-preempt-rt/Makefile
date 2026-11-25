# Compila o módulo de carga e os programas em C do seminário
KDIR ?= /lib/modules/$(shell uname -r)/build
obj-m += carga_spin.o

all: modulo programas

modulo:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

programas: rt_periodic inversao

rt_periodic: rt_periodic.c
	gcc -O2 -Wall -o $@ $<

inversao: inversao.c
	gcc -O2 -Wall -pthread -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f rt_periodic inversao

.PHONY: all modulo programas clean

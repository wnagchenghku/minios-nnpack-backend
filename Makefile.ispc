# make -j
ISPC=ispc

default: model.a

.PHONY: dirs clean

dirs:
	/bin/mkdir -p objs/

clean:
	/bin/rm -rf objs *.o *.a


obj := $(patsubst %.ispc, %.o, $(wildcard *.ispc))

model.a: $(obj)
	ar rcs -o $@ $(obj)

%.o: %.ispc
	$(ISPC) $(ISPCFLAGS) $< -o $@

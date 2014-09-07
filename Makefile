CRIU_GIT=git://git.criu.org/criu.git

CRIU_SRC=criu

CRIU_PB_DIR=$(CRIU_SRC)/protobuf

CFLAGS		+= -iquote include
CFLAGS		+= -iquote $(CRIU_SRC)/include
CFLAGS		+= -iquote $(CRIU_PB_DIR)

BUILTINS	+= $(CRIU_PB_DIR)/built-in.o
BUILTINS	+= src/protobuf2json.o
BUILTINS	+= src/criu2json.o

LIBS		:= -lprotobuf-c -ljansson

criu2json: $(CRIU_SRC) $(BUILTINS)
	gcc $(BUILTINS) $(LIBS) -o $@

$(CRIU_SRC):
	git clone ${CRIU_GIT} ${CRIU_SRC}

$(CRIU_PB_DIR)/built-in.o:
	make -C ${CRIU_SRC} protobuf

clean:
	rm -rf criu criu2json

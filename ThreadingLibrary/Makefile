# Specify the target file
OUTPUTFILE  = mythread.a

# Default target
.PHONY: all
all: $(OUTPUTFILE)

# Build mythread.a from mythread.o
$(OUTPUTFILE): mythread.o
	ar rcs $@ $^

# No rule to build *.o from *.cc files is required
# This is handled by make's database of implicit rules
mythread.o: mythread.cc

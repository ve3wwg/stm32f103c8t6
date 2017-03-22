include Makefile.incl

all:	test1

test1:	test1.o
	$(CXX) test1.o -o test1 $(LDFLAGS)

clean:
	rm -f *.o

clobber: clean
	rm -f .errs.t test1

# End

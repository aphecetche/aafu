include $(ROOTSYS)/etc/Makefile.arch

all: myaf

CXX := $(shell root-config --cxx)

CXXFLAGS := -g -Wall $(shell root-config --cflags)

LIBS := $(shell root-config --libs) -lProof

libmyaf.so: VAF.o AFStatic.o AFDynamic.o myaf.o myafDict.o
	$(LD) $(SOFLAGS)$@ $(LDFLAGS) $^ $(OutPutOpt) $@ $(LIBS)

myaf: myaf.o libmyaf.so
	$(CXX) $(CXXFLAGS) -o myaf $^ $(LIBS)

myafDict.o: myafDict.cxx myafLinkDef.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

myafDict.cxx: VAF.h AFStatic.h AFDynamic.h myafLinkDef.h
	@echo "Generating dictionary $@.."
	rootcint -f $@ -c $^

%.o: %.cxx %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *.d *.so *.o *Dict.* myaf *.dSYM treemap.html


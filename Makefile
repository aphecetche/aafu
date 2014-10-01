include $(ROOTSYS)/etc/Makefile.arch

all: myaf webmaker

CXX := $(shell root-config --cxx)

CXXFLAGS += -g -Wall $(shell root-config --cflags)

LIBS := $(shell root-config --libs) -lProof

libmyaf.so: VAF.o AFStatic.o AFDynamic.o myaf.o myafDict.o AFWebMaker.o
ifeq ($(PLATFORM),macosx)
	$(LD) $(SOFLAGS)$@ $(LDFLAGS) $^ $(OutPutOpt) $@ $(LIBS)
else
		$(LD) $(SOFLAGS) $(LDFLAGS) $^ $(OutPutOpt) $@ $(LIBS)
endif

myaf: myaf.o libmyaf.so
	$(CXX) $(CXXFLAGS) -o myaf $^ $(LIBS)

myafDict.o: myafDict.cxx myafLinkDef.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

myafDict.cxx: VAF.h AFStatic.h AFDynamic.h myafLinkDef.h
	@echo "Generating dictionary $@.."
	rootcint -f $@ -c $^

%.o: %.cxx %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

webmaker.o: webmaker.cxx
# no root dependency in the flags here
	$(CXX) -g -Wall -c $< -o $@
    
webmaker: AFWebMaker.o webmaker.o
	$(CXX) -g $^ -o $@

clean:
	rm -rf *.d *.so *.o *Dict.* myaf *.dSYM *.html webmaker

RPMVERSION=1.0

archive:
	mkdir aafu-webmaker-$(RPMVERSION)
	cp  AFWebMaker.cxx AFWebMaker.h webmaker.cxx  *.css *.js aafu-webmaker-$(RPMVERSION)
	cp Makefile.webmaker aafu-webmaker-$(RPMVERSION)/Makefile
	tar zcvf aafu-webmaker-$(RPMVERSION).tar.gz aafu-webmaker-$(RPMVERSION)
	rm -rf aafu-webmaker-$(RPMVERSION)/
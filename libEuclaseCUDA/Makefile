
all: libEuclaseCUDA.so
	cp libEuclaseCUDA.so ../_bin

libeuclasecuda.o: libeuclasecuda.cu
	nvcc -c -O3 -g -shared -Xcompiler -fPIC $^

libEuclaseCUDA.so: libeuclasecuda.cu
	nvcc -O3 -g -shared -Xcompiler -fPIC -o $@ $^

clean:
	rm -fr *.o libEuclaseCUDA.so

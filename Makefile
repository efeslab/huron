SO_PATH = 

all: false tensor

false: false.c
	clang -Wl,./libruntime.so -finstrumenter false.c -lpthread -o false 2> build_false.log

tensor: tensor_parallel.cpp
	clang++ -Wl,./libruntime.so -finstrumenter -std=c++11 \
		-I /home/takh/eigen/ -I ./5 -DEIGEN_USE_THREADS -DMSIZE=1000 \
		tensor_parallel.cpp -lpthread -o tensor  2> build_tensor.log

clean:
	rm -f false tensor

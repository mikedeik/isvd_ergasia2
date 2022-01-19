ht:
	@echo " Compile ht_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/ht_main.c ./src/hash_file.c ./src/help_functions.c -lbf -o ./build/runner -O2
	rm -rf *.db
bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c -lbf -o ./build/runner -O2
	rm -rf *.db

new:
	@echo " Compile new_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/new_main.c ./src/hash_file.c ./src/help_functions.c -lbf -o ./build/runner -O2
	rm -rf *.db

sht:
	@echo " Compile sht_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/sht_main.c ./src/hash_file.c ./src/help_functions.c ./src/sht_file.c -lbf -o ./build/runner -O2
	rm -rf *.db

newsht:
	@echo " Compile new_sht ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/sht_new_main.c ./src/hash_file.c ./src/help_functions.c ./src/sht_file.c -lbf -o ./build/runner -O2
	rm -rf *.db
gcc -I include  main.c -o ./build/app $(pkg-config --cflags gtk+-3.0) $(pkg-config --cflags --libs libevdev) -lm $(pkg-config --libs gtk+-3.0)

cd build
sudo ./app
cd ../

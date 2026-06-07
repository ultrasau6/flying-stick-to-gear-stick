gcc -I include  main.c -o ./build/app $(pkg-config --cflags --libs libevdev) -lm

cd build
sudo ./app
cd ../
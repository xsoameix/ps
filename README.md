# Getted Started

For debian/ubuntu/mint:

    $ sudo apt-get install libgtk-3-dev
    $ cc main.c -o main `pkg-config --cflags --libs gtk+-3.0`
    $ ./main

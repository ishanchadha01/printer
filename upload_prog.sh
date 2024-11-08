echo "Compiling main file..."
arduino-cli compile -b arduino:avr:mega --build-path ./build printer.ino

echo "Uploading program to board $1"
arduino-cli upload -p $1 -b arduino:avr:mega --input-dir ./build

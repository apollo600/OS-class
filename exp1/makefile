boot.bin: boot.asm
	@nasm boot.asm -o boot.bin
	@echo "\thave compiled boot.asm."

run:
	@cp ./boot.bin ~/Programs/qemu-7.1.0/build/
	@~/Programs/qemu-7.1.0/build/qemu-system-i386 -boot order=c -drive file=boot.bin,format=raw

objects = boot.bin
clean:
	@rm $(objects)
	@echo "\t$(objects) have been deleted."

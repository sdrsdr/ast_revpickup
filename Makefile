all: app_revpickup.so
i: app_revpickup.so
	install -m 755 app_revpickup.so /usr/lib/asterisk/modules
r: i
	asterisk -rx'module unload app_revpickup.so'
	asterisk -rx'module load app_revpickup.so'
s: app_revpickup.so
	strip --strip-unneeded app_revpickup.so
clean:
	rm *.o *.so
%.o: %.c
	gcc -c -o $@ $< 
	
%.so: %.o
	gcc -shared -Xlinker -x -o $@ $< 

app_revpickup.o: app_revpickup.c
app_revpickup.so: app_revpickup.o


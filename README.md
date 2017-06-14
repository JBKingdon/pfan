# pfan
Simple user space daemon to control fan speed

This version is for the NanoPi M3 ( http://www.friendlyarm.com/index.php?route=product/product&product_id=130 )

Either run cmake . to generate the Makefile or compile with

gcc -o pfand -lm pfand.c

Needs to be run as root unless you modify permissions on the pwm sys file.

I launch it from rc.local

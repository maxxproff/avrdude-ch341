# avrdude-ch341
avrdude with ch341a programmer support (fullspeed and bitbang) for Windows WCH official driver and Linux (based on Alexey Sadkov patch and WinChipHead sources).

Very VERY VERY DIRTY coding. I'm not programmer, I'm dirty-coder in the past.
But it works!

avrdude с поддержкой программатора ch341a (fullspeed и bitbang) для официального драйвера Windows WCH и Linux (на основе патча Алексея Садкова и исходников WinChipHead).

Очень ОЧЕНЬ ОЧЕНЬ грязный код. Я не программист, я просто попробовал, и у меня получилось!

(I build with i686-w64-mingw32-gcc (GCC) 4.9.1 on raspbian-jessie)

TODO:

It is hardcoded for use CS0 (D0, pin 15 of CH341A) chip select pin as reset RST pin of AVR.

If your programmer is made for use CS1 (D1, pin 16 of CH341A), you can change code ("CH341A_CMD_UIO_STM_DIR | 0x2A" for example, etc...)

or resolder CS0 pin instead CS1 on your programmer. (AT YOUR OWN RISK, For example, Maker39 uses coupling CS0+CS1 after 100 ohm resistors

http://forum.easyelectronics.ru/viewtopic.php?p=514203&sid=a6d35d21cdeb16a76044c32e8fc11987#p514203 )

Edit DELAY_US for tune bitbang speed.

Last stage of linking may be fail (I did it several years ago. I'm too lazy to test it right now again). 

I linked with libch341dll_1_modtxt.a and some other dependencies manually.

You can edit Makefile for clean linking.

Windows fullspeed SPI (fastSPI) mode available with LibUSB-alternative-driver only, 

but for WCH-driver this mode didn't released (It isn't need for me). 

You can simply release this mode by comparing "ch341a.c" and "ch341a_bitbang_wch.c".

Захардкожено на CS0 (D0, 15-й пин CH341A) в для подключения к RST на AVR.

Если Ваш программатор распаян для использования CS1 (D1, 16-й пин CH341A), вы можете изменить код (например, «CH341A_CMD_UIO_STM_DIR | 0x2A» и т. д.)

или перепаяйте пин CS0 вместо CS1 на вашем программаторе. (НА СВОЙ РИСК, например, Maker39 использует перемычку CS0+CS1 после резисторов 100 Ом

http://forum.easyelectronics.ru/viewtopic.php?p=514203&sid=a6d35d21cdeb16a76044c32e8fc11987#p514203 )

Отредактируйте DELAY_US для настройки скорости в режиме bitbang.

На последнем этапе при линковке может вылететь с ошибкой (Давно это было, мне сейчас не досуг снова проверять). 

Я линковал вручную с libch341dll_1_modtxt.a и некоторыми другими либами.

Вы можете поправить Makefile для корректной автоматической сборки.

Под Windows режим быстрого SPI доступен только с альтернативным LibUSB драйвером.

Для драйвера WCH я такой режим не делал за ненадобностью. 

Вы можете сделать его по аналогии, опираясь на "ch341a.c" и "ch341a_bitbang_wch.c".

CREDITS:

http://www.nongnu.org/avrdude/

https://github.com/Alx2000y/avrdude_ch341a

http://savannah.nongnu.org/patch/?9127

http://forum.easyelectronics.ru/viewtopic.php?f=13&t=32626

https://yourdevice.net/forum/viewtopic.php?f=26&t=1812#p3517

http://www.wch-ic.com/downloads/category/30.html

https://disk.yandex.ru/d/oBIy7HXc3MJgSg

https://github.com/Trel725/chavrprog

http://forum.easyelectronics.ru/viewtopic.php?p=503664#p503664

http://docs.expressvl.ru/index.html

